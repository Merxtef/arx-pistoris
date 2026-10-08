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
#include "api/c/level/internal.h"
#include "api/c/texture/internal.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace {

bool validWeldOptions(const ArxLevelVertexWeldOptions& options) noexcept {
  const bool valid_metric = options.metric == ARX_LEVEL_WELD_EUCLIDEAN || options.metric == ARX_LEVEL_WELD_AXIS_ALIGNED;
  const bool valid_policy = options.degenerate_faces == ARX_LEVEL_DEGENERATE_FACE_PRESERVE ||
                            options.degenerate_faces == ARX_LEVEL_DEGENERATE_FACE_REJECT ||
                            options.degenerate_faces == ARX_LEVEL_DEGENERATE_FACE_DISCARD;
  return valid_metric && valid_policy;
}

template <class Result, class T>
ArxReturnCode publishValue(Result&& result, T& out, ArxError* error) {
  if (!result) return pistoris::c_api::publish(result, error);
  out = *result;
  return pistoris::c_api::publish(result, error);
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_set_resource_path(ArxLevel* level, ArxStringView path, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.setResourcePath(pistoris::c_api::stringView(path)), error);
  });
}

ArxReturnCode arx_pistoris_level_set_minimap(ArxLevel* level, ArxEncodedImageView encoded_image,
                                             ArxRect world_xz_bounds, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(encoded_image)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setMinimap(encoded_image, world_xz_bounds), error); });
}

ArxReturnCode arx_pistoris_level_set_minimap_from_projection(ArxLevel* level, ArxEncodedImageView encoded_image,
                                                             ArxVector2 projection_offset, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(encoded_image)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.setMinimapFromProjection(encoded_image, projection_offset), error);
  });
}

ArxReturnCode arx_pistoris_level_clear_minimap(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearMinimap();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_set_loading_screen(ArxLevel* level, ArxEncodedImageView encoded_image,
                                                    ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(encoded_image)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setLoadingScreen(encoded_image), error); });
}

ArxReturnCode arx_pistoris_level_clear_loading_screen(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearLoadingScreen();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_set_vertex(ArxLevel* level, ArxVertexIndex index, ArxLevelVertex vertex,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setVertex(index, vertex), error); });
}

ArxReturnCode arx_pistoris_level_add_vertex(ArxLevel* level, ArxLevelVertex vertex, ArxVertexIndex* out_index,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addVertex(vertex), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_add_vertices(ArxLevel* level, const ArxLevelVertex* vertices, size_t count,
                                              ArxVertexIndex* out_first_index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_first_index || !pistoris::c_api::valid(vertices, count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_first_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] {
    return publishValue(level->value.addVertices(std::span(vertices, count)), *out_first_index, error);
  });
}

ArxReturnCode arx_pistoris_level_set_face(ArxLevel* level, ArxFaceIndex index, const ArxLevelFace* face,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!face) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setFace(index, *face), error); });
}

ArxReturnCode arx_pistoris_level_add_face(ArxLevel* level, const ArxLevelFace* face, ArxFaceIndex* out_index,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!face || !out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addFace(*face), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_face(ArxLevel* level, ArxFaceIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] { return pistoris::c_api::publish(level->value.removeFace(index), error); });
}

ArxReturnCode arx_pistoris_level_compact_vertices(ArxLevel* level, size_t* out_removed, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_removed) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_removed = 0;
  return pistoris::c_api::guard(error,
                                [&] { return publishValue(level->value.compactVertices(), *out_removed, error); });
}

ArxReturnCode arx_pistoris_level_compact_textures(ArxLevel* level, size_t* out_removed, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_removed) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_removed = 0;
  return pistoris::c_api::guard(error,
                                [&] { return publishValue(level->value.compactTextures(), *out_removed, error); });
}

ArxReturnCode arx_pistoris_level_rebase_texture_paths(ArxLevel* level, ArxStringView directory,
                                                      ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(directory)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.rebaseTexturePaths(pistoris::c_api::stringView(directory)), error);
  });
}

ArxReturnCode arx_pistoris_level_weld_vertices(ArxLevel* level, const ArxLevelVertexWeldOptions* options,
                                               ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (options && !validWeldOptions(*options)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.weldVertices(), error);
    return pistoris::c_api::publish(level->value.weldVertices(pistoris::c_api::weldOptions(*options)), error);
  });
}

