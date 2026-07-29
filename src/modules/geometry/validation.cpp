// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>

namespace pistoris::geometry {
namespace {

constexpr float kNormalLengthEpsilon = 1.0e-4f;

}  // namespace

Error validateVertex(const Vertex& vertex) noexcept {
  return math::finite(vertex.position) ? Error::kNone : Error::kBadVertex;
}

Error validateVertices(std::span<const Vertex> vertices, ArxAabb* out_bounds) noexcept {
  if (vertices.empty()) return Error::kNoGeometry;
  if (vertices.size() > static_cast<std::size_t>(kInvalidVertexIndex)) return Error::kTooManyVertices;

  ArxAabb bounds{};
  bool has_bounds = false;
  for (const Vertex& vertex : vertices) {
    Error error = validateVertex(vertex);
    if (error != Error::kNone) return error;
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

Error validateTexture(const Texture& texture) noexcept {
  if (texture.path.empty() || !validSemanticString(texture.path)) return Error::kBadTexture;
  if (!texture.encoded_image.empty()) {
    ImageError error = inspectImage(texture.encoded_image);
    if (error == ImageError::kOutOfMemory) return Error::kOutOfMemory;
    if (error != ImageError::kNone) return Error::kBadTextureImage;
  }
  return Error::kNone;
}

Error validateTextures(std::span<const Texture> textures) noexcept {
  if (textures.size() > static_cast<std::size_t>(kNoTexture)) return Error::kTooManyTextures;
  for (const Texture& texture : textures) {
    Error error = validateTexture(texture);
    if (error != Error::kNone) return error;
  }
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
  if (faces.empty()) return Error::kNoGeometry;
  if (faces.size() > static_cast<std::size_t>(kInvalidFaceIndex)) return Error::kTooManyFaces;

  ArxAabb referenced_bounds{};
  bool has_referenced_bounds = false;
  for (const Face& face : faces) {
    Error error = validateFaceReferences(face, vertices.size(), texture_count);
    if (error != Error::kNone) return error;
    if ((face.flags & kFaceBitsAll) != face.flags) return Error::kBadFaceType;
    if (!math::finite(face.transval)) return Error::kBadFaceTransval;

    if (face.corners[0].vertex == face.corners[1].vertex || face.corners[0].vertex == face.corners[2].vertex ||
        face.corners[1].vertex == face.corners[2].vertex)
      return Error::kBadFaceVertex;

    std::array<ArxVector3, 3> positions{};
    for (std::size_t i = 0; i < face.corners.size(); ++i) {
      const Corner& corner = face.corners[i];
      if (!math::finite(corner.normal)) return Error::kBadFaceNormal;
      if (!math::finite(corner.u) || !math::finite(corner.v)) return Error::kBadFaceUv;

      double normal_length = math::length(corner.normal);
      if (normal_length <= kNormalLengthEpsilon || std::abs(normal_length - 1.0) > kNormalLengthEpsilon)
        return Error::kBadFaceNormal;

      positions[i] = vertices[corner.vertex].position;
      if (!math::finite(positions[i])) return Error::kBadVertex;
      if (!has_referenced_bounds) {
        referenced_bounds.min = positions[i];
        referenced_bounds.max = positions[i];
        has_referenced_bounds = true;
      } else {
        math::expand(referenced_bounds, positions[i]);
      }
    }

    if (degenerateTriangle(positions[0], positions[1], positions[2])) return Error::kDegenerateFace;
  }

  if (!has_referenced_bounds) return Error::kNoGeometry;
  if (out_referenced_bounds) *out_referenced_bounds = referenced_bounds;
  return Error::kNone;
}

Error validate(const GeometryData& geometry, GeometryDerived* out) {
  ArxAabb bounds{};
  Error error = validateVertices(geometry.vertices, &bounds);
  if (error != Error::kNone) return error;

  error = validateTextures(geometry.textures);
  if (error != Error::kNone) return error;

  ArxAabb referenced_bounds{};
  error = validateFaces(geometry.faces, geometry.vertices, geometry.textures.size(), &referenced_bounds);
  if (error != Error::kNone) return error;

  if (out) *out = {bounds, referenced_bounds};
  return Error::kNone;
}

}  // namespace pistoris::geometry
