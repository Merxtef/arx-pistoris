// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"
#include "utils/unique_name.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

using math::finite;

bool validAnchorGenerationOptions(const AnchorGenOptions& options) {
  return finite(options.sample_spacing) && options.sample_spacing >= kMinAnchorSpacing && finite(options.radius) &&
         options.radius >= kMinAnchorRadius && finite(options.height) && options.height <= kMinAnchorHeight;
}

bool validNavSurfaceGenerationOptions(const NavSurfaceGenOptions& options) {
  return finite(options.radius) && options.radius >= kMinAnchorRadius && finite(options.height) &&
         options.height <= kMinAnchorHeight && finite(options.max_step_up) && options.max_step_up >= 0.0f &&
         validNavSurfaceSourceOptions(options);
}

bool validAnchorConnectionOptions(const AnchorConnectionGenOptions& options) {
  return finite(options.max_distance) && options.max_distance > 0.0f && finite(options.max_step_distance) &&
         options.max_step_distance > 0.0f && finite(options.max_step_up) && options.max_step_up >= 0.0f &&
         finite(options.radius_scale) && options.radius_scale >= 0.5f && options.radius_scale <= 1.0f &&
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
                               NavSurfaceGenDiagnostics& diagnostics) {
  std::vector<geometry::SurfaceSupportTriangle> triangles = support_index.triangles();
  diagnostics.support.reserve(triangles.size());
  for (const geometry::SurfaceSupportTriangle& triangle : triangles)
    diagnostics.support.push_back(toDebugTriangle(triangle));
}

}  // namespace

bool validNavSurfaceSourceOptions(const NavSurfaceSourceOptions& options) noexcept {
  return math::finite(options.clearance) && options.clearance >= 0.0f && math::finite(options.support_min_up_cos) &&
         options.support_min_up_cos >= 0.0f && options.support_min_up_cos <= 1.0f &&
         (options.support_ignore_flags & ~kFaceBitsAll) == 0;
}

Error validateAnchor(const Anchor& anchor) noexcept {
  if (!validSemanticString(anchor.name)) return Error::kBadAnchorName;
  if (!finite(anchor.position)) return Error::kBadAnchorPosition;
  if (!finite(anchor.radius) || anchor.radius < 0.0f) return Error::kBadAnchorRadius;
  if (!finite(anchor.height) || anchor.height > 0.0f) return Error::kBadAnchorHeight;
  if ((anchor.flags & ~kAnchorFlagsAll) != 0) return Error::kBadAnchorFlags;
  return Error::kNone;
}

Error validateAnchorDefinitions(std::span<const Anchor> anchors) {
  if (anchors.size() > static_cast<std::size_t>(kInvalidAnchorIndex)) return Error::kTooManyAnchors;
  std::unordered_set<std::string_view> names;
  names.reserve(anchors.size());
  for (const Anchor& anchor : anchors) {
    Error error = validateAnchor(anchor);
    if (error != Error::kNone) return error;
    if (!anchor.name.empty() && !names.insert(anchor.name).second) return Error::kDuplicateAnchorName;
  }
  return Error::kNone;
}

std::size_t makeAnchorNamesUnique(std::span<Anchor> anchors) {
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(anchors.size());
  for (const Anchor& anchor : anchors) {
    if (!anchor.name.empty()) unavailable.insert(anchor.name);
  }

  std::unordered_set<std::string> assigned;
  assigned.reserve(anchors.size());
  std::size_t renamed = 0;
  for (Anchor& anchor : anchors) {
    if (anchor.name.empty() || assigned.insert(anchor.name).second) continue;
    anchor.name = makeUniqueName(anchor.name, unavailable);
    unavailable.insert(anchor.name);
    assigned.insert(anchor.name);
    ++renamed;
  }
  return renamed;
}

Error validateConnection(const AnchorConnection& connection, std::size_t anchor_count) {
  if (connection.first >= anchor_count || connection.second >= anchor_count) return Error::kBadConnectionIndex;
  if (connection.first >= connection.second) return Error::kBadConnectionOrder;
  return Error::kNone;
}

