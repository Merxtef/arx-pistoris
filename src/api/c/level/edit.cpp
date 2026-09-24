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
#include "api/c/level/internal.h"
#include "api/c/texture/internal.h"

#include <cstddef>
#include <cstdint>

namespace {

bool validWeldOptions(const ArxLevelVertexWeldOptions& options) noexcept {
  const bool valid_metric = options.metric == ARX_LEVEL_WELD_EUCLIDEAN || options.metric == ARX_LEVEL_WELD_AXIS_ALIGNED;
  const bool valid_policy = options.degenerate_faces == ARX_LEVEL_DEGENERATE_FACE_PRESERVE ||
                            options.degenerate_faces == ARX_LEVEL_DEGENERATE_FACE_REJECT ||
                            options.degenerate_faces == ARX_LEVEL_DEGENERATE_FACE_DISCARD;
  return valid_metric && valid_policy;
}

bool validMeshCounts(const ArxLevelMeshInput& mesh) noexcept {
  return mesh.vertex_count <= static_cast<std::size_t>(pistoris::kInvalidVertexIndex) &&
         mesh.face_count <= static_cast<std::size_t>(pistoris::kInvalidFaceIndex) &&
         mesh.texture_count <= static_cast<std::size_t>(pistoris::kNoTexture);
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_set_resource_path(ArxLevel* level, ArxStringView path) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setResourcePath(pistoris::c_api::stringView(path)); });
}

ArxReturnCode arx_pistoris_level_set_minimap(ArxLevel* level, ArxEncodedImageView encoded_image,
                                             ArxRect world_xz_bounds) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(encoded_image)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setMinimap(encoded_image, world_xz_bounds); });
}

ArxReturnCode arx_pistoris_level_set_minimap_from_projection(ArxLevel* level, ArxEncodedImageView encoded_image,
                                                             ArxVector2 projection_offset) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(encoded_image)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return level->value.setMinimapFromProjection(encoded_image, projection_offset); });
}

ArxReturnCode arx_pistoris_level_clear_minimap(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  level->value.clearMinimap();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_level_set_loading_screen(ArxLevel* level, ArxEncodedImageView encoded_image) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(encoded_image)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setLoadingScreen(encoded_image); });
}

ArxReturnCode arx_pistoris_level_clear_loading_screen(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  level->value.clearLoadingScreen();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_level_set_vertex(ArxLevel* level, ArxVertexIndex index, ArxLevelVertex vertex) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.setVertex(index, vertex); });
}

ArxReturnCode arx_pistoris_level_add_vertex(ArxLevel* level, ArxLevelVertex vertex,
                                            ArxVertexIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_index) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addVertex(vertex, *out_index); });
}

ArxReturnCode arx_pistoris_level_add_vertices(ArxLevel* level, const ArxLevelVertex* vertices, size_t count,
                                              ArxVertexIndex* out_first_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_first_index || !pistoris::c_api::valid(vertices, count)) return ARX_INVALID_DATA_POINTER;
  *out_first_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addVertices(vertices, count, *out_first_index); });
}

ArxReturnCode arx_pistoris_level_set_face(ArxLevel* level, ArxFaceIndex index, const ArxLevelFace* face) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!face) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setFace(index, *face); });
}

ArxReturnCode arx_pistoris_level_add_face(ArxLevel* level, const ArxLevelFace* face, ArxFaceIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!face || !out_index) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addFace(*face, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_face(ArxLevel* level, ArxFaceIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeFace(index); });
}

ArxReturnCode arx_pistoris_level_compact_vertices(ArxLevel* level, size_t* out_removed) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_removed) return ARX_INVALID_DATA_POINTER;
  *out_removed = 0;
  return pistoris::c_api::guard([&] { return level->value.compactVertices(out_removed); });
}

ArxReturnCode arx_pistoris_level_compact_textures(ArxLevel* level, size_t* out_removed) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_removed) return ARX_INVALID_DATA_POINTER;
  *out_removed = 0;
  return pistoris::c_api::guard([&] { return level->value.compactTextures(out_removed); });
}

ArxReturnCode arx_pistoris_level_rebase_texture_paths(ArxLevel* level, ArxStringView directory) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(directory)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return level->value.rebaseTexturePaths(pistoris::c_api::stringView(directory)); });
}

ArxReturnCode arx_pistoris_level_weld_vertices(ArxLevel* level, const ArxLevelVertexWeldOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (options && !validWeldOptions(*options)) return ARX_INVALID_OPTIONS;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.weldVertices();
    return level->value.weldVertices(pistoris::c_api::weldOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_snap_geometry_to_portals(ArxLevel* level,
                                                          const ArxLevelPortalSnapOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.snapGeometryToPortals();
    return level->value.snapGeometryToPortals(pistoris::c_api::portalSnapOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_set_texture(ArxLevel* level, ArxTextureIndex index,
                                             const ArxTextureView* texture) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!texture || !pistoris::c_api::valid(*texture)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setTexture(index, *texture); });
}

ArxReturnCode arx_pistoris_level_add_texture(ArxLevel* level, const ArxTextureView* texture,
                                             ArxTextureIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!texture || !out_index || !pistoris::c_api::valid(*texture)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addTexture(*texture, *out_index); });
}