ArxReturnCode arx_pistoris_level_snap_geometry_to_portals(ArxLevel* level, const ArxLevelPortalSnapOptions* options,
                                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.snapGeometryToPortals(), error);
    return pistoris::c_api::publish(level->value.snapGeometryToPortals(pistoris::c_api::portalSnapOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_set_texture(ArxLevel* level, ArxTextureIndex index, const ArxTextureView* texture,
                                             ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!texture || !pistoris::c_api::valid(*texture))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setTexture(index, *texture), error); });
}

ArxReturnCode arx_pistoris_level_add_texture(ArxLevel* level, const ArxTextureView* texture, ArxTextureIndex* out_index,
                                             ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!texture || !out_index || !pistoris::c_api::valid(*texture))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error,
                                [&] { return publishValue(level->value.addTexture(*texture), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_set_texture_path(ArxLevel* level, ArxTextureIndex index, ArxStringView path,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.setTexturePath(index, pistoris::c_api::stringView(path)), error);
  });
}

ArxReturnCode arx_pistoris_level_set_texture_external_image_extension(ArxLevel* level, ArxTextureIndex index,
                                                                      ArxStringView extension,
                                                                      ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(extension)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(
        level->value.setTextureExternalImageExtension(index, pistoris::c_api::stringView(extension)), error);
  });
}

ArxReturnCode arx_pistoris_level_set_texture_image(ArxLevel* level, ArxTextureIndex index, const uint8_t* data,
                                                   size_t size, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(data, size)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setTextureImage(index, {data, size}), error); });
}

ArxReturnCode arx_pistoris_level_clear_texture_image(ArxLevel* level, ArxTextureIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.clearTextureImage(index), error); });
}

ArxReturnCode arx_pistoris_level_set_face_room(ArxLevel* level, ArxFaceIndex face, ArxRoomIndex room,
                                               ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setFaceRoom(face, room), error); });
}

ArxReturnCode arx_pistoris_level_set_corner_color(ArxLevel* level, ArxFaceIndex face, uint32_t corner, ArxColor3 color,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (corner > UINT8_MAX) return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.setCornerColor(face, static_cast<std::uint8_t>(corner), color), error);
  });
}

ArxReturnCode arx_pistoris_level_reset_corner_colors(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    level->value.resetCornerColors();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_replace_vertices(ArxLevel* level, const float* positions, size_t position_count,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(positions, position_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.replaceVertices(std::span(positions, position_count)), error);
  });
}

ArxReturnCode arx_pistoris_level_clear_vertices(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    level->value.clearVertices();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_replace_faces(ArxLevel* level, const ArxLevelFacesInput* faces,
                                               ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!faces || !pistoris::c_api::valid(faces->vertex_indices, faces->vertex_index_count) ||
      !pistoris::c_api::valid(faces->uvs, faces->uv_count) ||
      !pistoris::c_api::valid(faces->corner_normals, faces->corner_normal_count) ||
      !pistoris::c_api::valid(faces->textures, faces->texture_count) ||
      !pistoris::c_api::valid(faces->transvals, faces->transval_count) ||
      !pistoris::c_api::valid(faces->corner_colors, faces->corner_color_count) ||
      !pistoris::c_api::valid(faces->face_normals, faces->face_normal_count) ||
      !pistoris::c_api::valid(faces->flags, faces->flag_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    const auto result = level->value.replaceFaces(std::span(faces->vertex_indices, faces->vertex_index_count),
                                                  std::span(faces->uvs, faces->uv_count),
                                                  std::span(faces->corner_normals, faces->corner_normal_count),
                                                  std::span(faces->textures, faces->texture_count),
                                                  std::span(faces->transvals, faces->transval_count),
                                                  std::span(faces->corner_colors, faces->corner_color_count),
                                                  std::span(faces->face_normals, faces->face_normal_count),
                                                  std::span(faces->flags, faces->flag_count));
    return pistoris::c_api::publish(result, error);
  });
}

ArxReturnCode arx_pistoris_level_clear_faces(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearFaces();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_clear_textures(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearTextures();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_replace_face_textures(ArxLevel* level, const ArxTextureIndex* textures, size_t count,
                                                       ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(textures, count)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.replaceFaceTextures(std::span(textures, count)), error);
  });
}

ArxReturnCode arx_pistoris_level_replace_face_rooms(ArxLevel* level, const ArxRoomIndex* rooms, size_t count,
                                                    ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(rooms, count)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.replaceFaceRooms(std::span(rooms, count)), error); });
}

