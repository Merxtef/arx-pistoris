// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

bool supportHitAllowed(const geometry::SurfaceSupportHit& hit, const GeometryData& geometry,
                       const SurfaceSupportFilter& filter) {
  if (hit.face >= geometry.faces.size()) return false;
  const Face& face = geometry.faces[hit.face];
  if ((face.flags & filter.ignore_flags) != 0) return false;
  return -hit.normal.y >= filter.min_up_dot;
}

bool supportFaceAllowed(const Face& face, const GeometryData& geometry, const SurfaceSupportFilter& filter) {
  if ((face.flags & filter.ignore_flags) != 0) return false;
  const ArxVector3 normal = geometry::faceNormalOr(geometry, face, {});
  return -normal.y >= filter.min_up_dot;
}

std::vector<FaceIndex> supportFaceIndices(const GeometryData& geometry, const SurfaceSupportFilter& filter) {
  std::vector<FaceIndex> face_indices;
  face_indices.reserve(geometry.faces.size());
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const Face& face = geometry.faces[face_index];
    if (!supportFaceAllowed(face, geometry, filter)) continue;
    face_indices.push_back(static_cast<FaceIndex>(face_index));
  }
  return face_indices;
}

}  // namespace

std::optional<geometry::SurfaceSupportHit> closestMergedSupportHit(const geometry::SurfaceSupportIndex& index, float x,
                                                                   float z, float reference_y, float max_delta,
                                                                   std::vector<geometry::SurfaceSupportHit>& scratch) {
  index.findHitsAt(scratch, x, z);
  geometry::mergeSortedSurfaceSupportHits(scratch);
  const geometry::SurfaceSupportHit* best = nullptr;
  float best_delta = std::numeric_limits<float>::max();
  for (const geometry::SurfaceSupportHit& hit : scratch) {
    float delta = std::abs(hit.position.y - reference_y);
    if (delta >= best_delta) continue;
    best = &hit;
    best_delta = delta;
  }
  if (!best || best_delta > max_delta) return std::nullopt;
  return *best;
}

SurfaceSupportFilter navSurfaceSupportFilter(const NavSurfaceSourceOptions& options) {
  SurfaceSupportFilter filter;
  filter.ignore_flags = options.support_ignore_flags | kFaceBitNopath;
  filter.min_up_dot = options.support_min_up_cos;
  return filter;
}

std::vector<FaceIndex> navSurfaceSupportFaceIndices(const GeometryData& geometry,
                                                    const NavSurfaceSourceOptions& options) {
  return supportFaceIndices(geometry, navSurfaceSupportFilter(options));
}

NavSurfaceSupportIndexes buildNavSurfaceSupportIndexes(const GeometryData& geometry,
                                                       const NavSurfaceSourceOptions& options) {
  const SurfaceSupportFilter filter = navSurfaceSupportFilter(options);
  geometry::SurfaceSupportIndexBuilder geometry_builder(geometry.faces.size());
  std::vector<std::uint32_t> support_indices;
  support_indices.reserve(geometry.faces.size());
  for (std::size_t index = 0; index < geometry.faces.size(); ++index) {
    const FaceIndex face_index = static_cast<FaceIndex>(index);
    const Face& face = geometry.faces[index];
    const std::array<ArxVector3, 3> vertices = geometry::facePositions(geometry, face);
    geometry_builder.addTriangle(face_index, vertices);
    if (supportFaceAllowed(face, geometry, filter)) support_indices.push_back(static_cast<std::uint32_t>(index));
  }
  geometry::SurfaceSupportIndex geometry_support = std::move(geometry_builder).build();
  geometry::SurfaceSupportIndex support = geometry_support.subset(support_indices);
  return {std::move(support), std::move(geometry_support)};
}

geometry::SurfaceSupportIndex buildSurfaceSupportIndex(const NavSurface& surface) {
  geometry::SurfaceSupportIndexBuilder builder(surface.triangles.size());
  for (const NavSurfaceTriangle& triangle : surface.triangles) {
    std::array<ArxVector3, 3> vertices;
    for (std::size_t i = 0; i < vertices.size(); ++i) vertices[i] = surface.vertices[triangle.vertices[i]].position;
    builder.addTriangle(kInvalidFaceIndex, vertices);
  }
  return std::move(builder).build();
}

bool hasAllowedFinalGeometrySupport(const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                    const ArxVector3& cylinder_bottom, float radius,
                                    const SurfaceSupportFilter& filter) {
  constexpr float kFinalSupportRayOriginOffset = 5.0f;
  std::optional<geometry::SurfaceSupportHit> closest;
  float closest_distance = std::numeric_limits<float>::max();
  float origin_y = cylinder_bottom.y - kFinalSupportRayOriginOffset;
  for (const ArxVector3& offset : navigationProbeOffsets(radius)) {
    std::optional<geometry::SurfaceSupportHit> hit =
        geometry_support.closestDownwardHit(cylinder_bottom.x + offset.x, cylinder_bottom.z + offset.z, origin_y);
    if (!hit) continue;
    float distance = hit->position.y - origin_y;
    if (distance >= closest_distance) continue;
    closest = *hit;
    closest_distance = distance;
  }
  if (!closest.has_value()) return false;
  return supportHitAllowed(*closest, geometry, filter);
}

}  // namespace pistoris::navigation
