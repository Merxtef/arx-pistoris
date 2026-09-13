// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace pistoris::geometry {
constexpr float kNormalLengthEpsilon = 1.0e-4f;

Error validateVertex(const Vertex& vertex) noexcept {
  return math::finite(vertex.position) ? Error::kNone : Error::kBadVertex;
}

Error validateCounts(std::size_t vertex_count, std::size_t face_count) noexcept {
  if (vertex_count > static_cast<std::size_t>(kInvalidVertexIndex)) return Error::kTooManyVertices;
  if (face_count > static_cast<std::size_t>(kInvalidFaceIndex)) return Error::kTooManyFaces;
  return Error::kNone;
}

Error validateVertexAppend(const GeometryData& geometry, std::size_t count) noexcept {
  const std::size_t limit = static_cast<std::size_t>(kInvalidVertexIndex);
  if (geometry.vertices.size() > limit || count > limit - geometry.vertices.size()) return Error::kTooManyVertices;
  return Error::kNone;
}

Error validateVertices(std::span<const Vertex> vertices, ArxAabb* out_bounds) noexcept {
  if (vertices.empty()) {
    log(ARX_LOG_DEBUG, "Geometry validation: no vertices");
    return Error::kNoGeometry;
  }
  const Error count_error = validateCounts(vertices.size(), 0);
  if (count_error != Error::kNone) {
    log(ARX_LOG_DEBUG,
        "Geometry validation: vertex count {} exceeds limit {}",
        vertices.size(),
        static_cast<std::size_t>(kInvalidVertexIndex));
    return count_error;
  }

  ArxAabb bounds{};
  bool has_bounds = false;
  for (std::size_t index = 0; index < vertices.size(); ++index) {
    const Vertex& vertex = vertices[index];
    Error error = validateVertex(vertex);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Geometry validation: vertex {} has invalid position ({}, {}, {})",
          index,
          vertex.position.x,
          vertex.position.y,
          vertex.position.z);
      return error;
    }
    if (!has_bounds) {
      bounds.min = vertex.position;
      bounds.max = vertex.position;
      has_bounds = true;
    } else {
      math::expand(bounds, vertex.position);
    }
  }

  if (!has_bounds) return Error::kNoGeometry;
  if (out_bounds) *out_bounds = bounds;
  return Error::kNone;
}

Error validateFaceReferences(const Face& face, std::size_t vertex_count, std::size_t texture_count) noexcept {
  if (face.texture != kNoTexture && static_cast<std::size_t>(face.texture) >= texture_count)
    return Error::kBadFaceTexture;
  for (const Corner& corner : face.corners) {
    if (corner.vertex >= vertex_count) return Error::kBadFaceVertex;
  }
  return Error::kNone;
}