ArxReturnCode arx_pistoris_level_set_texture_image(ArxLevel* level, ArxTextureIndex index, const uint8_t* data,
                                                   size_t size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(data, size)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setTextureImage(index, {data, size}); });
}

ArxReturnCode arx_pistoris_level_clear_texture_image(ArxLevel* level, ArxTextureIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.clearTextureImage(index); });
}

ArxReturnCode arx_pistoris_level_set_face_room(ArxLevel* level, ArxFaceIndex face, ArxRoomIndex room) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.setFaceRoom(face, room); });
}

ArxReturnCode arx_pistoris_level_set_corner_color(ArxLevel* level, ArxFaceIndex face, uint32_t corner,
                                                  ArxColor3 color) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (corner > UINT8_MAX) return ARX_INDEX_OUT_OF_RANGE;
  return pistoris::c_api::guard(
      [&] { return level->value.setCornerColor(face, static_cast<std::uint8_t>(corner), color); });
}

ArxReturnCode arx_pistoris_level_clear_corner_colors(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    level->value.clearCornerColors();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_replace_mesh(ArxLevel* level, const ArxLevelMeshInput* mesh) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!mesh) return ARX_INVALID_DATA_POINTER;
  const bool counts_valid = validMeshCounts(*mesh);
  if (counts_valid && (!pistoris::c_api::valid(mesh->vertices, mesh->vertex_count) ||
                       !pistoris::c_api::valid(mesh->faces, mesh->face_count) ||
                       !pistoris::c_api::valid(mesh->textures, mesh->texture_count)))
    return ARX_INVALID_DATA_POINTER;
  if (counts_valid) {
    for (std::size_t i = 0; i < mesh->texture_count; ++i) {
      if (!pistoris::c_api::valid(mesh->textures[i])) return ARX_INVALID_DATA_POINTER;
    }
  }

  return pistoris::c_api::guard([&] { return level->value.replaceMesh(*mesh); });
}

ArxReturnCode arx_pistoris_level_clear_mesh(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    level->value.clearMesh();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_set_room(ArxLevel* level, ArxRoomIndex index, const ArxLevelRoom* room) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!room || !pistoris::c_api::valid(*room)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setRoom(index, *room); });
}

ArxReturnCode arx_pistoris_level_add_room(ArxLevel* level, const ArxLevelRoom* room, ArxRoomIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!room || !out_index || !pistoris::c_api::valid(*room)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addRoom(*room, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_room(ArxLevel* level, ArxRoomIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeRoom(index); });
}

ArxReturnCode arx_pistoris_level_set_portal(ArxLevel* level, ArxPortalIndex index,
                                            const ArxLevelPortal* portal) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!portal || !pistoris::c_api::valid(*portal)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setPortal(index, *portal); });
}

ArxReturnCode arx_pistoris_level_add_portal(ArxLevel* level, const ArxLevelPortal* portal,
                                            ArxPortalIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!portal || !out_index || !pistoris::c_api::valid(*portal)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addPortal(*portal, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_portal(ArxLevel* level, ArxPortalIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removePortal(index); });
}

ArxReturnCode arx_pistoris_level_flatten_portals(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.flattenPortals(); });
}

ArxReturnCode arx_pistoris_level_set_room_distance(ArxLevel* level, const ArxLevelRoomDistance* distance) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!distance) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setRoomDistance(*distance); });
}

ArxReturnCode arx_pistoris_level_replace_room_distances(ArxLevel* level, const ArxLevelRoomDistance* distances,
                                                        size_t count) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(distances, count)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.replaceRoomDistances(distances, count); });
}

ArxReturnCode arx_pistoris_level_clear_room_distances(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    level->value.clearRoomDistances();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_set_anchor(ArxLevel* level, ArxAnchorIndex index,
                                            const ArxLevelAnchor* anchor) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!anchor || !pistoris::c_api::valid(*anchor)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setAnchor(index, *anchor); });
}

ArxReturnCode arx_pistoris_level_add_anchor(ArxLevel* level, const ArxLevelAnchor* anchor,
                                            ArxAnchorIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!anchor || !out_index || !pistoris::c_api::valid(*anchor)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addAnchor(*anchor, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_anchor(ArxLevel* level, ArxAnchorIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeAnchor(index); });
}