Error validateConnections(std::span<const Anchor> anchors, std::span<const AnchorConnection> connections) {
  if (connections.size() > static_cast<std::size_t>(kInvalidAnchorConnectionIndex)) return Error::kTooManyConnections;
  AnchorConnection previous{};
  bool has_previous = false;
  for (const AnchorConnection& connection : connections) {
    Error error = validateConnection(connection, anchors.size());
    if (error != Error::kNone) return error;
    if (has_previous) {
      if (connection.first < previous.first ||
          (connection.first == previous.first && connection.second <= previous.second))
        return Error::kBadConnectionOrder;
    }
    previous = connection;
    has_previous = true;
  }
  return Error::kNone;
}

Error validateSurface(const NavSurface& surface) {
  if (surface.vertices.empty() || surface.triangles.empty()) return Error::kBadSurface;
  if (surface.vertices.size() > static_cast<std::size_t>(kInvalidNavSurfaceVertexIndex))
    return Error::kTooManySurfaceVertices;
  for (const Vertex& vertex : surface.vertices) {
    if (!finite(vertex.position)) return Error::kBadSurfaceVertex;
  }
  for (const NavSurfaceTriangle& triangle : surface.triangles) {
    if (triangle.vertices[0] >= surface.vertices.size() || triangle.vertices[1] >= surface.vertices.size() ||
        triangle.vertices[2] >= surface.vertices.size())
      return Error::kBadSurfaceTriangle;
    if (triangle.vertices[0] == triangle.vertices[1] || triangle.vertices[0] == triangle.vertices[2] ||
        triangle.vertices[1] == triangle.vertices[2])
      return Error::kDegenerateSurfaceTriangle;
    if (geometry::degenerateTriangle(surface.vertices[triangle.vertices[0]].position,
                                     surface.vertices[triangle.vertices[1]].position,
                                     surface.vertices[triangle.vertices[2]].position))
      return Error::kDegenerateSurfaceTriangle;
  }
  return Error::kNone;
}

Error validateSurface(const std::optional<NavSurface>& surface) {
  if (!surface.has_value()) return Error::kNone;
  return validateSurface(*surface);
}

Error validate(const NavigationData& navigation) {
  Error error = validateAnchorDefinitions(navigation.anchors);
  if (error != Error::kNone) return error;
  error = validateConnections(navigation.anchors, navigation.connections);
  if (error != Error::kNone) return error;
  return validateSurface(navigation.surface);
}

Error generateSurface(NavSurface& out, const GeometryData& geometry, const NavSurfaceGenOptions& options,
                      NavSurfaceGenDiagnostics* diagnostics) {
  if (!validNavSurfaceGenerationOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) *diagnostics = {};

  SurfaceSupportFilter support_filter = navSurfaceSupportFilter(options);
  geometry::SurfaceSupportIndex support_index = buildNavSurfaceSupportIndex(geometry, options);
  if (support_index.empty()) return Error::kEmptyResult;
  if (diagnostics) captureSupportDiagnostics(support_index, *diagnostics);

  const ArxAabb bounds = support_index.bounds();
  StaticAnchorTraversal traversal(geometry);
  geometry::SurfaceSupportIndex geometry_support = geometry::buildSurfaceSupportIndex(geometry);
  NavSurface surface;
  Error error = buildNavSurface(
      surface, geometry, bounds, support_index, geometry_support, support_filter, traversal, options, diagnostics);
  if (error != Error::kNone) return error;

  error = validateSurface(surface);
  if (error != Error::kNone) return error;

  out = std::move(surface);
  return Error::kNone;
}

Error generateAnchors(std::vector<Anchor>& out, const GeometryData& geometry, const NavSurface& surface,
                      const ArxAabb& referenced_bounds, const AnchorGenOptions& options,
                      AnchorGenDiagnostics* diagnostics) {
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

  out = std::move(anchors);
  return Error::kNone;
}

Error generateAnchorConnections(std::vector<AnchorConnection>& out, const GeometryData& geometry,
                                std::span<const Anchor> anchors, const AnchorConnectionGenOptions& options,
                                AnchorConnectionGenDiagnostics* diagnostics) {
  if (!validAnchorConnectionOptions(options)) return Error::kInvalidOptions;
  if (diagnostics) *diagnostics = {};

  StaticAnchorTraversal traversal(geometry);
  std::vector<AnchorConnection> connections;
  Error error = buildAnchorConnections(connections, anchors, traversal, options, diagnostics);
  if (error != Error::kNone) return error;
  error = validateConnections(anchors, connections);
  if (error != Error::kNone) return error;
  out = std::move(connections);
  return Error::kNone;
}

}  // namespace pistoris::navigation