Error validateFaces(std::span<const Face> faces, std::span<const Vertex> vertices, std::size_t texture_count,
                    ArxAabb* out_referenced_bounds) noexcept {
  if (faces.empty()) {
    log(ARX_LOG_DEBUG, "Geometry validation: no faces");
    return Error::kNoGeometry;
  }
  const Error count_error = validateCounts(0, faces.size());
  if (count_error != Error::kNone) {
    log(ARX_LOG_DEBUG,
        "Geometry validation: face count {} exceeds limit {}",
        faces.size(),
        static_cast<std::size_t>(kInvalidFaceIndex));
    return count_error;
  }

  ArxAabb referenced_bounds{};
  bool has_referenced_bounds = false;
  for (std::size_t face_index = 0; face_index < faces.size(); ++face_index) {
    const Face& face = faces[face_index];
    Error error = validateFaceReferences(face, vertices.size(), texture_count);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Geometry validation: face {} has invalid references: texture {}, vertices [{}, {}, {}], vertex count {}, "
          "texture count {}, error {}",
          face_index,
          face.texture,
          face.corners[0].vertex,
          face.corners[1].vertex,
          face.corners[2].vertex,
          vertices.size(),
          texture_count,
          static_cast<int>(error));
      return error;
    }
    if ((face.flags & kFaceBitQuad) != 0 || (face.flags & kFaceBitsAll) != face.flags) {
      log(ARX_LOG_DEBUG, "Geometry validation: face {} has invalid flags {:#x}", face_index, face.flags);
      return Error::kBadFaceType;
    }
    if (!math::finite(face.transval)) {
      log(ARX_LOG_DEBUG, "Geometry validation: face {} has invalid transval {}", face_index, face.transval);
      return Error::kBadFaceTransval;
    }
    if (!math::finite(face.normal)) {
      log(ARX_LOG_DEBUG,
          "Geometry validation: face {} has invalid normal ({}, {}, {})",
          face_index,
          face.normal.x,
          face.normal.y,
          face.normal.z);
      return Error::kBadFaceNormal;
    }

    if (face.corners[0].vertex == face.corners[1].vertex || face.corners[0].vertex == face.corners[2].vertex ||
        face.corners[1].vertex == face.corners[2].vertex) {
      log(ARX_LOG_DEBUG,
          "Geometry validation: face {} repeats a vertex in [{}, {}, {}]",
          face_index,
          face.corners[0].vertex,
          face.corners[1].vertex,
          face.corners[2].vertex);
      return Error::kBadFaceVertex;
    }

    std::array<ArxVector3, 3> positions{};
    for (std::size_t i = 0; i < face.corners.size(); ++i) {
      const Corner& corner = face.corners[i];
      if (!math::finite(corner.normal)) {
        log(ARX_LOG_DEBUG,
            "Geometry validation: face {} corner {} has invalid normal ({}, {}, {})",
            face_index,
            i,
            corner.normal.x,
            corner.normal.y,
            corner.normal.z);
        return Error::kBadCornerNormal;
      }
      const double normal_length = math::length(corner.normal);
      if (normal_length <= kNormalLengthEpsilon || std::abs(normal_length - 1.0) > kNormalLengthEpsilon) {
        log(ARX_LOG_DEBUG,
            "Geometry validation: face {} corner {} normal length {} is outside 1 +/- {}",
            face_index,
            i,
            normal_length,
            kNormalLengthEpsilon);
        return Error::kBadCornerNormal;
      }
      if (!math::finite(corner.u) || !math::finite(corner.v)) {
        log(ARX_LOG_DEBUG,
            "Geometry validation: face {} corner {} has invalid UV ({}, {})",
            face_index,
            i,
            corner.u,
            corner.v);
        return Error::kBadFaceUv;
      }

      positions[i] = vertices[corner.vertex].position;
      if (!math::finite(positions[i])) {
        log(ARX_LOG_DEBUG,
            "Geometry validation: face {} corner {} references vertex {} with invalid position",
            face_index,
            i,
            corner.vertex);
        return Error::kBadVertex;
      }
      if (!has_referenced_bounds) {
        referenced_bounds.min = positions[i];
        referenced_bounds.max = positions[i];
        has_referenced_bounds = true;
      } else {
        math::expand(referenced_bounds, positions[i]);
      }
    }

    if (degenerateTriangle(positions[0], positions[1], positions[2])) {
      log(ARX_LOG_DEBUG, "Geometry validation: face {} is degenerate", face_index);
      return Error::kDegenerateFace;
    }
  }

  if (!has_referenced_bounds) return Error::kNoGeometry;
  if (out_referenced_bounds) *out_referenced_bounds = referenced_bounds;
  return Error::kNone;
}

Error validate(const GeometryData& geometry, std::size_t texture_count, GeometryDerived* out) {
  ArxAabb bounds{};
  Error error = validateVertices(geometry.vertices, &bounds);
  if (error != Error::kNone) return error;

  ArxAabb referenced_bounds{};
  error = validateFaces(geometry.faces, geometry.vertices, texture_count, &referenced_bounds);
  if (error != Error::kNone) return error;

  if (out) *out = {bounds, referenced_bounds};
  return Error::kNone;
}

}  // namespace pistoris::geometry