ArxReturnCode arx_pistoris_level_set_anchor_connection(ArxLevel* level, ArxAnchorConnectionIndex index,
                                                       ArxLevelAnchorConnection connection) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.setAnchorConnection(index, connection); });
}

ArxReturnCode arx_pistoris_level_add_anchor_connection(ArxLevel* level, ArxLevelAnchorConnection connection,
                                                       ArxAnchorConnectionIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_index) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addAnchorConnection(connection, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_anchor_connection(ArxLevel* level, ArxAnchorConnectionIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeAnchorConnection(index); });
}

ArxReturnCode arx_pistoris_level_replace_anchors(ArxLevel* level, const ArxLevelAnchorsInput* anchors) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!anchors || !pistoris::c_api::valid(anchors->anchors, anchors->anchor_count) ||
      !pistoris::c_api::valid(anchors->connections, anchors->connection_count))
    return ARX_INVALID_DATA_POINTER;
  for (std::size_t i = 0; i < anchors->anchor_count; ++i) {
    if (!pistoris::c_api::valid(anchors->anchors[i])) return ARX_INVALID_DATA_POINTER;
  }

  return pistoris::c_api::guard([&] { return level->value.replaceAnchors(*anchors); });
}

ArxReturnCode arx_pistoris_level_clear_anchors(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    level->value.clearAnchors();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_set_nav_surface(ArxLevel* level, const ArxLevelNavSurfaceInput* surface) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!surface || !pistoris::c_api::valid(surface->vertices, surface->vertex_count) ||
      !pistoris::c_api::valid(surface->triangles, surface->triangle_count))
    return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setNavSurface(*surface); });
}

ArxReturnCode arx_pistoris_level_clear_nav_surface(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    level->value.clearNavSurface();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_set_light(ArxLevel* level, ArxLightIndex index, const ArxLevelLight* light) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!light || !pistoris::c_api::valid(*light)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setLight(index, *light); });
}

ArxReturnCode arx_pistoris_level_add_light(ArxLevel* level, const ArxLevelLight* light,
                                           ArxLightIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!light || !out_index || !pistoris::c_api::valid(*light)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addLight(*light, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_light(ArxLevel* level, ArxLightIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeLight(index); });
}

ArxReturnCode arx_pistoris_level_set_player_spawn(ArxLevel* level, const ArxLevelPlayerSpawn* spawn) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!spawn) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setPlayerSpawn(*spawn); });
}

ArxReturnCode arx_pistoris_level_clear_player_spawn(ArxLevel* level) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    level->value.clearPlayerSpawn();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_level_set_entity(ArxLevel* level, ArxEntityIndex index,
                                            const ArxLevelEntity* entity) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!entity || !pistoris::c_api::valid(*entity)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setEntity(index, *entity); });
}

ArxReturnCode arx_pistoris_level_add_entity(ArxLevel* level, const ArxLevelEntity* entity,
                                            ArxEntityIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!entity || !out_index || !pistoris::c_api::valid(*entity)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addEntity(*entity, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_entity(ArxLevel* level, ArxEntityIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeEntity(index); });
}

ArxReturnCode arx_pistoris_level_set_fog(ArxLevel* level, ArxFogIndex index, const ArxLevelFog* fog) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!fog || !pistoris::c_api::valid(*fog)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setFog(index, *fog); });
}

ArxReturnCode arx_pistoris_level_add_fog(ArxLevel* level, const ArxLevelFog* fog, ArxFogIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!fog || !out_index || !pistoris::c_api::valid(*fog)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addFog(*fog, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_fog(ArxLevel* level, ArxFogIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeFog(index); });
}

ArxReturnCode arx_pistoris_level_set_zone(ArxLevel* level, ArxZoneIndex index, const ArxLevelZoneInput* zone) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!zone || !pistoris::c_api::valid(*zone)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setZone(index, *zone); });
}

ArxReturnCode arx_pistoris_level_add_zone(ArxLevel* level, const ArxLevelZoneInput* zone,
                                          ArxZoneIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!zone || !out_index || !pistoris::c_api::valid(*zone)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addZone(*zone, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_zone(ArxLevel* level, ArxZoneIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removeZone(index); });
}

ArxReturnCode arx_pistoris_level_set_path(ArxLevel* level, ArxPathIndex index, const ArxLevelPathInput* path) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!path || !pistoris::c_api::valid(*path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return level->value.setPath(index, *path); });
}

ArxReturnCode arx_pistoris_level_add_path(ArxLevel* level, const ArxLevelPathInput* path,
                                          ArxPathIndex* out_index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!path || !out_index || !pistoris::c_api::valid(*path)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return level->value.addPath(*path, *out_index); });
}

ArxReturnCode arx_pistoris_level_remove_path(ArxLevel* level, ArxPathIndex index) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return level->value.removePath(index); });
}

// NOLINTEND(readability-identifier-naming)
