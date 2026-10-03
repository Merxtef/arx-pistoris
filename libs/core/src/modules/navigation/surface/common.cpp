// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/surface/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/math/geometry_algorithms.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace pistoris::navigation::surface {

ArxVector3 navigationVertexPosition(const ArxVector3& support, float clearance) {
  return {support.x, support.y - clearance, support.z};
}

ArxVector3 navigationSupportPosition(const ArxVector3& navigation_vertex, float clearance) {
  return {navigation_vertex.x, navigation_vertex.y + clearance, navigation_vertex.z};
}

bool usableSurfaceSupport(const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                          const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                          const ArxVector3& support, const NavSurfaceGenerationOptions& options) {
  ArxVector3 placed{};
  if (traversal.placeEndpointAt(support, options.radius, options.height, placed) != CylinderPlacementStatus::kPlaced)
    return false;
  return hasAllowedFinalGeometrySupport(geometry_support, geometry, placed, options.radius, final_support_filter);
}

bool validateSupportPoint(const geometry::SurfaceSupportIndex& index,
                          const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                          const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                          const NavSurfaceGenerationOptions& options, float x, float z, float reference_y,
                          std::vector<geometry::SurfaceSupportHit>& scratch) {
  std::optional<geometry::SurfaceSupportHit> hit =
      closestMergedSupportHit(index, x, z, reference_y, options.max_step_up * 0.5f, scratch);
  return hit.has_value() &&
         usableSurfaceSupport(geometry_support, geometry, final_support_filter, traversal, hit->position, options);
}

std::optional<NavSurfaceTriangle> makeNavigationTriangle(const NavSurface& surface, std::uint32_t a, std::uint32_t b,
                                                         std::uint32_t c) {
  if (a == b || a == c || b == c) return std::nullopt;
  const ArxVector3& pa = surface.vertices[a].position;
  const ArxVector3& pb = surface.vertices[b].position;
  const ArxVector3& pc = surface.vertices[c].position;
  double area = math::projectedAreaXz2(pa, pb, pc);
  if (std::abs(area) <= kSurfaceTriangleAreaEpsilon) return std::nullopt;
  if (area < 0.0) std::swap(b, c);
  return NavSurfaceTriangle{{a, b, c}};
}

bool tryAppendNavigationTriangle(NavSurface& surface, std::uint32_t a, std::uint32_t b, std::uint32_t c) {
  std::optional<NavSurfaceTriangle> triangle = makeNavigationTriangle(surface, a, b, c);
  if (!triangle.has_value()) return false;
  surface.triangles.push_back(*triangle);
  return true;
}

}  // namespace pistoris::navigation::surface
