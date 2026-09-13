// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/texture.h"

#include "api/c/internal.h"
#include "api/c/level/internal.h"  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

#define ARX_LEVEL_COUNT(name, method)                                                                  \
  ArxReturnCode arx_pistoris_level_##name##_count(const ArxLevel* level, size_t* out_count) noexcept { \
    if (!level) return ARX_INVALID_HANDLE;                                                             \
    if (!out_count) return ARX_INVALID_DATA_POINTER;                                                   \
    *out_count = level->value.method();                                                                \
    return ARX_OK;                                                                                     \
  }

ARX_LEVEL_COUNT(vertex, vertexCount)
ARX_LEVEL_COUNT(face, faceCount)
ARX_LEVEL_COUNT(texture, textureCount)
ARX_LEVEL_COUNT(room, roomCount)
ARX_LEVEL_COUNT(portal, portalCount)
ARX_LEVEL_COUNT(room_distance, roomDistanceCount)
ARX_LEVEL_COUNT(anchor, anchorCount)
ARX_LEVEL_COUNT(anchor_connection, anchorConnectionCount)
ARX_LEVEL_COUNT(light, lightCount)
ARX_LEVEL_COUNT(entity, entityCount)
ARX_LEVEL_COUNT(fog, fogCount)
ARX_LEVEL_COUNT(zone, zoneCount)
ARX_LEVEL_COUNT(path, pathCount)

#undef ARX_LEVEL_COUNT

ArxReturnCode arx_pistoris_level_resource_path(const ArxLevel* level, ArxStringView* out_path) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = pistoris::c_api::view(level->value.resourcePath());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_level_minimap(const ArxLevel* level, ArxLevelMinimapView* out_minimap) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_minimap) return ARX_INVALID_DATA_POINTER;
  const pistoris::Level::MinimapView minimap = level->value.minimap();
  *out_minimap = {
      .encoded_image = minimap.encoded_image,
      .world_xz_bounds = minimap.world_xz_bounds,
  };
  return ARX_OK;
}

ArxReturnCode arx_pistoris_level_loading_screen(const ArxLevel* level, ArxEncodedImageView* out_image) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_image) return ARX_INVALID_DATA_POINTER;
  *out_image = level->value.loadingScreen();
  return ARX_OK;
}

#define ARX_LEVEL_COPY(name, method, type)                                                         \
  ArxReturnCode arx_pistoris_level_copy_##name(                                                    \
      const ArxLevel* level, size_t offset, size_t count, type(*out_values)) noexcept {            \
    if (!level) return ARX_INVALID_HANDLE;                                                         \
    return pistoris::c_api::guard([&] { return level->value.method(offset, count, out_values); }); \
  }

ARX_LEVEL_COPY(vertices, copyVertices, ArxLevelVertex)
ARX_LEVEL_COPY(faces, copyFaces, ArxLevelFace)
ARX_LEVEL_COPY(texture_views, copyTextureViews, ArxTextureView)
ARX_LEVEL_COPY(rooms, copyRooms, ArxLevelRoom)
ARX_LEVEL_COPY(portals, copyPortals, ArxLevelPortal)
ARX_LEVEL_COPY(room_distances, copyRoomDistances, ArxLevelRoomDistance)
ARX_LEVEL_COPY(anchors, copyAnchors, ArxLevelAnchor)
ARX_LEVEL_COPY(anchor_connections, copyAnchorConnections, ArxLevelAnchorConnection)
ARX_LEVEL_COPY(nav_surface_vertices, copyNavSurfaceVertices, ArxLevelVertex)
ARX_LEVEL_COPY(nav_surface_triangles, copyNavSurfaceTriangles, ArxLevelNavSurfaceTriangle)
ARX_LEVEL_COPY(lights, copyLights, ArxLevelLight)
ARX_LEVEL_COPY(entities, copyEntities, ArxLevelEntity)
ARX_LEVEL_COPY(fogs, copyFogs, ArxLevelFog)
ARX_LEVEL_COPY(zones, copyZones, ArxLevelZone)
ARX_LEVEL_COPY(paths, copyPaths, ArxLevelPath)

#undef ARX_LEVEL_COPY

ArxReturnCode arx_pistoris_level_nav_surface_info(const ArxLevel* level, ArxLevelNavSurfaceInfo* out_info) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_info) return ARX_INVALID_DATA_POINTER;
  *out_info = level->value.navSurfaceInfo();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_level_player_spawn(const ArxLevel* level, ArxLevelPlayerSpawn* out_spawn) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_spawn) return ARX_INVALID_DATA_POINTER;
  *out_spawn = level->value.playerSpawn();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_level_copy_zone_perimeter(const ArxLevel* level, ArxZoneIndex zone, size_t offset,
                                                     size_t count, ArxVector2* out_points) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.copyZonePerimeter(zone, offset, count, out_points); });
}

ArxReturnCode arx_pistoris_level_copy_path_nodes(const ArxLevel* level, ArxPathIndex path, size_t offset, size_t count,
                                                 ArxLevelPathNode* out_nodes) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.copyPathNodes(path, offset, count, out_nodes); });
}

ArxReturnCode arx_pistoris_level_room_distance(const ArxLevel* level, ArxRoomIndex room_a, ArxRoomIndex room_b,
                                               uint8_t* out_has_distance, ArxLevelRoomDistance* out_distance) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_has_distance || !out_distance) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return level->value.roomDistance(room_a, room_b, *out_has_distance, *out_distance); });
}

// NOLINTEND(readability-identifier-naming)
