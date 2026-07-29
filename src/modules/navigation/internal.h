// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/navigation.h"
#include "modules/navigation/traversal.h"

namespace pistoris::navigation {

std::vector<ArxVector3> navigationProbeOffsets(float radius);
bool validNavSurfaceSourceOptions(const NavSurfaceSourceOptions& options) noexcept;

std::vector<FaceIndex> navSurfaceSupportFaceIndices(const GeometryData& geometry,
                                                    const NavSurfaceSourceOptions& options);
SurfaceSupportFilter navSurfaceSupportFilter(const NavSurfaceSourceOptions& options);
geometry::SurfaceSupportIndex buildNavSurfaceSupportIndex(const GeometryData& geometry,
                                                          const NavSurfaceSourceOptions& options);
geometry::SurfaceSupportIndex buildSurfaceSupportIndex(const NavSurface& surface);
std::optional<geometry::SurfaceSupportHit> closestMergedSupportHit(const geometry::SurfaceSupportIndex& index, float x,
                                                                   float z, float reference_y, float max_delta);

bool hasAllowedFinalGeometrySupport(const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                    const ArxVector3& cylinder_bottom, float radius,
                                    const SurfaceSupportFilter& filter);

Error buildNavSurface(NavSurface& out, const GeometryData& geometry, const ArxAabb& support_bounds,
                      const geometry::SurfaceSupportIndex& support_index,
                      const geometry::SurfaceSupportIndex& geometry_support,
                      const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                      const NavSurfaceGenOptions& options, NavSurfaceGenDiagnostics* diagnostics = nullptr);

Error buildNavigationAnchors(std::vector<Anchor>& out, const GeometryData& geometry,
                             const geometry::SurfaceSupportIndex& support_index,
                             const geometry::SurfaceSupportIndex& geometry_support,
                             const SurfaceSupportFilter& final_support_filter, const ArxAabb& bounds,
                             const StaticAnchorTraversal& traversal, const AnchorGenOptions& options,
                             AnchorGenDiagnostics* diagnostics = nullptr);

Error buildAnchorConnections(std::vector<AnchorConnection>& out, std::span<const Anchor> anchors,
                             const StaticAnchorTraversal& traversal, const AnchorConnectionGenOptions& options,
                             AnchorConnectionGenDiagnostics* diagnostics = nullptr);

}  // namespace pistoris::navigation
