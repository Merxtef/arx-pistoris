// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
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
#include <limits>
#include <optional>
#include <ranges>
#include <span>

namespace {

template <std::ranges::random_access_range View, class T>
ArxReturnCode copyView(const View& view, std::size_t offset, std::size_t count, T* out) {
  if (offset > view.size() || count > view.size() - offset) return ARX_INDEX_OUT_OF_RANGE;
  if (count != 0 && out == nullptr) return ARX_INVALID_DATA_POINTER;
  for (std::size_t index = 0; index < count; ++index) out[index] = view[offset + index];
  return ARX_OK;
}

template <class T>
std::optional<std::span<T>> optionalOutput(T* data, std::size_t count) noexcept {
  if (!data) return std::nullopt;
  return std::span<T>(data, count);
}

template <class T>
bool validOutput(T* data, std::size_t count) noexcept {
  if (!data) return count == 0;
  if (count == 0) return true;
  if (reinterpret_cast<std::uintptr_t>(data) % alignof(T) != 0 ||
      count > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T) ||
      count > std::numeric_limits<std::uintptr_t>::max() / sizeof(T))
    return false;
  const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(data);
  const std::uintptr_t bytes = count * sizeof(T);
  return begin <= std::numeric_limits<std::uintptr_t>::max() - bytes;
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

#define ARX_LEVEL_COUNT(name, method)                                                     \
  ArxReturnCode arx_pistoris_level_##name##_count(                                        \
      const ArxLevel* level, size_t* out_count, ArxError* error) noexcept {               \
    if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);           \
    if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error); \
    *out_count = level->value.method();                                                   \
    return pistoris::c_api::publishCode(ARX_OK, error);                                   \
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

ArxReturnCode arx_pistoris_level_resource_path(const ArxLevel* level, ArxStringView* out_path,
                                               ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_path) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_path = pistoris::c_api::view(level->value.resourcePath());
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_minimap(const ArxLevel* level, ArxLevelMinimapView* out_minimap,
                                         ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_minimap) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Level::MinimapView minimap = level->value.minimap();
  *out_minimap = {
      .encoded_image = minimap.encoded_image,
      .world_xz_bounds = minimap.world_xz_bounds,
  };
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_loading_screen(const ArxLevel* level, ArxEncodedImageView* out_image,
                                                ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_image) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_image = level->value.loadingScreen();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

#define ARX_LEVEL_COPY(name, method, type)                                                                            \
  ArxReturnCode arx_pistoris_level_copy_##name(                                                                       \
      const ArxLevel* level, size_t offset, size_t count, type(*out_values), ArxError* error) noexcept {              \
    if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                                       \
    return pistoris::c_api::guard(error, [&] { return copyView(level->value.method(), offset, count, out_values); }); \
  }

ARX_LEVEL_COPY(vertices, vertices, ArxLevelVertex)
ARX_LEVEL_COPY(faces, faces, ArxLevelFace)
ARX_LEVEL_COPY(texture_views, textures, ArxTextureView)
ARX_LEVEL_COPY(rooms, rooms, ArxLevelRoom)
ARX_LEVEL_COPY(portals, portals, ArxLevelPortal)
ARX_LEVEL_COPY(room_distances, roomDistances, ArxLevelRoomDistance)
ARX_LEVEL_COPY(anchors, anchors, ArxLevelAnchor)
ARX_LEVEL_COPY(anchor_connections, anchorConnections, ArxLevelAnchorConnection)
ARX_LEVEL_COPY(nav_surface_vertices, navSurfaceVertices, ArxLevelVertex)
ARX_LEVEL_COPY(nav_surface_triangles, navSurfaceTriangles, ArxLevelNavSurfaceTriangle)
ARX_LEVEL_COPY(lights, lights, ArxLevelLight)
ARX_LEVEL_COPY(entities, entities, ArxLevelEntity)
ARX_LEVEL_COPY(fogs, fogs, ArxLevelFog)
ARX_LEVEL_COPY(zones, zones, ArxLevelZone)
ARX_LEVEL_COPY(paths, paths, ArxLevelPath)

#undef ARX_LEVEL_COPY

ArxReturnCode arx_pistoris_level_copy_vertex_positions(const ArxLevel* level, float* positions, size_t position_count,
                                                       ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!validOutput(positions, position_count)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(level->value.copyVertexPositions(std::span(positions, position_count)), error);
}

