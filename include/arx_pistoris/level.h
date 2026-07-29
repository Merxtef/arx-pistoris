// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_LEVEL_H
#define ARX_PISTORIS_LEVEL_H

#include "arx_pistoris/level/types.h"
#include "arx_pistoris/native.h"

#include <stdint.h>

// Public C API: ABI-stable ARX_* / arx_pistoris_* naming and C-compatible enums
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef struct arx_pistoris_level ArxLevel;
typedef struct arx_pistoris_native_texture_files ArxNativeTextureFiles;

/* Input strings, byte views, and arrays are copied before a mutating call returns */
/* String and encoded-image views are borrowed and invalidated by any function taking non-const ArxLevel* */
/* Collection indices are current zero-based positions and have the same invalidation rule */
/* A successful add call returns an index valid for the resulting Level state */
/* Native texture-file views remain valid until their ArxNativeTextureFiles handle is destroyed */
/* uint8_t presence and option fields use zero for false and every nonzero value for true */

typedef uint32_t ArxLevelWeldMetric;
enum { ARX_LEVEL_WELD_EUCLIDEAN = 0, ARX_LEVEL_WELD_AXIS_ALIGNED = 1 };

typedef uint32_t ArxLevelDegenerateFacePolicy;
enum {
  ARX_LEVEL_DEGENERATE_FACE_PRESERVE = 0,
  ARX_LEVEL_DEGENERATE_FACE_REJECT = 1,
  ARX_LEVEL_DEGENERATE_FACE_DISCARD = 2
};

typedef uint32_t ArxNativeTexturePathMode;
enum { ARX_NATIVE_TEXTURE_PATH_PRESERVE = 0, ARX_NATIVE_TEXTURE_PATH_REBASE = 1 };

typedef struct ArxLevelVertexWeldOptions {
  float radius;
  ArxLevelWeldMetric metric;
  ArxLevelDegenerateFacePolicy degenerate_faces;
} ArxLevelVertexWeldOptions;

#define ARX_LEVEL_VERTEX_WELD_OPTIONS_INIT {1.0e-4f, ARX_LEVEL_WELD_EUCLIDEAN, ARX_LEVEL_DEGENERATE_FACE_PRESERVE}

typedef struct ArxLevelNavSurfaceSourceOptions {
  float clearance;
  float support_min_up_cos;
  ArxFaceType support_ignore_flags;
} ArxLevelNavSurfaceSourceOptions;

#define ARX_LEVEL_NAV_SURFACE_SOURCE_OPTIONS_INIT \
  {5.0f, 0.5881716976750462f, ARX_FACE_BIT_WATER | ARX_FACE_BIT_TRANS | ARX_FACE_BIT_NOCOL | ARX_FACE_BIT_LAVA}

typedef struct ArxLevelNavSurfaceGenOptions {
  float clearance;
  float support_min_up_cos;
  ArxFaceType support_ignore_flags;
  float radius;
  float height;
  float max_step_up;
} ArxLevelNavSurfaceGenOptions;

#define ARX_LEVEL_NAV_SURFACE_GEN_OPTIONS_INIT                                       \
  {5.0f,                                                                             \
   0.5881716976750462f,                                                              \
   ARX_FACE_BIT_WATER | ARX_FACE_BIT_TRANS | ARX_FACE_BIT_NOCOL | ARX_FACE_BIT_LAVA, \
   50.0f,                                                                            \
   -165.0f,                                                                          \
   55.0f}

typedef struct ArxLevelNavSurfacePruneOptions {
  float min_component_area_ratio;
  double min_component_area;
} ArxLevelNavSurfacePruneOptions;

#define ARX_LEVEL_NAV_SURFACE_PRUNE_OPTIONS_INIT {0.05f, 0.0}

typedef struct ArxLevelAnchorGenOptions {
  float sample_spacing;
  float radius;
  float height;
} ArxLevelAnchorGenOptions;

#define ARX_LEVEL_ANCHOR_GEN_OPTIONS_INIT {100.0f, 50.0f, -165.0f}

typedef struct ArxLevelAnchorPruneOptions {
  float min_component_anchor_ratio;
  uint32_t min_component_anchor_count;
} ArxLevelAnchorPruneOptions;

#define ARX_LEVEL_ANCHOR_PRUNE_OPTIONS_INIT {0.05f, 1U}

typedef struct ArxLevelAnchorConnectionGenOptions {
  float max_distance;
  float max_step_distance;
  float max_step_up;
  float radius_scale;
  int32_t max_steps;
} ArxLevelAnchorConnectionGenOptions;