ArxReturnCode arx_pistoris_level_set_room(ArxLevel* level, ArxRoomIndex index, const ArxLevelRoom* room,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!room || !pistoris::c_api::valid(*room)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setRoom(index, *room), error); });
}

ArxReturnCode arx_pistoris_level_add_room(ArxLevel* level, const ArxLevelRoom* room, ArxRoomIndex* out_index,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!room || !out_index || !pistoris::c_api::valid(*room))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addRoom(*room), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_room(ArxLevel* level, ArxRoomIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] { return pistoris::c_api::publish(level->value.removeRoom(index), error); });
}

ArxReturnCode arx_pistoris_level_clear_rooms(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearRooms();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_set_portal(ArxLevel* level, ArxPortalIndex index, const ArxLevelPortal* portal,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!portal || !pistoris::c_api::valid(*portal)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setPortal(index, *portal), error); });
}

ArxReturnCode arx_pistoris_level_add_portal(ArxLevel* level, const ArxLevelPortal* portal, ArxPortalIndex* out_index,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!portal || !out_index || !pistoris::c_api::valid(*portal))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error,
                                [&] { return publishValue(level->value.addPortal(*portal), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_portal(ArxLevel* level, ArxPortalIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.removePortal(index), error); });
}

ArxReturnCode arx_pistoris_level_flatten_portals(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] { return pistoris::c_api::publish(level->value.flattenPortals(), error); });
}

ArxReturnCode arx_pistoris_level_clear_portals(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearPortals();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_set_room_distance(ArxLevel* level, const ArxLevelRoomDistance* distance,
                                                   ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!distance) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setRoomDistance(*distance), error); });
}

ArxReturnCode arx_pistoris_level_replace_room_distances(ArxLevel* level, const float* distances, size_t distance_count,
                                                        const ArxPortalIndex* endpoint_portals,
                                                        size_t endpoint_portal_count, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(distances, distance_count) ||
      !pistoris::c_api::valid(endpoint_portals, endpoint_portal_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(
        level->value.replaceRoomDistances(std::span(distances, distance_count),
                                          std::span(endpoint_portals, endpoint_portal_count)),
        error);
  });
}

ArxReturnCode arx_pistoris_level_clear_room_distances(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    level->value.clearRoomDistances();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_set_anchor(ArxLevel* level, ArxAnchorIndex index, const ArxLevelAnchor* anchor,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!anchor || !pistoris::c_api::valid(*anchor)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setAnchor(index, *anchor), error); });
}

ArxReturnCode arx_pistoris_level_add_anchor(ArxLevel* level, const ArxLevelAnchor* anchor, ArxAnchorIndex* out_index,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!anchor || !out_index || !pistoris::c_api::valid(*anchor))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error,
                                [&] { return publishValue(level->value.addAnchor(*anchor), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_anchor(ArxLevel* level, ArxAnchorIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.removeAnchor(index), error); });
}

ArxReturnCode arx_pistoris_level_set_anchor_connection(ArxLevel* level, ArxAnchorConnectionIndex index,
                                                       ArxLevelAnchorConnection connection, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setAnchorConnection(index, connection), error); });
}

