// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
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

std::vector<FaceIndex> supportFaceIndices(const GeometryData& geometry, const SurfaceSupportFilter& filter) {
  std::vector<FaceIndex> face_indices;
  face_indices.reserve(geometry.faces.size());
  for (std::size_t face_index = 0; face_index < geometry.faces.size(); ++face_index) {
    const Face& face = geometry.faces[face_index];
    if ((face.flags & filter.ignore_flags) != 0) continue;
    ArxVector3 normal = geometry::faceNormalOr(geometry, face, {});
    if (-normal.y < filter.min_up_dot) continue;
    face_indices.push_back(static_cast<FaceIndex>(face_index));
  }
  return face_indices;
}

}  // namespace

std::optional<geometry::SurfaceSupportHit> closestMergedSupportHit(const geometry::SurfaceSupportIndex& index, float x,
                                                                   float z, float reference_y, float max_delta) {
  std::vector<geometry::SurfaceSupportHit> hits = index.hitsAt(x, z);
  geometry::mergeSurfaceSupportHits(hits);
  const geometry::SurfaceSupportHit* best = nullptr;
  float best_delta = std::numeric_limits<float>::max();
  for (const geometry::SurfaceSupportHit& hit : hits) {
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

geometry::SurfaceSupportIndex buildNavSurfaceSupportIndex(const GeometryData& geometry,
                                                          const NavSurfaceSourceOptions& options) {
  std::vector<FaceIndex> face_indices = navSurfaceSupportFaceIndices(geometry, options);
  return geometry::buildSurfaceSupportIndex(geometry, face_indices);
}

geometry::SurfaceSupportIndex buildSurfaceSupportIndex(const NavSurface& surface) {
  std::vector<geometry::SurfaceSupportTriangle> triangles;
  triangles.reserve(surface.triangles.size());
  for (const NavSurfaceTriangle& source : surface.triangles) {
    geometry::SurfaceSupportTriangle triangle;
    for (std::size_t i = 0; i < triangle.vertices.size(); ++i)
      triangle.vertices[i] = surface.vertices[source.vertices[i]].position;
    triangles.push_back(triangle);
  }
  return geometry::SurfaceSupportIndex(triangles);
}

bool hasAllowedFinalGeometrySupport(const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                    const ArxVector3& cylinder_bottom, float radius,
                                    const SurfaceSupportFilter& filter) {
  constexpr float kFinalSupportRayOriginOffset = 5.0f;
  std::optional<geometry::SurfaceSupportHit> closest;
  float closest_distance = std::numeric_limits<float>::max();
  float origin_y = cylinder_bottom.y - kFinalSupportRayOriginOffset;
  for (const ArxVector3& offset : navigationProbeOffsets(radius)) {
    std::vector<geometry::SurfaceSupportHit> hits =
        geometry_support.downwardHitsAt(cylinder_bottom.x + offset.x, cylinder_bottom.z + offset.z, origin_y);
    if (hits.empty()) continue;
    const geometry::SurfaceSupportHit& hit = hits.front();
    float distance = hit.position.y - origin_y;
    if (distance >= closest_distance) continue;
    closest = hit;
    closest_distance = distance;
  }
  if (!closest.has_value()) return false;
  return supportHitAllowed(*closest, geometry, filter);
}

}  // namespace pistoris::navigation
