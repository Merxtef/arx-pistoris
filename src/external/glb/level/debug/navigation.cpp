// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime/types.h"

#include "../coordinates.h"
#include "../palette.h"
#include "api/status_boundary.h"
#include "common.h"
#include "level/data.h"
#include "level/debug/access.h"
#include "utils/log.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level_debug {
namespace {

using glb_level::Palette;
using glb_level::PaletteItem;

void appendNavigationDebugTriangleMeshData(std::span<const pistoris::level_debug::SurfaceDebugTriangle> triangles,
                                           std::vector<GlbVec3>& positions, std::vector<std::uint32_t>& indices) {
  for (const pistoris::level_debug::SurfaceDebugTriangle& triangle : triangles) {
    std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    positions.push_back(toVec3(triangle.vertices[0]));
    positions.push_back(toVec3(triangle.vertices[1]));
    positions.push_back(toVec3(triangle.vertices[2]));
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
  }
}

void addNavigationDebugTriangleMesh(Builder& builder, int parent, const std::string& name,
                                    std::span<const pistoris::level_debug::SurfaceDebugTriangle> triangles,
                                    int material, const ArxVector3& parent_origin) {
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(triangles.size() * 3);
  indices.reserve(triangles.size() * 3);
  appendNavigationDebugTriangleMeshData(triangles, positions, indices);
  addDebugMeshChildAtCenter(builder, parent, name, positions, indices, material, parent_origin);
}

void addNavigationSupportContextMesh(Builder& builder, Palette& palette, int parent,
                                     std::span<const pistoris::level_debug::SurfaceDebugTriangle> triangles) {
  if (triangles.empty()) return;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(triangles.size() * 3);
  indices.reserve(triangles.size() * 3);
  appendNavigationDebugTriangleMeshData(triangles, positions, indices);
  int material = palette.material(PaletteItem::kNavigationSupport);
  addDebugMeshChild(builder, parent, "navigation_debug_floor_context", positions, indices, material);
}

void addNavigationDebugMarkerMesh(Builder& builder, Palette& palette, int parent, const std::string& name,
                                  std::span<const ArxVector3> points, PaletteItem palette_item, float marker_size,
                                  const ArxVector3& parent_origin) {
  if (points.empty()) return;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(points.size() * 6);
  indices.reserve(points.size() * 24);
  for (const ArxVector3& point : points) appendMarkerMeshData(point, marker_size, positions, indices);
  int material = palette.material(palette_item);
  addDebugMeshChildAtCenter(builder, parent, name, positions, indices, material, parent_origin);
}

void addNavSurfaceContextMesh(Builder& builder, Palette& palette, int parent, const NavSurface& surface) {
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(surface.triangles.size() * 3U);
  indices.reserve(surface.triangles.size() * 3U);
  for (const NavSurfaceTriangle& source : surface.triangles) {
    const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    for (NavSurfaceVertexIndex vertex : source.vertices) positions.push_back(toVec3(surface.vertices[vertex].position));
    indices.insert(indices.end(), {base, base + 1U, base + 2U});
  }
  int material = palette.material(PaletteItem::kNavigationSurface);
  addDebugMeshChild(builder, parent, "navigation_debug_surface_context", positions, indices, material);
}

void addAnchorContextNodes(Builder& builder, Palette& palette, int parent, std::span<const Anchor> anchors) {
  if (anchors.empty()) return;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(anchors.size() * 6);
  indices.reserve(anchors.size() * 24);
  for (const Anchor& anchor : anchors) {
    appendMarkerMeshData(anchor.position, 8.0f, positions, indices);
  }
  int material = palette.material(PaletteItem::kAnchor);
  addDebugMeshChildAtCenter(builder, parent, "navigation_debug_anchors", positions, indices, material, {});
}

void addAnchorConnectionContextMesh(Builder& builder, Palette& palette, int parent, std::span<const Anchor> anchors,
                                    std::span<const AnchorConnection> connections) {
  if (connections.empty()) return;
  constexpr float kHalfWidth = 3.0f;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(connections.size() * 4);
  indices.reserve(connections.size() * 6);
  for (const AnchorConnection& connection : connections) {
    if (connection.first >= anchors.size() || connection.second >= anchors.size()) continue;
    const ArxVector3& a = anchors[connection.first].position;
    const ArxVector3& b = anchors[connection.second].position;
    float dx = b.x - a.x;
    float dz = b.z - a.z;
    float len = std::sqrt(dx * dx + dz * dz);
    if (len <= std::numeric_limits<float>::epsilon()) continue;
    float ox = -dz / len * kHalfWidth;
    float oz = dx / len * kHalfWidth;
    std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    positions.push_back({a.x + ox, a.y, a.z + oz});
    positions.push_back({a.x - ox, a.y, a.z - oz});
    positions.push_back({b.x - ox, b.y, b.z - oz});
    positions.push_back({b.x + ox, b.y, b.z + oz});
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    indices.push_back(base);
    indices.push_back(base + 2);
    indices.push_back(base + 3);
  }
  int material = palette.material(PaletteItem::kAnchorConnection);
  addDebugMeshChild(builder, parent, "navigation_debug_anchor_connections", positions, indices, material);
}

void addNavigationDebugSegmentMesh(Builder& builder, Palette& palette, int parent, const std::string& name,
                                   std::span<const DebugSegment> segments, PaletteItem palette_item, float half_width,
                                   const ArxVector3& parent_origin) {
  if (segments.empty()) return;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(segments.size() * 4);
  indices.reserve(segments.size() * 6);
  for (const DebugSegment& segment : segments)
    appendSegmentQuad(segment.start, segment.end, half_width, positions, indices);
  int material = palette.material(palette_item);
  addDebugMeshChildAtCenter(builder, parent, name, positions, indices, material, parent_origin);
}

void addNavSurfaceDiagnostics(Builder& builder, Palette& palette, int parent,
                              const pistoris::level_debug::NavSurfaceGenDiagnostics& diagnostics,
                              const ArxVector3& parent_origin) {
  if (diagnostics.base.empty() && diagnostics.repaired.empty()) return;
  int surface_group = builder.addNode("navigation_debug_surface_generation");
  builder.addChild(parent, surface_group);
  int generated_group = builder.addNode("navigation_debug_surface_generated");
  builder.addChild(surface_group, generated_group);

  int base_material = palette.material(PaletteItem::kSurfaceGenerated);
  int repaired_material = palette.material(PaletteItem::kSurfaceRepaired);

  addNavigationDebugTriangleMesh(
      builder, generated_group, "navigation_debug_surface_base", diagnostics.base, base_material, parent_origin);
  addNavigationDebugTriangleMesh(builder,
                                 generated_group,
                                 "navigation_debug_surface_repaired",
                                 diagnostics.repaired,
                                 repaired_material,
                                 parent_origin);
}

void addNavSurfacePruneDiagnostics(Builder& builder, Palette& palette, int parent,
                                   const pistoris::level_debug::NavSurfacePruneDiagnostics& diagnostics,
                                   const ArxVector3& parent_origin) {
  if (diagnostics.pruned.empty()) return;
  int group = builder.addNode("navigation_debug_surface_pruning");
  int material = palette.material(PaletteItem::kSurfacePruned);
  builder.addChild(parent, group);
  addNavigationDebugTriangleMesh(
      builder, group, "navigation_debug_surface_pruned", diagnostics.pruned, material, parent_origin);
}

void addAnchorGenerationDiagnostics(Builder& builder, Palette& palette, int parent,
                                    const pistoris::level_debug::AnchorGenDiagnostics& diagnostics,
                                    const ArxVector3& parent_origin) {
  if (diagnostics.points.empty()) return;
  std::vector<ArxVector3> repaired;
  std::vector<ArxVector3> rejected;
  std::vector<DebugSegment> repaired_segments;
  for (const pistoris::level_debug::AnchorGenDebugPoint& point : diagnostics.points) {
    switch (point.status) {
      case pistoris::level_debug::AnchorGenDebugStatus::kRepaired:
        repaired.push_back(point.resolved);
        repaired_segments.push_back({point.requested, point.resolved});
        break;
      case pistoris::level_debug::AnchorGenDebugStatus::kRejected:
        rejected.push_back(point.requested);
        break;
    }
  }

  if (repaired.empty() && rejected.empty()) return;
  int group = builder.addNode("navigation_debug_anchor_generation");
  builder.addChild(parent, group);
  addNavigationDebugMarkerMesh(builder,
                               palette,
                               group,
                               "navigation_debug_anchor_repaired",
                               repaired,
                               PaletteItem::kAnchorRepaired,
                               6.0f,
                               parent_origin);
  addNavigationDebugMarkerMesh(builder,
                               palette,
                               group,
                               "navigation_debug_anchor_rejected",
                               rejected,
                               PaletteItem::kAnchorRejected,
                               5.0f,
                               parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_repair_segments",
                                repaired_segments,
                                PaletteItem::kAnchorRepairSegment,
                                1.5f,
                                parent_origin);
}

void addAnchorPruneDiagnostics(Builder& builder, Palette& palette, int parent,
                               const pistoris::level_debug::AnchorComponentPruneDiagnostics& diagnostics,
                               const ArxVector3& parent_origin) {
  if (diagnostics.pruned.empty()) return;
  int group = builder.addNode("navigation_debug_anchor_pruning");
  builder.addChild(parent, group);
  addNavigationDebugMarkerMesh(builder,
                               palette,
                               group,
                               "navigation_debug_anchor_pruned",
                               diagnostics.pruned,
                               PaletteItem::kAnchorPruned,
                               5.0f,
                               parent_origin);
}

void addAnchorConnectionDiagnostics(Builder& builder, Palette& palette, int parent,
                                    const pistoris::level_debug::AnchorConnectionGenDiagnostics& diagnostics,
                                    const ArxVector3& parent_origin) {
  if (!diagnostics.skipped_endpoints.empty()) {
    std::array<std::vector<ArxVector3>, 4> points_by_status;
    std::vector<ArxVector3> resolved_points;
    std::vector<DebugSegment> resolution_segments;
    resolved_points.reserve(diagnostics.skipped_endpoints.size());
    resolution_segments.reserve(diagnostics.skipped_endpoints.size());
    for (const pistoris::level_debug::AnchorConnectionEndpointDebugPoint& endpoint : diagnostics.skipped_endpoints) {
      points_by_status[static_cast<std::size_t>(endpoint.status)].push_back(endpoint.requested);
      resolved_points.push_back(endpoint.resolved);
      resolution_segments.push_back({endpoint.requested, endpoint.resolved});
    }
    int group = builder.addNode("navigation_debug_anchor_connection_endpoints");
    builder.addChild(parent, group);
    addNavigationDebugMarkerMesh(builder,
                                 palette,
                                 group,
                                 "navigation_debug_anchor_endpoint_invalid",
                                 points_by_status[0],
                                 PaletteItem::kInvalid,
                                 6.0f,
                                 parent_origin);
    addNavigationDebugMarkerMesh(builder,
                                 palette,
                                 group,
                                 "navigation_debug_anchor_endpoint_no_support",
                                 points_by_status[1],
                                 PaletteItem::kNoSupport,
                                 6.0f,
                                 parent_origin);
    addNavigationDebugMarkerMesh(builder,
                                 palette,
                                 group,
                                 "navigation_debug_anchor_endpoint_too_far",
                                 points_by_status[2],
                                 PaletteItem::kTooFar,
                                 6.0f,
                                 parent_origin);
    addNavigationDebugMarkerMesh(builder,
                                 palette,
                                 group,
                                 "navigation_debug_anchor_endpoint_unresolved",
                                 points_by_status[3],
                                 PaletteItem::kUnresolved,
                                 6.0f,
                                 parent_origin);
    addNavigationDebugMarkerMesh(builder,
                                 palette,
                                 group,
                                 "navigation_debug_anchor_endpoint_resolved",
                                 resolved_points,
                                 PaletteItem::kResolved,
                                 4.0f,
                                 parent_origin);
    addNavigationDebugSegmentMesh(builder,
                                  palette,
                                  group,
                                  "navigation_debug_anchor_endpoint_resolution",
                                  resolution_segments,
                                  PaletteItem::kResolution,
                                  1.0f,
                                  parent_origin);
  }

  if (diagnostics.rejected_connections.empty()) return;
  std::array<std::vector<DebugSegment>, 6> segments_by_reason;
  std::vector<ArxVector3> failure_requested;
  std::vector<ArxVector3> failure_resolved;
  std::vector<DebugSegment> failure_resolution;
  failure_requested.reserve(diagnostics.rejected_connections.size());
  failure_resolved.reserve(diagnostics.rejected_connections.size());
  failure_resolution.reserve(diagnostics.rejected_connections.size());
  for (const pistoris::level_debug::AnchorConnectionRejectedDebugSegment& segment : diagnostics.rejected_connections) {
    segments_by_reason[static_cast<std::size_t>(segment.reason)].push_back({segment.start, segment.end});
    failure_requested.push_back(segment.failure_requested);
    failure_resolved.push_back(segment.failure_resolved);
    failure_resolution.push_back({segment.failure_requested, segment.failure_resolved});
  }
  int group = builder.addNode("navigation_debug_anchor_rejected_connections");
  builder.addChild(parent, group);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_invalid",
                                segments_by_reason[0],
                                PaletteItem::kInvalid,
                                1.5f,
                                parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_no_support",
                                segments_by_reason[1],
                                PaletteItem::kNoSupport,
                                1.5f,
                                parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_too_far",
                                segments_by_reason[2],
                                PaletteItem::kTooFar,
                                1.5f,
                                parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_unresolved",
                                segments_by_reason[3],
                                PaletteItem::kUnresolved,
                                1.5f,
                                parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_max_steps",
                                segments_by_reason[4],
                                PaletteItem::kMaxSteps,
                                1.5f,
                                parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_end_mismatch",
                                segments_by_reason[5],
                                PaletteItem::kEndMismatch,
                                1.5f,
                                parent_origin);
  addNavigationDebugMarkerMesh(builder,
                               palette,
                               group,
                               "navigation_debug_anchor_rejected_failure_requested",
                               failure_requested,
                               PaletteItem::kRequested,
                               4.0f,
                               parent_origin);
  addNavigationDebugMarkerMesh(builder,
                               palette,
                               group,
                               "navigation_debug_anchor_rejected_failure_resolved",
                               failure_resolved,
                               PaletteItem::kResolved,
                               4.0f,
                               parent_origin);
  addNavigationDebugSegmentMesh(builder,
                                palette,
                                group,
                                "navigation_debug_anchor_rejected_failure_resolution",
                                failure_resolution,
                                PaletteItem::kResolution,
                                1.0f,
                                parent_origin);
}

