// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"

#include "modules/geometry.h"
#include "modules/navigation/collision/internal.h"
#include "utils/math/geometry.h"
#include "utils/spatial/arx_level_grid.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <vector>

namespace pistoris::navigation::collision {
namespace {

ArxVector3 normalizeOrZero(const ArxVector3& value) {
  float len = static_cast<float>(math::length(value));
  if (len <= std::numeric_limits<float>::epsilon()) return {};
  return value * (1.0f / len);
}

bool ignoredForStaticTraversal(FaceType flags) {
  return (flags & (kFaceBitWater | kFaceBitTrans | kFaceBitNocol)) != 0;
}

TraversalFace makeTraversalFace(const GeometryData& geometry, const Face& face) {
  TraversalFace out;
  out.flags = face.flags;
  for (std::size_t i = 0; i < out.vertices.size(); ++i)
    out.vertices[i] = geometry.vertices[face.corners[i].vertex].position;
  out.center = (out.vertices[0] + out.vertices[1] + out.vertices[2]) * (1.0f / 3.0f);
  out.normal = normalizeOrZero(math::cross(out.vertices[1] - out.vertices[0], out.vertices[2] - out.vertices[0]));
  out.min = out.vertices[0];
  out.max = out.vertices[0];
  for (const ArxVector3& vertex : out.vertices) {
    out.min.x = std::min(out.min.x, vertex.x);
    out.min.y = std::min(out.min.y, vertex.y);
    out.min.z = std::min(out.min.z, vertex.z);
    out.max.x = std::max(out.max.x, vertex.x);
    out.max.y = std::max(out.max.y, vertex.y);
    out.max.z = std::max(out.max.z, vertex.z);
  }
  out.area = math::triangleArea(out.vertices[0], out.vertices[1], out.vertices[2]);
  return out;
}

}  // namespace

StaticCollisionIndex::StaticCollisionIndex(const GeometryData& geometry) {
  faces_.reserve(geometry.faces.size());
  for (const Face& face : geometry.faces) {
    if (ignoredForStaticTraversal(face.flags)) continue;
    TraversalFace traversal_face = makeTraversalFace(geometry, face);
    std::uint32_t face_index = static_cast<std::uint32_t>(faces_.size());
    faces_.push_back(traversal_face);
    std::optional<spatial::ArxLevelGrid::Coord> min_x = spatial::ArxLevelGrid::cellCoord(traversal_face.min.x);
    std::optional<spatial::ArxLevelGrid::Coord> max_x = spatial::ArxLevelGrid::cellCoord(traversal_face.max.x);
    std::optional<spatial::ArxLevelGrid::Coord> min_z = spatial::ArxLevelGrid::cellCoord(traversal_face.min.z);
    std::optional<spatial::ArxLevelGrid::Coord> max_z = spatial::ArxLevelGrid::cellCoord(traversal_face.max.z);
    if (!min_x || !max_x || !min_z || !max_z) continue;
    for (std::uint16_t x = *min_x; x <= *max_x; ++x)
      for (std::uint16_t z = *min_z; z <= *max_z; ++z)
        buckets_[spatial::ArxLevelGrid::cellKey(static_cast<spatial::ArxLevelGrid::Coord>(x),
                                                static_cast<spatial::ArxLevelGrid::Coord>(z))]
            .push_back(face_index);
  }
}

const TraversalFace& StaticCollisionIndex::face(std::uint32_t index) const { return faces_[index]; }

std::vector<std::uint32_t> StaticCollisionIndex::candidates(const Cylinder& cylinder) const {
  float radius = broadphaseRadius(cylinder);
  std::optional<spatial::ArxLevelGrid::Coord> min_x = spatial::ArxLevelGrid::cellCoord(cylinder.origin.x - radius);
  std::optional<spatial::ArxLevelGrid::Coord> max_x = spatial::ArxLevelGrid::cellCoord(cylinder.origin.x + radius);
  std::optional<spatial::ArxLevelGrid::Coord> min_z = spatial::ArxLevelGrid::cellCoord(cylinder.origin.z - radius);
  std::optional<spatial::ArxLevelGrid::Coord> max_z = spatial::ArxLevelGrid::cellCoord(cylinder.origin.z + radius);

  std::vector<std::uint32_t> out;
  if (!min_x || !max_x || !min_z || !max_z) return out;
  const std::uint32_t cell_count =
      (static_cast<std::uint32_t>(*max_x) - *min_x + 1U) * (static_cast<std::uint32_t>(*max_z) - *min_z + 1U);
  if (cell_count > faces_.size()) {
    out.resize(faces_.size());
    std::iota(out.begin(), out.end(), 0U);
    return out;
  }
  for (std::uint16_t x = *min_x; x <= *max_x; ++x) {
    for (std::uint16_t z = *min_z; z <= *max_z; ++z) {
      auto bucket = buckets_.find(spatial::ArxLevelGrid::cellKey(static_cast<spatial::ArxLevelGrid::Coord>(x),
                                                                 static_cast<spatial::ArxLevelGrid::Coord>(z)));
      if (bucket == buckets_.end()) continue;
      out.insert(out.end(), bucket->second.begin(), bucket->second.end());
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

}  // namespace pistoris::navigation::collision
