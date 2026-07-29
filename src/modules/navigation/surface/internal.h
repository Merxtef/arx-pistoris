// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include "modules/navigation/internal.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace pistoris::navigation::surface {

inline constexpr double kSurfaceTriangleAreaEpsilon = 1.0e-5;

ArxVector3 navigationVertexPosition(const ArxVector3& support, float clearance);
ArxVector3 navigationSupportPosition(const ArxVector3& navigation_vertex, float clearance);

bool usableSurfaceSupport(const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                          const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                          const ArxVector3& support, const NavSurfaceGenOptions& options);

bool validateSupportPoint(const geometry::SurfaceSupportIndex& index,
                          const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                          const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                          const NavSurfaceGenOptions& options, float x, float z, float reference_y);

std::optional<NavSurfaceTriangle> makeNavigationTriangle(const NavSurface& surface, std::uint32_t a, std::uint32_t b,
                                                         std::uint32_t c);

bool appendNavigationTriangle(NavSurface& surface, std::uint32_t a, std::uint32_t b, std::uint32_t c);

void buildSurfaceLattice(const ArxAabb& support_bounds, const geometry::SurfaceSupportIndex& support_index,
                         const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                         const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                         const NavSurfaceGenOptions& options, NavSurface& surface);

std::size_t repairNavSurfaceEdges(const geometry::SurfaceSupportIndex& index,
                                  const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                  const SurfaceSupportFilter& final_support_filter,
                                  const StaticAnchorTraversal& traversal, const NavSurfaceGenOptions& options,
                                  NavSurface& surface);

}  // namespace pistoris::navigation::surface