void addNavigationDiagnostics(Builder& builder, Palette& palette, int parent,
                              const pistoris::level_debug::NavigationDiagnostics& diagnostics, const ArxAabb& bounds) {
  bool empty = diagnostics.surface.base.empty() && diagnostics.surface.repaired.empty() &&
               diagnostics.surface_pruning.pruned.empty() && diagnostics.anchors.points.empty() &&
               diagnostics.anchor_pruning.pruned.empty() && diagnostics.connections.skipped_endpoints.empty() &&
               diagnostics.connections.rejected_connections.empty();
  if (empty) return;

  ArxVector3 origin{};
  int group = addDebugGroupUnderMap(builder, parent, "navigation_debug_generation", bounds, origin);
  addNavSurfaceDiagnostics(builder, palette, group, diagnostics.surface, origin);
  addNavSurfacePruneDiagnostics(builder, palette, group, diagnostics.surface_pruning, origin);
  addAnchorGenerationDiagnostics(builder, palette, group, diagnostics.anchors, origin);
  addAnchorPruneDiagnostics(builder, palette, group, diagnostics.anchor_pruning, origin);
  addAnchorConnectionDiagnostics(builder, palette, group, diagnostics.connections, origin);
}

}  // namespace

}  // namespace pistoris::glb_level_debug

