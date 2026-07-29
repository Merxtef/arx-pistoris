// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level_diagnostics.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "../coordinates.h"
#include "../palette.h"
#include "common.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::glb_level_debug {
namespace {

using glb_level::Palette;
using glb_level::PaletteItem;

ArxVector3 portalCentroid(const ArxLevelPortal& portal) {
  const std::size_t count = portal.shape == ARX_PORTAL_QUAD ? 4U : 3U;
  ArxVector3 centroid{};
  for (std::size_t i = 0; i < count; ++i) {
    centroid.x += portal.vertices[i].x;
    centroid.y += portal.vertices[i].y;
    centroid.z += portal.vertices[i].z;
  }
  const float scale = 1.0f / static_cast<float>(count);
  return {centroid.x * scale, centroid.y * scale, centroid.z * scale};
}

bool hasCompleteRoomDistances(const pistoris::Level& level) {
  const std::size_t room_count = level.roomCount();
  if (room_count < 2U) return level.roomDistanceCount() == 0;
  return level.roomDistanceCount() == room_count * (room_count - 1U) / 2U;
}

void addPortalContextNodes(Builder& builder, Palette& palette, int parent, std::span<const ArxLevelPortal> portals) {
  if (portals.empty()) return;
  int material = palette.material(PaletteItem::kPortalCentroid);
  int mesh = addMarkerMesh(builder, "room_distance_debug_portal", material, 10.0f);
  int group = builder.addNode("room_distance_debug_portals");
  builder.addChild(parent, group);
  for (std::size_t i = 0; i < portals.size(); ++i)
    addMarkerNode(builder, group, mesh, std::format("room_distance_debug_portal_{:06}", i), portalCentroid(portals[i]));
}

void addPortalPlaneContextMesh(Builder& builder, Palette& palette, int parent,
                               std::span<const ArxLevelPortal> portals) {
  if (portals.empty()) return;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(portals.size() * 4);
  indices.reserve(portals.size() * 6);
  for (const ArxLevelPortal& portal : portals) {
    const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
    const std::size_t count = portal.shape == ARX_PORTAL_QUAD ? 4U : 3U;
    for (std::size_t i = 0; i < count; ++i) positions.push_back(toVec3(portal.vertices[i]));
    indices.push_back(base);
    indices.push_back(base + 1);
    indices.push_back(base + 2);
    if (count == 4U) {
      indices.push_back(base);
      indices.push_back(base + 2);
      indices.push_back(base + 3);
    }
  }
  int material = palette.material(PaletteItem::kPortal);
  addDebugMeshChild(builder, parent, "room_distance_debug_portal_planes", positions, indices, material);
}

void addRoomSupportContextMeshes(
    Builder& builder, Palette& palette, int parent,
    std::span<const std::vector<pistoris::level_debug::RoomDistanceSupportTriangle>> support_by_room) {
  if (support_by_room.empty()) return;
  int group = builder.addNode("room_distance_debug_room_support");
  builder.addChild(parent, group);
  int material = palette.material(PaletteItem::kNavigationSupport);
  for (std::size_t room = 0; room < support_by_room.size(); ++room) {
    const std::vector<pistoris::level_debug::RoomDistanceSupportTriangle>& triangles = support_by_room[room];
    std::vector<GlbVec3> positions;
    std::vector<std::uint32_t> indices;
    positions.reserve(triangles.size() * 3);
    indices.reserve(triangles.size() * 3);
    for (const pistoris::level_debug::RoomDistanceSupportTriangle& triangle : triangles) {
      const std::uint32_t base = static_cast<std::uint32_t>(positions.size());
      positions.push_back(toVec3(triangle.vertices[0]));
      positions.push_back(toVec3(triangle.vertices[1]));
      positions.push_back(toVec3(triangle.vertices[2]));
      indices.push_back(base);
      indices.push_back(base + 1);
      indices.push_back(base + 2);
    }
    addDebugMeshChild(
        builder, group, std::format("room_distance_debug_room_{:06}_support", room), positions, indices, material);
  }
}

void addStoredRoomDistanceContextMesh(Builder& builder, Palette& palette, int parent, const pistoris::Level& level,
                                      std::span<const ArxLevelPortal> portals) {
  if (!hasCompleteRoomDistances(level)) return;
  constexpr float kHalfWidth = 4.0f;
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(level.roomDistanceCount() * 4);
  indices.reserve(level.roomDistanceCount() * 6);
  for (std::size_t room_a = 0; room_a < level.roomCount(); ++room_a) {
    for (std::size_t room_b = room_a + 1U; room_b < level.roomCount(); ++room_b) {
      ArxLevelRoomDistance distance;
      std::uint8_t has_distance = 0;
      if (level.getRoomDistance(static_cast<pistoris::RoomIndex>(room_a),
                                static_cast<pistoris::RoomIndex>(room_b),
                                has_distance,
                                distance) != ARX_OK ||
          has_distance == 0)
        continue;
      if (distance.portal_a >= portals.size() || distance.portal_b >= portals.size()) continue;
      appendSegmentQuad(portalCentroid(portals[distance.portal_a]),
                        portalCentroid(portals[distance.portal_b]),
                        kHalfWidth,
                        positions,
                        indices);
    }
  }
  int material = palette.material(PaletteItem::kStoredRoomDistance);
  addDebugMeshChild(builder, parent, "room_distance_debug_stored_distances", positions, indices, material);
}

void addRoomDistanceSegmentMesh(Builder& builder, Palette& palette, int parent, const std::string& name,
                                std::span<const pistoris::level_debug::RoomDistanceDebugSegment> segments,
                                PaletteItem palette_item, float half_width) {
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  positions.reserve(segments.size() * 4);
  indices.reserve(segments.size() * 6);
  for (const pistoris::level_debug::RoomDistanceDebugSegment& segment : segments)
    appendSegmentQuad(segment.start, segment.end, half_width, positions, indices);
  int material = palette.material(palette_item);
  addDebugMeshChild(builder, parent, name, positions, indices, material);
}

void addRoomDistancePointGroups(
    Builder& builder, Palette& palette, int parent, const std::string& root_name, const std::string& item_prefix,
    std::span<const std::vector<pistoris::level_debug::RoomDistanceDebugPoint>> points_by_room,
    PaletteItem palette_item, float marker_size) {
  int material = palette.material(palette_item);
  int mesh = addMarkerMesh(builder, item_prefix, material, marker_size);
  int root = builder.addNode(root_name);
  builder.addChild(parent, root);
  for (std::size_t room = 0; room < points_by_room.size(); ++room) {
    int room_node = builder.addNode(std::format("{}_room_{:06}", root_name, room));
    builder.addChild(root, room_node);
    for (std::size_t i = 0; i < points_by_room[room].size(); ++i) {
      addMarkerNode(builder,
                    room_node,
                    mesh,
                    std::format("{}_room_{:06}_{:06}", item_prefix, room, i),
                    points_by_room[room][i].position);
    }
  }
}

void addRoomDistancePathMesh(Builder& builder, int parent, const std::string& name, std::span<const ArxVector3> points,
                             int material, float half_width) {
  std::vector<GlbVec3> positions;
  std::vector<std::uint32_t> indices;
  if (points.size() < 2U) return;
  positions.reserve((points.size() - 1U) * 4U);
  indices.reserve((points.size() - 1U) * 6U);
  for (std::size_t i = 1; i < points.size(); ++i)
    appendSegmentQuad(points[i - 1U], points[i], half_width, positions, indices);
  addDebugMeshChild(builder, parent, name, positions, indices, material);
}

void addInRoomPortalPathGroups(
    Builder& builder, Palette& palette, int parent,
    std::span<const std::vector<pistoris::level_debug::RoomDistanceDebugPath>> paths_by_room) {
  int root = builder.addNode("room_distance_debug_in_room_portal_paths");
  builder.addChild(parent, root);
  int material = palette.material(PaletteItem::kInRoomPortalPath);
  for (std::size_t room = 0; room < paths_by_room.size(); ++room) {
    int room_node = builder.addNode(std::format("room_distance_debug_in_room_portal_paths_room_{:06}", room));
    builder.addChild(root, room_node);
    for (std::size_t i = 0; i < paths_by_room[room].size(); ++i) {
      const pistoris::level_debug::RoomDistanceDebugPath& path = paths_by_room[room][i];
      addRoomDistancePathMesh(builder,
                              room_node,
                              std::format("room_distance_debug_in_room_portal_path_room_{:06}_portal_{:06}_{:06}_{:06}",
                                          room,
                                          path.portal_1 == pistoris::kInvalidPortalIndex ? 0U : path.portal_1,
                                          path.portal_2 == pistoris::kInvalidPortalIndex ? 0U : path.portal_2,
                                          i),
                              path.points,
                              material,
                              3.0f);
    }
  }
}

void addRoomPairPathGroups(Builder& builder, Palette& palette, int parent,
                           std::span<const pistoris::level_debug::RoomDistanceDebugPath> paths) {
  int root = builder.addNode("room_distance_debug_room_pair_paths");
  builder.addChild(parent, root);
  int material = palette.material(PaletteItem::kRoomPairPath);
  for (std::size_t i = 0; i < paths.size(); ++i) {
    const pistoris::level_debug::RoomDistanceDebugPath& path = paths[i];
    addRoomDistancePathMesh(
        builder,
        root,
        std::format("room_distance_debug_room_pair_path_{:06}_{:06}_{:06}", path.room_1, path.room_2, i),
        path.points,
        material,
        5.0f);
  }
}

void addRoomDistanceDiagnostics(Builder& builder, Palette& palette, int parent,
                                const pistoris::level_debug::RoomDistanceGenDiagnostics& diagnostics) {
  if (!diagnostics.portal_access_points_by_room.empty()) {
    addRoomDistancePointGroups(builder,
                               palette,
                               parent,
                               "room_distance_debug_portal_access_points",
                               "room_distance_debug_portal_access_point",
                               diagnostics.portal_access_points_by_room,
                               PaletteItem::kPortalAccessPoint,
                               7.0f);
  }
  if (!diagnostics.sampled_points_by_room.empty()) {
    addRoomDistancePointGroups(builder,
                               palette,
                               parent,
                               "room_distance_debug_sampled_points",
                               "room_distance_debug_sampled_point",
                               diagnostics.sampled_points_by_room,
                               PaletteItem::kSampledPoint,
                               5.0f);
  }
  if (!diagnostics.portal_access_segments.empty()) {
    addRoomDistanceSegmentMesh(builder,
                               palette,
                               parent,
                               "room_distance_debug_portal_access_segments",
                               diagnostics.portal_access_segments,
                               PaletteItem::kPortalAccessSegment,
                               2.5f);
  }
  if (!diagnostics.in_room_visibility_edges.empty()) {
    addRoomDistanceSegmentMesh(builder,
                               palette,
                               parent,
                               "room_distance_debug_in_room_visibility_edges",
                               diagnostics.in_room_visibility_edges,
                               PaletteItem::kVisibilityEdge,
                               1.5f);
  }
  if (!diagnostics.in_room_portal_paths_by_room.empty())
    addInRoomPortalPathGroups(builder, palette, parent, diagnostics.in_room_portal_paths_by_room);
  if (!diagnostics.room_pair_paths.empty())
    addRoomPairPathGroups(builder, palette, parent, diagnostics.room_pair_paths);
}

std::size_t pointGroupCount(
    std::span<const std::vector<pistoris::level_debug::RoomDistanceDebugPoint>> points_by_room) {
  std::size_t count = 0;
  for (const std::vector<pistoris::level_debug::RoomDistanceDebugPoint>& points : points_by_room)
    count += points.size();
  return count;
}

std::size_t pathGroupCount(std::span<const std::vector<pistoris::level_debug::RoomDistanceDebugPath>> paths_by_room) {
  std::size_t count = 0;
  for (const std::vector<pistoris::level_debug::RoomDistanceDebugPath>& paths : paths_by_room) count += paths.size();
  return count;
}

}  // namespace

}  // namespace pistoris::glb_level_debug

