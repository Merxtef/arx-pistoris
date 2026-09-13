// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

bool validAnchorGenerationOptions(const AnchorGenerationOptions& options) {
  return math::finite(options.sample_spacing) && options.sample_spacing >= kMinAnchorSpacing &&
         math::finite(options.radius) && options.radius >= kMinAnchorRadius && math::finite(options.height) &&
         options.height <= kMinAnchorHeight;
}

bool validNavSurfaceGenerationOptions(const NavSurfaceGenerationOptions& options) {
  return math::finite(options.radius) && options.radius >= kMinAnchorRadius && math::finite(options.height) &&
         options.height <= kMinAnchorHeight && math::finite(options.max_step_up) && options.max_step_up >= 0.0f &&
         validNavSurfaceSourceOptions(options);
}

bool validAnchorConnectionOptions(const AnchorConnectionGenerationOptions& options) {
  return math::finite(options.max_distance) && options.max_distance > 0.0f && math::finite(options.max_step_distance) &&
         options.max_step_distance > 0.0f && math::finite(options.max_step_up) && options.max_step_up >= 0.0f &&
         math::finite(options.radius_scale) && options.radius_scale >= 0.5f && options.radius_scale <= 1.0f &&
         options.max_steps > 0;
}

SurfaceSupportFilter defaultGeneratedAnchorSupportFilter() {
  NavSurfaceSourceOptions options;
  return navSurfaceSupportFilter(options);
}

SurfaceDebugTriangle toDebugTriangle(const geometry::SurfaceSupportTriangle& triangle) {
  return {.vertices = triangle.vertices};
}

void captureSupportDiagnostics(const geometry::SurfaceSupportIndex& support_index,
                               NavSurfaceGenerationDiagnostics& diagnostics) {
  diagnostics.support.reserve(support_index.size());
  for (std::size_t index = 0; index < support_index.size(); ++index)
    diagnostics.support.push_back(toDebugTriangle(support_index.triangle(index)));
}

}  // namespace

Error generateSurface(NavSurface& out, const GeometryData& geometry, const NavSurfaceGenerationOptions& options,
                      NavSurfaceGenerationDiagnostics* diagnostics) {
  if (!validNavSurfaceGenerationOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) *diagnostics = {};

  SurfaceSupportFilter support_filter = navSurfaceSupportFilter(options);
  NavSurfaceSupportIndexes support_indexes = buildNavSurfaceSupportIndexes(geometry, options);
  const geometry::SurfaceSupportIndex& support_index = support_indexes.support;
  if (support_index.empty()) return Error::kEmptyResult;
  if (diagnostics) captureSupportDiagnostics(support_index, *diagnostics);

  const ArxAabb bounds = support_index.bounds();
  StaticAnchorTraversal traversal(geometry);
  const geometry::SurfaceSupportIndex& geometry_support = support_indexes.geometry_support;
  NavSurface surface;
  Error error = buildNavSurface(
      surface, geometry, bounds, support_index, geometry_support, support_filter, traversal, options, diagnostics);
  if (error != Error::kNone) return error;

  error = validateSurface(surface);
  if (error != Error::kNone) return error;

  log(ARX_LOG_DEBUG,
      "Navigation-surface generation: {} support triangles, {} geometry triangles, {} vertices, {} triangles, "
      "radius {}, height {}, step up {}",
      support_index.size(),
      geometry_support.size(),
      surface.vertices.size(),
      surface.triangles.size(),
      options.radius,
      options.height,
      options.max_step_up);
  out = std::move(surface);
  return Error::kNone;
}

Error generateAnchors(std::vector<Anchor>& out, const GeometryData& geometry, const NavSurface& surface,
                      const ArxAabb& referenced_bounds, const AnchorGenerationOptions& options,
                      AnchorGenerationDiagnostics* diagnostics) {
  if (!validAnchorGenerationOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) *diagnostics = {};

  std::vector<Anchor> anchors;
  StaticAnchorTraversal traversal(geometry);
  geometry::SurfaceSupportIndex geometry_support = geometry::buildSurfaceSupportIndex(geometry);
  SurfaceSupportFilter final_support_filter = defaultGeneratedAnchorSupportFilter();
  geometry::SurfaceSupportIndex support_index = buildSurfaceSupportIndex(surface);
  Error error = buildNavigationAnchors(anchors,
                                       geometry,
                                       support_index,
                                       geometry_support,
                                       final_support_filter,
                                       referenced_bounds,
                                       traversal,
                                       options,
                                       diagnostics);
  if (error != Error::kNone) return error;

  error = validateAnchorDefinitions(anchors);
  if (error != Error::kNone) return error;

  log(ARX_LOG_DEBUG,
      "Anchor generation: {} surface vertices, {} surface triangles, {} anchors, spacing {}, radius {}, height {}",
      surface.vertices.size(),
      surface.triangles.size(),
      anchors.size(),
      options.sample_spacing,
      options.radius,
      options.height);
  out = std::move(anchors);
  return Error::kNone;
}

Error generateAnchorConnections(std::vector<AnchorConnection>& out, const GeometryData& geometry,
                                std::span<const Anchor> anchors, const AnchorConnectionGenerationOptions& options,
                                AnchorConnectionGenerationDiagnostics* diagnostics) {
  if (!validAnchorConnectionOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) *diagnostics = {};

  StaticAnchorTraversal traversal(geometry);
  std::vector<AnchorConnection> connections;
  Error error = buildAnchorConnections(connections, anchors, traversal, options, diagnostics);
  if (error != Error::kNone) return error;
  error = validateConnections(anchors, connections);
  if (error != Error::kNone) return error;
  log(ARX_LOG_DEBUG,
      "Anchor-connection generation: {} anchors, {} connections, max distance {}, step distance {}, step up {}, "
      "max steps {}",
      anchors.size(),
      connections.size(),
      options.max_distance,
      options.max_step_distance,
      options.max_step_up,
      options.max_steps);
  out = std::move(connections);
  return Error::kNone;
}

}  // namespace pistoris::navigation