namespace pistoris::level_debug {

ArxReturnCode exportNavigationDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                       const NavigationDiagnostics* diagnostics,
                                       const Level::GlbExportOptions& options) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = level.validateMesh();
    if (rc != ARX_OK) return rc;
    rc = level.validateNavSurface();
    if (rc != ARX_OK) return rc;
    rc = level.validateAnchors();
    if (rc != ARX_OK) return rc;
    rc = level.validateAnchorConnections();
    if (rc != ARX_OK) return rc;
    const LevelModules& modules = LevelDebugAccess::modules(level);
    const LevelValidationState& validation = LevelDebugAccess::validation(level);
    const std::optional<ArxAabb>& referenced_bounds = validation.derived.referenced_bounds;
    if (!referenced_bounds) return ARX_LEVEL_NO_GEOMETRY;

    glb_level_debug::Builder builder;
    glb_level::Palette palette(builder);
    rc = glb_level::configureGlbExportCoordinates(builder, options);
    if (rc != ARX_OK) return rc;
    int root = builder.addNode("navigation_debug");
    builder.addRoot(root);

    int context = builder.addNode("navigation_debug_context");
    builder.addChild(root, context);
    glb_level_debug::addGeometryContextMesh(builder, context, modules.geometry, palette);
    if (diagnostics)
      glb_level_debug::addNavigationSupportContextMesh(builder, palette, context, diagnostics->surface.support);
    if (modules.navigation.surface)
      glb_level_debug::addNavSurfaceContextMesh(builder, palette, context, *modules.navigation.surface);
    glb_level_debug::addAnchorContextNodes(builder, palette, root, modules.navigation.anchors);
    glb_level_debug::addAnchorConnectionContextMesh(
        builder, palette, root, modules.navigation.anchors, modules.navigation.connections);
    if (diagnostics)
      glb_level_debug::addNavigationDiagnostics(builder, palette, root, *diagnostics, *referenced_bounds);

    log(ARX_LOG_INFO,
        "Level navigation debug GLB export: {} anchor(s), {} connection(s), nav surface {}",
        modules.navigation.anchors.size(),
        modules.navigation.connections.size(),
        modules.navigation.surface ? "present" : "missing");
    rc = builder.write(tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

}  // namespace pistoris::level_debug