ArxReturnCode arx_pistoris_level_add_anchor_connection(ArxLevel* level, ArxLevelAnchorConnection connection,
                                                       ArxAnchorConnectionIndex* out_index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(
      error, [&] { return publishValue(level->value.addAnchorConnection(connection), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_anchor_connection(ArxLevel* level, ArxAnchorConnectionIndex index,
                                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.removeAnchorConnection(index), error); });
}

ArxReturnCode arx_pistoris_level_replace_anchors(ArxLevel* level, const ArxLevelAnchorsInput* anchors,
                                                 ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!anchors || !pistoris::c_api::valid(anchors->positions, anchors->position_count) ||
      !pistoris::c_api::valid(anchors->radii, anchors->radius_count) ||
      !pistoris::c_api::valid(anchors->heights, anchors->height_count) ||
      !pistoris::c_api::valid(anchors->flags, anchors->flag_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.replaceAnchors(std::span(anchors->positions, anchors->position_count),
                                                                std::span(anchors->radii, anchors->radius_count),
                                                                std::span(anchors->heights, anchors->height_count),
                                                                std::span(anchors->flags, anchors->flag_count)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_replace_anchor_connections(ArxLevel* level, const ArxAnchorIndex* endpoints,
                                                            size_t endpoint_count, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(endpoints, endpoint_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(level->value.replaceAnchorConnections(std::span(endpoints, endpoint_count)), error);
  });
}

ArxReturnCode arx_pistoris_level_clear_anchor_connections(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  level->value.clearAnchorConnections();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_level_clear_anchors(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    level->value.clearAnchors();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_set_nav_surface(ArxLevel* level, const ArxLevelNavSurfaceInput* surface,
                                                 ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!surface || !pistoris::c_api::valid(surface->positions, surface->position_count) ||
      !pistoris::c_api::valid(surface->triangle_indices, surface->triangle_index_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(
        level->value.setNavSurface(std::span(surface->positions, surface->position_count),
                                   std::span(surface->triangle_indices, surface->triangle_index_count)),
        error);
  });
}

ArxReturnCode arx_pistoris_level_clear_nav_surface(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    level->value.clearNavSurface();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_set_light(ArxLevel* level, ArxLightIndex index, const ArxLevelLight* light,
                                           ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!light || !pistoris::c_api::valid(*light)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setLight(index, *light), error); });
}

ArxReturnCode arx_pistoris_level_add_light(ArxLevel* level, const ArxLevelLight* light, ArxLightIndex* out_index,
                                           ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!light || !out_index || !pistoris::c_api::valid(*light))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addLight(*light), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_light(ArxLevel* level, ArxLightIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.removeLight(index), error); });
}

ArxReturnCode arx_pistoris_level_set_player_spawn(ArxLevel* level, const ArxLevelPlayerSpawn* spawn,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!spawn) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setPlayerSpawn(*spawn), error); });
}

ArxReturnCode arx_pistoris_level_clear_player_spawn(ArxLevel* level, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    level->value.clearPlayerSpawn();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_level_set_entity(ArxLevel* level, ArxEntityIndex index, const ArxLevelEntity* entity,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!entity || !pistoris::c_api::valid(*entity)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(
      error, [&] { return pistoris::c_api::publish(level->value.setEntity(index, *entity), error); });
}

ArxReturnCode arx_pistoris_level_add_entity(ArxLevel* level, const ArxLevelEntity* entity, ArxEntityIndex* out_index,
                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!entity || !out_index || !pistoris::c_api::valid(*entity))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error,
                                [&] { return publishValue(level->value.addEntity(*entity), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_entity(ArxLevel* level, ArxEntityIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.removeEntity(index), error); });
}

ArxReturnCode arx_pistoris_level_set_fog(ArxLevel* level, ArxFogIndex index, const ArxLevelFog* fog,
                                         ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!fog || !pistoris::c_api::valid(*fog)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setFog(index, *fog), error); });
}

ArxReturnCode arx_pistoris_level_add_fog(ArxLevel* level, const ArxLevelFog* fog, ArxFogIndex* out_index,
                                         ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!fog || !out_index || !pistoris::c_api::valid(*fog))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addFog(*fog), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_fog(ArxLevel* level, ArxFogIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] { return pistoris::c_api::publish(level->value.removeFog(index), error); });
}

ArxReturnCode arx_pistoris_level_set_zone(ArxLevel* level, ArxZoneIndex index, const ArxLevelZoneInput* zone,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!zone || !pistoris::c_api::valid(*zone)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setZone(index, *zone), error); });
}

ArxReturnCode arx_pistoris_level_add_zone(ArxLevel* level, const ArxLevelZoneInput* zone, ArxZoneIndex* out_index,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!zone || !out_index || !pistoris::c_api::valid(*zone))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addZone(*zone), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_zone(ArxLevel* level, ArxZoneIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] { return pistoris::c_api::publish(level->value.removeZone(index), error); });
}

ArxReturnCode arx_pistoris_level_set_path(ArxLevel* level, ArxPathIndex index, const ArxLevelPathInput* path,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!path || !pistoris::c_api::valid(*path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error,
                                [&] { return pistoris::c_api::publish(level->value.setPath(index, *path), error); });
}

ArxReturnCode arx_pistoris_level_add_path(ArxLevel* level, const ArxLevelPathInput* path, ArxPathIndex* out_index,
                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!path || !out_index || !pistoris::c_api::valid(*path))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard(error, [&] { return publishValue(level->value.addPath(*path), *out_index, error); });
}

ArxReturnCode arx_pistoris_level_remove_path(ArxLevel* level, ArxPathIndex index, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] { return pistoris::c_api::publish(level->value.removePath(index), error); });
}

// NOLINTEND(readability-identifier-naming)
