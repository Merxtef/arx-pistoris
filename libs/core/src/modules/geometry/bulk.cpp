// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "utils/math/finite.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {

bool normalizeNormal(ArxVector3& normal) noexcept {
  if (!math::finite(normal)) return false;
  const double length =
      std::hypot(static_cast<double>(normal.x), static_cast<double>(normal.y), static_cast<double>(normal.z));
  if (length <= 1.0e-4) return false;
  normal = {static_cast<float>(normal.x / length),
            static_cast<float>(normal.y / length),
            static_cast<float>(normal.z / length)};
  return true;
}

Error buildVertices(std::span<const float> positions, std::vector<Vertex>& out) {
  if (positions.size() % 3 != 0) return Error::kBadVertexCount;
  const std::size_t count = positions.size() / 3;
  const Error counts = validateCounts(count, 0);
  if (counts != Error::kNone) return counts;
  try {
    std::vector<Vertex> replacement(count);
    for (std::size_t i = 0; i < count; ++i) {
      replacement[i].position = {positions[3 * i], positions[3 * i + 1], positions[3 * i + 2]};
      if (validateVertex(replacement[i]) != Error::kNone) return Error::kBadVertex;
    }
    out = std::move(replacement);
    return Error::kNone;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

Error buildFaces(std::span<const Vertex> vertices, std::size_t texture_count,
                 std::span<const std::uint32_t> vertex_indices, std::span<const float> uvs,
                 std::span<const float> corner_normals, std::span<const TextureIndex> textures,
                 std::span<const float> transvals, std::span<const float> face_normals, std::span<const FaceType> flags,
                 std::vector<Face>& out) {
  if (vertex_indices.size() % 3 != 0) return Error::kBadFaceCount;
  const std::size_t count = vertex_indices.size() / 3;
  const Error counts = validateCounts(0, count);
  if (counts != Error::kNone) return counts;
  if (count > std::numeric_limits<std::size_t>::max() / 9U) return Error::kTooManyFaces;
  if (uvs.size() != count * 6 || corner_normals.size() != count * 9 || textures.size() != count ||
      transvals.size() != count || (!face_normals.empty() && face_normals.size() != count * 3) ||
      (!flags.empty() && flags.size() != count))
    return Error::kBadFaceCount;
  try {
    std::vector<Face> replacement(count);
    for (std::size_t i = 0; i < count; ++i) {
      Face& face = replacement[i];
      face.texture = textures[i];
      face.transval = transvals[i];
      face.flags = flags.empty() ? 0 : flags[i];
      for (std::size_t c = 0; c < 3; ++c) {
        const std::size_t corner = 3 * i + c;
        face.corners[c] = {vertex_indices[corner],
                           {corner_normals[corner * 3], corner_normals[corner * 3 + 1], corner_normals[corner * 3 + 2]},
                           uvs[corner * 2],
                           uvs[corner * 2 + 1]};
        if (!normalizeNormal(face.corners[c].normal)) return Error::kBadCornerNormal;
      }
      const Error refs = validateFaceReferences(face, vertices.size(), texture_count);
      if (refs != Error::kNone) return refs;
      if (face.corners[0].vertex == face.corners[1].vertex || face.corners[0].vertex == face.corners[2].vertex ||
          face.corners[1].vertex == face.corners[2].vertex)
        return Error::kBadFaceVertex;
      for (const Corner& corner : face.corners)
        if (validateVertex(vertices[corner.vertex]) != Error::kNone) return Error::kBadVertex;
      if (face_normals.empty()) {
        const auto& a = vertices[face.corners[0].vertex].position;
        const auto& b = vertices[face.corners[1].vertex].position;
        const auto& c = vertices[face.corners[2].vertex].position;
        face.normal = triangleNormalOr(a, b, c, {});
        if (face.normal == ArxVector3{}) return Error::kDegenerateFace;
      } else {
        face.normal = {face_normals[3 * i], face_normals[3 * i + 1], face_normals[3 * i + 2]};
        if (!normalizeNormal(face.normal)) return Error::kBadFaceNormal;
      }
    }
    if (!replacement.empty()) {
      const Error error = validateFaces(replacement, vertices, texture_count);
      if (error != Error::kNone) return error;
    }
    out = std::move(replacement);
    return Error::kNone;
  } catch (const std::bad_alloc&) {
    return Error::kOutOfMemory;
  }
}

}  // namespace pistoris::geometry
