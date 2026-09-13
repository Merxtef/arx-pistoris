// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/navigation.h"
#include "modules/navigation/traversal.h"

namespace pistoris::navigation {

std::array<ArxVector3, 17> navigationProbeOffsets(float radius);
bool validNavSurfaceSourceOptions(const NavSurfaceSourceOptions& options) noexcept;

std::vector<FaceIndex> navSurfaceSupportFaceIndices(const GeometryData& geometry,
                                                    const NavSurfaceSourceOptions& options);
SurfaceSupportFilter navSurfaceSupportFilter(const NavSurfaceSourceOptions& options);
struct NavSurfaceSupportIndexes {
  geometry::SurfaceSupportIndex support;
  geometry::SurfaceSupportIndex geometry_support;
};
NavSurfaceSupportIndexes buildNavSurfaceSupportIndexes(const GeometryData& geometry,
                                                       const NavSurfaceSourceOptions& options);
geometry::SurfaceSupportIndex buildSurfaceSupportIndex(const NavSurface& surface);
std::optional<geometry::SurfaceSupportHit> closestMergedSupportHit(const geometry::SurfaceSupportIndex& index, float x,
                                                                   float z, float reference_y, float max_delta,
                                                                   std::vector<geometry::SurfaceSupportHit>& scratch);

bool hasAllowedFinalGeometrySupport(const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                                    const ArxVector3& cylinder_bottom, float radius,
                                    const SurfaceSupportFilter& filter);

Error buildNavSurface(NavSurface& out, const GeometryData& geometry, const ArxAabb& support_bounds,
                      const geometry::SurfaceSupportIndex& support_index,
                      const geometry::SurfaceSupportIndex& geometry_support,
                      const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                      const NavSurfaceGenerationOptions& options,
                      NavSurfaceGenerationDiagnostics* diagnostics = nullptr);

Error buildNavigationAnchors(std::vector<Anchor>& out, const GeometryData& geometry,
                             const geometry::SurfaceSupportIndex& support_index,
                             const geometry::SurfaceSupportIndex& geometry_support,
                             const SurfaceSupportFilter& final_support_filter, const ArxAabb& bounds,
                             StaticAnchorTraversal& traversal, const AnchorGenerationOptions& options,
                             AnchorGenerationDiagnostics* diagnostics = nullptr);

Error buildAnchorConnections(std::vector<AnchorConnection>& out, std::span<const Anchor> anchors,
                             StaticAnchorTraversal& traversal, const AnchorConnectionGenerationOptions& options,
                             AnchorConnectionGenerationDiagnostics* diagnostics = nullptr);

}  // namespace pistoris::navigation