namespace pistoris::level_debug {

ArxReturnCode exportRoomDistanceDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                         const RoomDistanceGenDiagnostics* diagnostics,
                                         const Level::GlbExportOptions& options) {
  return glb_level_debug::guardDebugExport("level_debug::exportRoomDistanceDebugGlb", [&] {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = level.validateMesh();
    if (rc != ARX_OK) return rc;
    rc = level.validateRooms();
    if (rc != ARX_OK) return rc;
    rc = level.validateFaceRooms();
    if (rc != ARX_OK) return rc;
    rc = level.validateRoomDistances();
    if (rc != ARX_OK) return rc;
    rc = level.validatePortals();
    if (rc != ARX_OK) return rc;

    glb_level_debug::Builder builder;
    glb_level::Palette palette(builder);
    rc = glb_level::configureGlbExportCoordinates(builder, options);
    if (rc != ARX_OK) return rc;
    int root = builder.addNode("room_distance_debug");
    builder.addRoot(root);

    int context = builder.addNode("room_distance_debug_context");
    builder.addChild(root, context);
    glb_level_debug::addGeometryContextMesh(builder, context, level, palette);
    if (diagnostics)
      glb_level_debug::addRoomSupportContextMeshes(builder, palette, context, diagnostics->support_by_room);
    std::vector<ArxLevelPortal> portals(level.portalCount());
    rc = level.copyPortals(0, portals.size(), portals.data());
    if (rc != ARX_OK) return rc;
    glb_level_debug::addPortalPlaneContextMesh(builder, palette, context, portals);
    glb_level_debug::addPortalContextNodes(builder, palette, root, portals);
    glb_level_debug::addStoredRoomDistanceContextMesh(builder, palette, root, level, portals);
    if (diagnostics) glb_level_debug::addRoomDistanceDiagnostics(builder, palette, root, *diagnostics);

    log(ARX_LOG_INFO,
        std::format("Level room distance debug GLB export: {} room(s), {} portal(s), compact distances {}",
                    level.roomCount(),
                    level.portalCount(),
                    glb_level_debug::hasCompleteRoomDistances(level) ? "present" : "missing"));
    if (diagnostics) {
      log(ARX_LOG_DEBUG,
          std::format("Level room distance debug diagnostics: {} portal access point(s), {} sampled point(s), {} "
                      "portal access segment(s), {} visibility edge(s), {} in-room path(s), {} room-pair path(s)",
                      glb_level_debug::pointGroupCount(diagnostics->portal_access_points_by_room),
                      glb_level_debug::pointGroupCount(diagnostics->sampled_points_by_room),
                      diagnostics->portal_access_segments.size(),
                      diagnostics->in_room_visibility_edges.size(),
                      glb_level_debug::pathGroupCount(diagnostics->in_room_portal_paths_by_room),
                      diagnostics->room_pair_paths.size()));
    }
    rc = builder.write(tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

}  // namespace pistoris::level_debug
