// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/geometry.h"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::geometry {

std::span<const FaceIndex> VertexFaceIndex::incidentFaces(VertexIndex vertex) const noexcept {
  if (static_cast<std::size_t>(vertex) + 1U >= offsets_.size()) return {};
  return std::span<const FaceIndex>(faces_).subspan(offsets_[vertex], offsets_[vertex + 1U] - offsets_[vertex]);
}

Error buildVertexFaceIndex(const GeometryData& geometry, VertexFaceIndex& out) {
  if (geometry.vertices.size() > static_cast<std::size_t>(kInvalidVertexIndex)) return Error::kTooManyVertices;
  if (geometry.faces.size() > static_cast<std::size_t>(kInvalidFaceIndex)) return Error::kTooManyFaces;

  VertexFaceIndex built;
  built.offsets_.assign(geometry.vertices.size() + 1U, 0);
  for (const Face& face : geometry.faces) {
    for (const Corner& corner : face.corners) {
      if (corner.vertex >= geometry.vertices.size()) return Error::kBadFaceVertex;
      ++built.offsets_[static_cast<std::size_t>(corner.vertex) + 1U];
    }
  }
  for (std::size_t vertex = 1; vertex < built.offsets_.size(); ++vertex)
    built.offsets_[vertex] += built.offsets_[vertex - 1U];

  built.faces_.resize(built.offsets_.back());
  std::vector<std::size_t> write_offsets = built.offsets_;
  for (std::size_t face = 0; face < geometry.faces.size(); ++face) {
    for (const Corner& corner : geometry.faces[face].corners)
      built.faces_[write_offsets[corner.vertex]++] = static_cast<FaceIndex>(face);
  }
  out = std::move(built);
  return Error::kNone;
}

}  // namespace pistoris::geometry