#define ARX_LEVEL_ANCHOR_CONNECTION_GEN_OPTIONS_INIT {150.0f, 40.0f, 55.0f, 0.9f, 100}

typedef struct ArxLevelRoomDistanceGenOptions {
  float portal_side_offset;
  float sample_spacing;
  float sample_height_offset;
  float max_link_distance;
} ArxLevelRoomDistanceGenOptions;

#define ARX_LEVEL_ROOM_DISTANCE_GEN_OPTIONS_INIT {10.0f, 100.0f, 82.5f, 0.0f}

typedef struct ArxLevelStaticLightingGenOptions {
  ArxColor3 ambient_color;
  float global_factor;
  uint8_t use_normals;
  uint8_t use_shadows;
} ArxLevelStaticLightingGenOptions;

#define ARX_LEVEL_STATIC_LIGHTING_GEN_OPTIONS_INIT {{0.25f, 0.25f, 0.25f}, 0.85f, 1U, 1U}

typedef struct ArxLevelGlbImportOptions {
  float arx_units_per_glb_unit;
  uint8_t has_arx_offset;
  ArxVector3 arx_offset;
} ArxLevelGlbImportOptions;

#define ARX_LEVEL_GLB_IMPORT_OPTIONS_INIT {100.0f, 0U, {0.0f, 0.0f, 0.0f}}

typedef struct ArxLevelGlbExportOptions {
  float arx_units_per_glb_unit;
  ArxVector3 arx_offset;
} ArxLevelGlbExportOptions;

#define ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT {100.0f, {0.0f, 0.0f, 0.0f}}

typedef struct ArxLevelGlbImportInfo {
  ArxVector3 applied_arx_offset;
} ArxLevelGlbImportInfo;

typedef struct ArxLevelNativeBakeOptions {
  ArxStringView level_name;
  ArxStringView texture_folder;
  ArxNativeTexturePathMode texture_path_mode;
  uint8_t reconstruct_quads;
  uint8_t include_texture_files;
  ArxStringView dlf_scene_path;
} ArxLevelNativeBakeOptions;

#define ARX_LEVEL_NATIVE_BAKE_OPTIONS_INIT {{NULL, 0}, {NULL, 0}, ARX_NATIVE_TEXTURE_PATH_PRESERVE, 1U, 1U, {NULL, 0}}

typedef struct ArxLevelNativeDlfBakeOptions {
  ArxStringView level_name;
  ArxVector3 target_fts_offset;
  ArxStringView dlf_scene_path;
} ArxLevelNativeDlfBakeOptions;

#define ARX_LEVEL_NATIVE_DLF_BAKE_OPTIONS_INIT {{NULL, 0}, {0.0f, 0.0f, 0.0f}, {NULL, 0}}

typedef struct ArxNativeTextureFile {
  ArxTextureIndex source_texture;
  ArxStringView resource_path;
  ArxEncodedImageView encoded_image;
} ArxNativeTextureFile;

ARX_EXTERN_C_BEGIN

