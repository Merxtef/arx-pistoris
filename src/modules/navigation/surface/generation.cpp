// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/surface/internal.h"
#include "modules/navigation/traversal.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

SurfaceDebugTriangle debugTriangle(const NavSurface& surface, const NavSurfaceTriangle& triangle) {
  return {{
      surface.vertices[triangle.vertices[0]].position,
      surface.vertices[triangle.vertices[1]].position,
      surface.vertices[triangle.vertices[2]].position,
  }};
}

void appendSurfaceDiagnostics(const NavSurface& surface, std::size_t base_count,
                              NavSurfaceGenerationDiagnostics& diagnostics) {
  for (std::size_t i = 0; i < surface.triangles.size(); ++i) {
    if (i < base_count) {
      diagnostics.base.push_back(debugTriangle(surface, surface.triangles[i]));
    } else {
      diagnostics.repaired.push_back(debugTriangle(surface, surface.triangles[i]));
    }
  }
}

}  // namespace

Error buildNavSurface(NavSurface& out, const GeometryData& geometry, const ArxAabb& support_bounds,
                      const geometry::SurfaceSupportIndex& support_index,
                      const geometry::SurfaceSupportIndex& geometry_support,
                      const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                      const NavSurfaceGenerationOptions& options, NavSurfaceGenerationDiagnostics* diagnostics) {
  NavSurface surface;
  surface::buildSurfaceLattice(
      support_bounds, support_index, geometry_support, geometry, final_support_filter, traversal, options, surface);
  std::size_t base_count = surface.triangles.size();
  surface::repairNavSurfaceEdges(
      support_index, geometry_support, geometry, final_support_filter, traversal, options, surface);
  if (diagnostics) appendSurfaceDiagnostics(surface, base_count, *diagnostics);
  if (surface.triangles.empty()) return Error::kEmptyResult;

  out = std::move(surface);
  return Error::kNone;
}

}  // namespace pistoris::navigation
