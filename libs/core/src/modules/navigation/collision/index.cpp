// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/navigation/collision/internal.h"
#include "utils/math/geometry_algorithms.h"
#include "utils/spatial/arx_level_grid_index.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pistoris::navigation::collision {
namespace {

bool ignoredForStaticTraversal(FaceType flags) {
  return (flags & (kFaceBitWater | kFaceBitTrans | kFaceBitNocol)) != 0;
}

TraversalFace makeTraversalFace(const GeometryData& geometry, const Face& face) {
  TraversalFace out;
  out.flags = face.flags;
  for (std::size_t i = 0; i < out.vertices.size(); ++i)
    out.vertices[i] = geometry.vertices[face.corners[i].vertex].position;
  out.center = (out.vertices[0] + out.vertices[1] + out.vertices[2]) * (1.0f / 3.0f);
  out.normal =
      math::normalizeFiniteOr(math::cross(out.vertices[1] - out.vertices[0], out.vertices[2] - out.vertices[0]),
                              {},
                              std::numeric_limits<float>::epsilon());
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
    grid_index_.add(face_index, {traversal_face.min, traversal_face.max});
  }
}

const TraversalFace& StaticCollisionIndex::face(std::uint32_t index) const { return faces_[index]; }

void StaticCollisionIndex::findCandidates(std::vector<std::uint32_t>& out, const Cylinder& cylinder) const {
  out.clear();
  float radius = broadphaseRadius(cylinder);
  ArxAabb query_bounds;
  query_bounds.min = {cylinder.origin.x - radius, cylinder.origin.y + cylinder.height, cylinder.origin.z - radius};
  query_bounds.max = {cylinder.origin.x + radius, cylinder.origin.y, cylinder.origin.z + radius};
  if (query_bounds.min.y > query_bounds.max.y) std::swap(query_bounds.min.y, query_bounds.max.y);
  grid_index_.findAabbCandidates(out, query_bounds);
  out.erase(std::remove_if(out.begin(),
                           out.end(),
                           [&](std::uint32_t face_index) {
                             const TraversalFace& candidate = faces_[face_index];
                             return candidate.max.x < query_bounds.min.x || candidate.min.x > query_bounds.max.x ||
                                    candidate.max.z < query_bounds.min.z || candidate.min.z > query_bounds.max.z;
                           }),
            out.end());
}

}  // namespace pistoris::navigation::collision