ARX_API ArxReturnCode arx_pistoris_level_create(ArxLevel** out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clone(const ArxLevel* level, ArxLevel** out_level) ARX_NOEXCEPT;
ARX_API void arx_pistoris_level_destroy(ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_reset(ArxLevel* level) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_from_native(const ArxFts* fts, const ArxLlf* llf, const ArxDlf* dlf,
                                                     ArxLevel** out_level) ARX_NOEXCEPT;
/* NULL GLB, welding, and Level-generation option pointers select the corresponding C++ Level defaults */
ARX_API ArxReturnCode arx_pistoris_level_from_glb(const uint8_t* data, size_t size,
                                                  const ArxLevelGlbImportOptions* options,
                                                  ArxLevelGlbImportInfo* out_info, ArxLevel** out_level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_export_glb(const ArxLevel* level, const ArxLevelGlbExportOptions* options,
                                                    uint8_t** out_data, size_t* out_size) ARX_NOEXCEPT;
/* Native bake option pointers and all native bundle output slots are required */
ARX_API ArxReturnCode arx_pistoris_level_bake_native(const ArxLevel* level, const ArxLevelNativeBakeOptions* options,
                                                     ArxFts** out_fts, ArxLlf** out_llf, ArxDlf** out_dlf,
                                                     ArxNativeTextureFiles** out_textures) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_bake_dlf(const ArxLevel* level, const ArxLevelNativeDlfBakeOptions* options,
                                                  ArxDlf** out_dlf) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_native_texture_files_count(const ArxNativeTextureFiles* files,
                                                              size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_native_texture_files_get(const ArxNativeTextureFiles* files, size_t index,
                                                            ArxNativeTextureFile* out_file) ARX_NOEXCEPT;
ARX_API void arx_pistoris_native_texture_files_destroy(ArxNativeTextureFiles* files) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_validate(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_mesh(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_vertices(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_textures(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_faces(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_face_rooms(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_corner_colors(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_rooms(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_portals(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_room_distances(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_nav_surface(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_anchors(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_anchor_connections(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_lights(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_player_spawn(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_entities(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_fogs(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_zones(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_validate_paths(const ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_bounds(const ArxLevel* level, ArxAabb* out_bounds) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_referenced_bounds(const ArxLevel* level, ArxAabb* out_bounds) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_vertex_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_face_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_texture_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_room_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_portal_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_room_distance_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_anchor_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_anchor_connection_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_light_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_entity_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_fog_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_zone_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_path_count(const ArxLevel* level, size_t* out_count) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_copy_vertices(const ArxLevel* level, size_t offset, size_t count,
                                                       ArxLevelVertex* out_vertices) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_faces(const ArxLevel* level, size_t offset, size_t count,
                                                    ArxLevelFace* out_faces) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_texture_views(const ArxLevel* level, size_t offset, size_t count,
                                                            ArxLevelTextureView* out_views) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_rooms(const ArxLevel* level, size_t offset, size_t count,
                                                    ArxLevelRoom* out_rooms) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_portals(const ArxLevel* level, size_t offset, size_t count,
                                                      ArxLevelPortal* out_portals) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_room_distances(const ArxLevel* level, size_t offset, size_t count,
                                                             ArxLevelRoomDistance* out_distances) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_anchors(const ArxLevel* level, size_t offset, size_t count,
                                                      ArxLevelAnchor* out_anchors) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_anchor_connections(
    const ArxLevel* level, size_t offset, size_t count, ArxLevelAnchorConnection* out_connections) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_get_nav_surface_info(const ArxLevel* level,
                                                              ArxLevelNavSurfaceInfo* out_info) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_nav_surface_vertices(const ArxLevel* level, size_t offset, size_t count,
                                                                   ArxLevelVertex* out_vertices) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_nav_surface_triangles(
    const ArxLevel* level, size_t offset, size_t count, ArxLevelNavSurfaceTriangle* out_triangles) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_lights(const ArxLevel* level, size_t offset, size_t count,
                                                     ArxLevelLight* out_lights) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_get_player_spawn(const ArxLevel* level,
                                                          ArxLevelPlayerSpawn* out_spawn) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_entities(const ArxLevel* level, size_t offset, size_t count,
                                                       ArxLevelEntity* out_entities) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_fogs(const ArxLevel* level, size_t offset, size_t count,
                                                   ArxLevelFog* out_fogs) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_zones(const ArxLevel* level, size_t offset, size_t count,
                                                    ArxLevelZone* out_zones) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_zone_perimeter(const ArxLevel* level, ArxZoneIndex zone, size_t offset,
                                                             size_t count, ArxVector2* out_points) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_paths(const ArxLevel* level, size_t offset, size_t count,
                                                    ArxLevelPath* out_paths) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_copy_path_nodes(const ArxLevel* level, ArxPathIndex path, size_t offset,
                                                         size_t count, ArxLevelPathNode* out_nodes) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_get_room_distance(const ArxLevel* level, ArxRoomIndex room_a,
                                                           ArxRoomIndex room_b, uint8_t* out_has_distance,
                                                           ArxLevelRoomDistance* out_distance) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_set_vertex(ArxLevel* level, ArxVertexIndex index,
                                                    ArxLevelVertex vertex) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_vertex(ArxLevel* level, ArxLevelVertex vertex,
                                                    ArxVertexIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_vertices(ArxLevel* level, const ArxLevelVertex* vertices, size_t count,
                                                      ArxVertexIndex* out_first_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_face(ArxLevel* level, ArxFaceIndex index,
                                                  const ArxLevelFace* face) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_face(ArxLevel* level, const ArxLevelFace* face,
                                                  ArxFaceIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_face(ArxLevel* level, ArxFaceIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_compact_vertices(ArxLevel* level, size_t* out_removed) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_compact_textures(ArxLevel* level, size_t* out_removed) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_weld_vertices(ArxLevel* level,
                                                       const ArxLevelVertexWeldOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_texture(ArxLevel* level, ArxTextureIndex index,
                                                     const ArxLevelTextureView* texture) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_texture(ArxLevel* level, const ArxLevelTextureView* texture,
                                                     ArxTextureIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_texture_image(ArxLevel* level, ArxTextureIndex index, const uint8_t* data,
                                                           size_t size) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_texture_image(ArxLevel* level, ArxTextureIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_face_room(ArxLevel* level, ArxFaceIndex face,
                                                       ArxRoomIndex room) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_corner_color(ArxLevel* level, ArxFaceIndex face, uint32_t corner,
                                                          ArxColor3 color) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_corner_colors(ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_replace_mesh(ArxLevel* level, const ArxLevelMeshInput* mesh) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_mesh(ArxLevel* level) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_set_room(ArxLevel* level, ArxRoomIndex index,
                                                  const ArxLevelRoom* room) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_room(ArxLevel* level, const ArxLevelRoom* room,
                                                  ArxRoomIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_room(ArxLevel* level, ArxRoomIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_portal(ArxLevel* level, ArxPortalIndex index,
                                                    const ArxLevelPortal* portal) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_portal(ArxLevel* level, const ArxLevelPortal* portal,
                                                    ArxPortalIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_portal(ArxLevel* level, ArxPortalIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_room_distance(ArxLevel* level,
                                                           const ArxLevelRoomDistance* distance) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_replace_room_distances(ArxLevel* level, const ArxLevelRoomDistance* distances,
                                                                size_t count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_room_distances(ArxLevel* level) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_set_anchor(ArxLevel* level, ArxAnchorIndex index,
                                                    const ArxLevelAnchor* anchor) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_anchor(ArxLevel* level, const ArxLevelAnchor* anchor,
                                                    ArxAnchorIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_anchor(ArxLevel* level, ArxAnchorIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_anchor_connection(ArxLevel* level, ArxAnchorConnectionIndex index,
                                                               ArxLevelAnchorConnection connection) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_anchor_connection(ArxLevel* level, ArxLevelAnchorConnection connection,
                                                               ArxAnchorConnectionIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_anchor_connection(ArxLevel* level,
                                                                  ArxAnchorConnectionIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_replace_anchors(ArxLevel* level,
                                                         const ArxLevelAnchorsInput* anchors) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_anchors(ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_nav_surface(ArxLevel* level,
                                                         const ArxLevelNavSurfaceInput* surface) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_nav_surface(ArxLevel* level) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_set_light(ArxLevel* level, ArxLightIndex index,
                                                   const ArxLevelLight* light) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_light(ArxLevel* level, const ArxLevelLight* light,
                                                   ArxLightIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_light(ArxLevel* level, ArxLightIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_player_spawn(ArxLevel* level,
                                                          const ArxLevelPlayerSpawn* spawn) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_clear_player_spawn(ArxLevel* level) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_entity(ArxLevel* level, ArxEntityIndex index,
                                                    const ArxLevelEntity* entity) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_entity(ArxLevel* level, const ArxLevelEntity* entity,
                                                    ArxEntityIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_entity(ArxLevel* level, ArxEntityIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_fog(ArxLevel* level, ArxFogIndex index,
                                                 const ArxLevelFog* fog) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_fog(ArxLevel* level, const ArxLevelFog* fog,
                                                 ArxFogIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_fog(ArxLevel* level, ArxFogIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_zone(ArxLevel* level, ArxZoneIndex index,
                                                  const ArxLevelZoneInput* zone) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_zone(ArxLevel* level, const ArxLevelZoneInput* zone,
                                                  ArxZoneIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_zone(ArxLevel* level, ArxZoneIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_path(ArxLevel* level, ArxPathIndex index,
                                                  const ArxLevelPathInput* path) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_add_path(ArxLevel* level, const ArxLevelPathInput* path,
                                                  ArxPathIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_remove_path(ArxLevel* level, ArxPathIndex index) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_level_generate_nav_surface(ArxLevel* level,
                                                              const ArxLevelNavSurfaceGenOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_set_nav_surface_from_floor(
    ArxLevel* level, const ArxLevelNavSurfaceSourceOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_prune_nav_surface_islands(
    ArxLevel* level, const ArxLevelNavSurfacePruneOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_generate_anchors(ArxLevel* level,
                                                          const ArxLevelAnchorGenOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_generate_anchor_connections(
    ArxLevel* level, const ArxLevelAnchorConnectionGenOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_prune_anchor_islands(ArxLevel* level,
                                                              const ArxLevelAnchorPruneOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode
arx_pistoris_level_generate_room_distances(ArxLevel* level, const ArxLevelRoomDistanceGenOptions* options) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_generate_static_lighting(
    ArxLevel* level, const ArxLevelStaticLightingGenOptions* options) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_LEVEL_H */