ArxReturnCode arx_pistoris_level_copy_face_data(const ArxLevel* level, const ArxLevelFacesOutput* output,
                                                ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!output) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!validOutput(output->vertex_indices, output->vertex_index_count) || !validOutput(output->uvs, output->uv_count) ||
      !validOutput(output->corner_normals, output->corner_normal_count) ||
      !validOutput(output->textures, output->texture_count) ||
      !validOutput(output->transvals, output->transval_count) ||
      !validOutput(output->corner_colors, output->corner_color_count) ||
      !validOutput(output->face_normals, output->face_normal_count) || !validOutput(output->flags, output->flag_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  pistoris::Level::FacesOutput values{
      .vertex_indices = optionalOutput(output->vertex_indices, output->vertex_index_count),
      .uvs = optionalOutput(output->uvs, output->uv_count),
      .corner_normals = optionalOutput(output->corner_normals, output->corner_normal_count),
      .textures = optionalOutput(output->textures, output->texture_count),
      .transvals = optionalOutput(output->transvals, output->transval_count),
      .corner_colors = optionalOutput(output->corner_colors, output->corner_color_count),
      .face_normals = optionalOutput(output->face_normals, output->face_normal_count),
      .flags = optionalOutput(output->flags, output->flag_count),
  };
  return pistoris::c_api::publish(level->value.copyFaces(values), error);
}

ArxReturnCode arx_pistoris_level_copy_face_textures(const ArxLevel* level, ArxTextureIndex* textures,
                                                    size_t texture_count, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!validOutput(textures, texture_count)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(level->value.copyFaceTextures(std::span(textures, texture_count)), error);
}

ArxReturnCode arx_pistoris_level_copy_face_rooms(const ArxLevel* level, ArxRoomIndex* rooms, size_t room_count,
                                                 ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!validOutput(rooms, room_count)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(level->value.copyFaceRooms(std::span(rooms, room_count)), error);
}

ArxReturnCode arx_pistoris_level_copy_room_distance_data(const ArxLevel* level,
                                                         const ArxLevelRoomDistancesOutput* output,
                                                         ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!output) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!validOutput(output->distances, output->distance_count) ||
      !validOutput(output->endpoint_portals, output->endpoint_portal_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Level::RoomDistancesOutput values{
      .distances = optionalOutput(output->distances, output->distance_count),
      .endpoint_portals = optionalOutput(output->endpoint_portals, output->endpoint_portal_count),
  };
  return pistoris::c_api::publish(level->value.copyRoomDistances(values), error);
}

ArxReturnCode arx_pistoris_level_copy_anchor_data(const ArxLevel* level, const ArxLevelAnchorsOutput* output,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!output) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!validOutput(output->positions, output->position_count) || !validOutput(output->radii, output->radius_count) ||
      !validOutput(output->heights, output->height_count) || !validOutput(output->flags, output->flag_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Level::AnchorsOutput values{
      .positions = optionalOutput(output->positions, output->position_count),
      .radii = optionalOutput(output->radii, output->radius_count),
      .heights = optionalOutput(output->heights, output->height_count),
      .flags = optionalOutput(output->flags, output->flag_count),
  };
  return pistoris::c_api::publish(level->value.copyAnchors(values), error);
}

ArxReturnCode arx_pistoris_level_copy_anchor_connection_endpoints(const ArxLevel* level, ArxAnchorIndex* endpoints,
                                                                  size_t endpoint_count, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!validOutput(endpoints, endpoint_count)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(level->value.copyAnchorConnections(std::span(endpoints, endpoint_count)), error);
}

ArxReturnCode arx_pistoris_level_copy_nav_surface_data(const ArxLevel* level, const ArxLevelNavSurfaceOutput* output,
                                                       ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!output) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (!validOutput(output->positions, output->position_count) ||
      !validOutput(output->triangle_indices, output->triangle_index_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Level::NavSurfaceOutput values{
      .positions = optionalOutput(output->positions, output->position_count),
      .triangle_indices = optionalOutput(output->triangle_indices, output->triangle_index_count),
  };
  return pistoris::c_api::publish(level->value.copyNavSurface(values), error);
}

ArxReturnCode arx_pistoris_level_nav_surface_info(const ArxLevel* level, ArxLevelNavSurfaceInfo* out_info,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_info) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_info = level->value.navSurfaceInfo();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_player_spawn(const ArxLevel* level, ArxLevelPlayerSpawn* out_spawn,
                                              ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_spawn) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_spawn = level->value.playerSpawn();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_copy_zone_perimeter(const ArxLevel* level, ArxZoneIndex zone, size_t offset,
                                                     size_t count, ArxVector2* out_points, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    const auto view = level->value.zonePerimeter(zone);
    if (!view) return pistoris::c_api::publish(view, error);
    return copyView(*view, offset, count, out_points);
  });
}

ArxReturnCode arx_pistoris_level_copy_path_nodes(const ArxLevel* level, ArxPathIndex path, size_t offset, size_t count,
                                                 ArxLevelPathNode* out_nodes, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    const auto view = level->value.pathNodes(path);
    if (!view) return pistoris::c_api::publish(view, error);
    return copyView(*view, offset, count, out_nodes);
  });
}

ArxReturnCode arx_pistoris_level_room_distance(const ArxLevel* level, ArxRoomIndex room_a, ArxRoomIndex room_b,
                                               ArxLevelRoomDistance* out_distance, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_distance) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    const auto distance = level->value.roomDistance(room_a, room_b);
    if (!distance) return pistoris::c_api::publish(distance, error);
    *out_distance = *distance;
    return static_cast<ArxReturnCode>(ARX_OK);
  });
}

// NOLINTEND(readability-identifier-naming)
