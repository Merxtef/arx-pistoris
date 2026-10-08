// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_MODEL_H
#define ARX_PISTORIS_MODEL_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/native/text.h"

#include <stddef.h>
#include <stdint.h>

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef struct arx_pistoris_ftl ArxFtl;
typedef struct arx_pistoris_animation ArxAnimation;
typedef struct arx_pistoris_animation_list ArxAnimationList;
typedef struct arx_pistoris_animation_sound_files ArxAnimationSoundFiles;
typedef struct arx_pistoris_animation_sound_source_references ArxAnimationSoundSourceReferences;
typedef struct arx_pistoris_model ArxModel;
typedef struct arx_pistoris_native_texture_files ArxNativeTextureFiles;
typedef struct arx_pistoris_obj_material_library_paths ArxObjMaterialLibraryPaths;
typedef struct arx_pistoris_obj_texture_files ArxObjTextureFiles;
typedef struct arx_pistoris_texture_source_paths ArxTextureSourcePaths;
typedef struct ArxAnimationConversionReport ArxAnimationConversionReport;

typedef struct ArxNativeModelBakeOptions {
  uint8_t include_texture_files;
  ArxNativeTextMode text_mode;
} ArxNativeModelBakeOptions;

#define ARX_NATIVE_MODEL_BAKE_OPTIONS_INIT {1U, ARX_NATIVE_TEXT_AUTO}

typedef struct ArxObjMaterialLibraryView {
  ArxStringView path;
  const uint8_t* data;
  size_t size;
} ArxObjMaterialLibraryView;

typedef struct ArxObjExportOptions {
  // Include encoded sidecars in output bundle
  uint8_t include_files;
} ArxObjExportOptions;
#define ARX_OBJ_EXPORT_OPTIONS_INIT {1U}

typedef struct ArxObjTextureFile {
  ArxTextureIndex source_texture;
  ArxStringView path;
  ArxEncodedImageView encoded_image;
} ArxObjTextureFile;

typedef struct ArxModelGlbImportOptions {
  // Range [1, 1000]
  float arx_units_per_glb_unit;
} ArxModelGlbImportOptions;
#define ARX_MODEL_GLB_IMPORT_OPTIONS_INIT {10.0f}

typedef struct ArxModelGlbExportOptions {
  // Range [1, 1000]
  float arx_units_per_glb_unit;
} ArxModelGlbExportOptions;
#define ARX_MODEL_GLB_EXPORT_OPTIONS_INIT {10.0f}

typedef struct ArxModelLevelPreviewGlbOptions {
  // Range [1, 1000]
  float arx_units_per_glb_unit;
  // Empty exports literal <class-path>
  ArxStringView class_path;
  // Empty exports literal asset
  ArxStringView asset_name;
} ArxModelLevelPreviewGlbOptions;
#define ARX_MODEL_LEVEL_PREVIEW_GLB_OPTIONS_INIT {100.0f, {NULL, 0}, {NULL, 0}}

// Enable at least one operation
typedef struct ArxModelReferenceOptions {
  // Copy exact bone positions by index
  uint8_t snap_bone_positions;
  // Replace bone selection memberships by selection name
  uint8_t copy_bone_selection_memberships;
  // Replace action-point memberships by name and occurrence
  uint8_t copy_action_point_selections;
} ArxModelReferenceOptions;
#define ARX_MODEL_REFERENCE_OPTIONS_INIT {0U, 0U, 0U}

typedef uint32_t ArxModelWeldMetric;
enum { ARX_MODEL_WELD_EUCLIDEAN = 0, ARX_MODEL_WELD_AXIS_ALIGNED = 1 };

typedef uint32_t ArxModelDegenerateFacePolicy;
enum {
  ARX_MODEL_DEGENERATE_FACE_PRESERVE = 0,
  ARX_MODEL_DEGENERATE_FACE_REJECT = 1,
  ARX_MODEL_DEGENERATE_FACE_DISCARD = 2
};

typedef struct ArxModelVertexWeldOptions {
  // Positive weld tolerance
  float radius;
  ArxModelWeldMetric metric;
  ArxModelDegenerateFacePolicy degenerate_faces;
} ArxModelVertexWeldOptions;

#define ARX_MODEL_VERTEX_WELD_OPTIONS_INIT {1.0e-4f, ARX_MODEL_WELD_EUCLIDEAN, ARX_MODEL_DEGENERATE_FACE_PRESERVE}

typedef struct ArxModelInventoryIconView {
  ArxEncodedImageView encoded_image;
  uint8_t width_slots;
  uint8_t height_slots;
} ArxModelInventoryIconView;
#define ARX_MODEL_INVENTORY_ICON_VIEW_INIT {{NULL, 0}, 0U, 0U}

typedef struct ArxModelInventoryIconSetOptions {
  // Zero derives, range [1, 3] explicit
  uint8_t width_slots;
  uint8_t height_slots;
} ArxModelInventoryIconSetOptions;
#define ARX_MODEL_INVENTORY_ICON_SET_OPTIONS_INIT {0U, 0U}

typedef uint8_t ArxModelInventoryIconLayout;
enum {
  ARX_MODEL_INVENTORY_ICON_LAYOUT_CENTER = 0,
  ARX_MODEL_INVENTORY_ICON_LAYOUT_TOP_LEFT,
  ARX_MODEL_INVENTORY_ICON_LAYOUT_TOP_RIGHT,
  ARX_MODEL_INVENTORY_ICON_LAYOUT_BOTTOM_LEFT,
  ARX_MODEL_INVENTORY_ICON_LAYOUT_BOTTOM_RIGHT,
  ARX_MODEL_INVENTORY_ICON_LAYOUT_STRETCH,
};

typedef struct ArxModelInventoryIconRenderOptions {
  // Both zero use the stored footprint; one zero derives from the explicit dimension
  uint8_t width_slots;
  uint8_t height_slots;
  ArxModelInventoryIconLayout layout;
  ArxImageFormat format;
} ArxModelInventoryIconRenderOptions;
#define ARX_MODEL_INVENTORY_ICON_RENDER_OPTIONS_INIT \
  {0U, 0U, ARX_MODEL_INVENTORY_ICON_LAYOUT_CENTER, ARX_IMAGE_FORMAT_PNG}

/*
 * Input string, image, and array storage required only for call duration
 * Functions taking ArxModel* invalidate collection indices and borrowed views
 * Selection IDs remain stable within the current Model state until that selection is removed
 */

ARX_EXTERN_C_BEGIN

// --- Lifetime ---

ARX_API ArxReturnCode arx_pistoris_model_create(ArxModel** out_model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clone(const ArxModel* model, ArxModel** out_model,
                                               ArxError* error) ARX_NOEXCEPT;
ARX_API void arx_pistoris_model_destroy(ArxModel* model) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_reset(ArxModel* model, ArxError* error) ARX_NOEXCEPT;

// --- Conversion ---

ARX_API ArxReturnCode arx_pistoris_model_import_native(const ArxFtl* native, ArxModel** out_model,
                                                       ArxTextureSourcePaths** out_texture_source_paths,
                                                       ArxNativeTextMode text_mode, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_obj_material_library_paths(const uint8_t* obj_data, size_t obj_size,
                                                              ArxObjMaterialLibraryPaths** out_paths,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_obj_material_library_paths_count(const ArxObjMaterialLibraryPaths* paths,
                                                                    size_t* out_count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_obj_material_library_paths_get(const ArxObjMaterialLibraryPaths* paths, size_t index,
                                                                  ArxStringView* out_path,
                                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API void arx_pistoris_obj_material_library_paths_destroy(ArxObjMaterialLibraryPaths* paths) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_import_obj(const uint8_t* obj_data, size_t obj_size,
                                                    const ArxObjMaterialLibraryView* material_libraries,
                                                    size_t material_library_count, ArxModel** out_model,
                                                    ArxTextureSourcePaths** out_texture_source_paths,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_export_obj(const ArxModel* model, ArxStringView stem,
                                                    const ArxObjExportOptions* options, char** out_obj, char** out_mtl,
                                                    ArxObjTextureFiles** out_textures, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_obj_texture_files_count(const ArxObjTextureFiles* files, size_t* out_count,
                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_obj_texture_files_get(const ArxObjTextureFiles* files, size_t index,
                                                         ArxObjTextureFile* out_file, ArxError* error) ARX_NOEXCEPT;
ARX_API void arx_pistoris_obj_texture_files_destroy(ArxObjTextureFiles* files) ARX_NOEXCEPT;
/* Animation report and sound-source output require out_animations */
ARX_API ArxReturnCode arx_pistoris_model_import_glb(const uint8_t* data, size_t size,
                                                    const ArxModelGlbImportOptions* options, ArxModel** out_model,
                                                    ArxAnimationList** out_animations,
                                                    ArxAnimationConversionReport* report,
                                                    ArxTextureSourcePaths** out_texture_source_paths,
                                                    ArxAnimationSoundSourceReferences** out_sound_sources,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_export_glb(const ArxModel* model, const ArxAnimation* const* animations,
                                                    size_t animation_count, const ArxModelGlbExportOptions* options,
                                                    ArxAnimationConversionReport* report, uint8_t** out_data,
                                                    size_t* out_size, ArxAnimationSoundFiles** out_sounds,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_export_level_preview_glb(const ArxModel* model,
                                                                  const ArxModelLevelPreviewGlbOptions* options,
                                                                  uint8_t** out_data, size_t* out_size,
                                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_bake_native(const ArxModel* model, const ArxNativeModelBakeOptions* options,
                                                     ArxFtl** out_native, ArxNativeTextureFiles** out_textures,
                                                     ArxError* error) ARX_NOEXCEPT;

// --- Validation ---

ARX_API ArxReturnCode arx_pistoris_model_validate(const ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_validate_geometry(const ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_validate_skeleton(const ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_validate_action_points(const ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_validate_selections(const ArxModel* model, ArxError* error) ARX_NOEXCEPT;

// --- Model operations ---

ARX_API ArxReturnCode arx_pistoris_model_scale(ArxModel* model, float factor, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_rotate(ArxModel* model, ArxQuat rotation, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_translate(ArxModel* model, ArxVector3 offset, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_apply_reference(ArxModel* model, const ArxModel* reference,
                                                         const ArxModelReferenceOptions* options,
                                                         ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_infer_bone_selection_memberships(ArxModel* model,
                                                                          ArxError* error) ARX_NOEXCEPT;

// --- Resource data ---

ARX_API ArxReturnCode arx_pistoris_model_resource_path(const ArxModel* model, ArxStringView* out_path,
                                                       ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_resource_path(ArxModel* model, ArxStringView path,
                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_inventory_icon(const ArxModel* model, ArxModelInventoryIconView* out_icon,
                                                        ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_inventory_icon(ArxModel* model, ArxEncodedImageView encoded_image,
                                                            const ArxModelInventoryIconSetOptions* options,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_inventory_icon(ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_render_icon(const ArxModel* model,
                                                     const ArxModelInventoryIconRenderOptions* options,
                                                     uint8_t** out_data, size_t* out_size,
                                                     ArxError* error) ARX_NOEXCEPT;

// --- Inspection ---

ARX_API ArxReturnCode arx_pistoris_model_vertex_count(const ArxModel* model, size_t* out_count,
                                                      ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_face_count(const ArxModel* model, size_t* out_count,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_texture_count(const ArxModel* model, size_t* out_count,
                                                       ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_bone_count(const ArxModel* model, size_t* out_count,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_action_point_count(const ArxModel* model, size_t* out_count,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection_count(const ArxModel* model, size_t* out_count,
                                                         ArxError* error) ARX_NOEXCEPT;

ARX_API ArxReturnCode arx_pistoris_model_copy_vertices(const ArxModel* model, size_t offset, size_t count,
                                                       ArxModelVertex* out_vertices, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_faces(const ArxModel* model, size_t offset, size_t count,
                                                    ArxModelFace* out_faces, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_face_data(const ArxModel* model, const ArxModelFacesOutput* output,
                                                        ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_vertex_positions(const ArxModel* model, float* output, size_t count,
                                                               ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_face_textures(const ArxModel* model, ArxTextureIndex* output,
                                                            size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_vertex_bones(const ArxModel* model, ArxBoneIndex* output, size_t count,
                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_action_point_bones(const ArxModel* model, ArxBoneIndex* output,
                                                                 size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_vertex_selection_masks(const ArxModel* model, ArxSelectionMask* output,
                                                                     size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_bone_selection_masks(const ArxModel* model, ArxSelectionMask* output,
                                                                   size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_action_point_selection_masks(const ArxModel* model,
                                                                           ArxSelectionMask* output, size_t count,
                                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_texture_views(const ArxModel* model, size_t offset, size_t count,
                                                            ArxTextureView* out_views, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_bones(const ArxModel* model, size_t offset, size_t count,
                                                    ArxModelBone* out_bones, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_action_points(const ArxModel* model, size_t offset, size_t count,
                                                            ArxModelActionPoint* out_points,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_origin(const ArxModel* model, ArxModelOrigin* out_origin,
                                                ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_selection_ids(const ArxModel* model, size_t offset, size_t count,
                                                            ArxSelectionId* out_ids, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection(const ArxModel* model, ArxSelectionId id,
                                                   ArxModelSelection* out_selection, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection_vertex_count(const ArxModel* model, ArxSelectionId id,
                                                                size_t* out_count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection_bone_count(const ArxModel* model, ArxSelectionId id,
                                                              size_t* out_count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection_action_point_count(const ArxModel* model, ArxSelectionId id,
                                                                      size_t* out_count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_selection_vertices(const ArxModel* model, ArxSelectionId id,
                                                                 size_t offset, size_t count,
                                                                 ArxVertexIndex* out_vertices,
                                                                 ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_selection_bones(const ArxModel* model, ArxSelectionId id, size_t offset,
                                                              size_t count, ArxBoneIndex* out_bones,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_copy_selection_action_points(const ArxModel* model, ArxSelectionId id,
                                                                      size_t offset, size_t count,
                                                                      ArxActionPointIndex* out_points,
                                                                      ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection_includes_origin(const ArxModel* model, ArxSelectionId id,
                                                                   uint8_t* out_includes, ArxError* error) ARX_NOEXCEPT;

// --- Geometry editing ---

ARX_API ArxReturnCode arx_pistoris_model_set_vertex(ArxModel* model, ArxVertexIndex index, const ArxModelVertex* vertex,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_add_vertex(ArxModel* model, const ArxModelVertex* vertex,
                                                    ArxVertexIndex* out_index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_add_vertices(ArxModel* model, const ArxModelVertex* vertices, size_t count,
                                                      ArxVertexIndex* out_first_index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_face(ArxModel* model, ArxFaceIndex index, const ArxModelFace* face,
                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_add_face(ArxModel* model, const ArxModelFace* face, ArxFaceIndex* out_index,
                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_remove_face(ArxModel* model, ArxFaceIndex index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_compact_vertices(ArxModel* model, size_t* out_removed,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_weld_vertices(ArxModel* model, const ArxModelVertexWeldOptions* options,
                                                       ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_compact_textures(ArxModel* model, size_t* out_removed,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_rebase_texture_paths(ArxModel* model, ArxStringView directory,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_texture(ArxModel* model, ArxTextureIndex index,
                                                     const ArxTextureView* texture, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_add_texture(ArxModel* model, const ArxTextureView* texture,
                                                     ArxTextureIndex* out_index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_texture_path(ArxModel* model, ArxTextureIndex index, ArxStringView path,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_texture_external_image_extension(ArxModel* model, ArxTextureIndex index,
                                                                              ArxStringView extension,
                                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_texture_image(ArxModel* model, ArxTextureIndex index, const uint8_t* data,
                                                           size_t size, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_texture_image(ArxModel* model, ArxTextureIndex index,
                                                             ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_faces(ArxModel* model, const ArxModelFacesInput* faces,
                                                       ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_vertices(ArxModel* model, const float* data, size_t count,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_face_textures(ArxModel* model, const ArxTextureIndex* data,
                                                               size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_vertex_bones(ArxModel* model, const ArxBoneIndex* data, size_t count,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_action_point_bones(ArxModel* model, const ArxBoneIndex* data,
                                                                    size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_vertex_selection_masks(ArxModel* model, const ArxSelectionMask* data,
                                                                        size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_bone_selection_masks(ArxModel* model, const ArxSelectionMask* data,
                                                                      size_t count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_replace_action_point_selection_masks(ArxModel* model,
                                                                              const ArxSelectionMask* data,
                                                                              size_t count,
                                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_vertices(ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_faces(ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_textures(ArxModel* model, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_active_selection_mask(const ArxModel* model, ArxSelectionMask* out_mask,
                                                               ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_selection_mask(const ArxModel* model, ArxSelectionId id,
                                                        ArxSelectionMask* out_mask, ArxError* error) ARX_NOEXCEPT;

// --- Skeleton editing ---

ARX_API ArxReturnCode arx_pistoris_model_set_bone(ArxModel* model, ArxBoneIndex index, const ArxModelBone* bone,
                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_add_bone(ArxModel* model, const ArxModelBone* bone, ArxBoneIndex* out_index,
                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_remove_bone(ArxModel* model, ArxBoneIndex index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_origin(ArxModel* model, ArxModelOrigin origin,
                                                    ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_bones(ArxModel* model, ArxError* error) ARX_NOEXCEPT;

// --- Action points ---

ARX_API ArxReturnCode arx_pistoris_model_set_action_point(ArxModel* model, ArxActionPointIndex index,
                                                          const ArxModelActionPoint* point,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_add_action_point(ArxModel* model, const ArxModelActionPoint* point,
                                                          ArxActionPointIndex* out_index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_remove_action_point(ArxModel* model, ArxActionPointIndex index,
                                                             ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_action_points(ArxModel* model, ArxError* error) ARX_NOEXCEPT;

// --- Selections ---

ARX_API ArxReturnCode arx_pistoris_model_add_selection(ArxModel* model, const ArxModelSelection* selection,
                                                       ArxSelectionId* out_id, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_selection(ArxModel* model, ArxSelectionId id,
                                                       const ArxModelSelection* selection,
                                                       ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_update_selection_members(ArxModel* model, ArxSelectionId id,
                                                                  const ArxModelSelectionMembersInput* members,
                                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_selection_vertices(ArxModel* model, ArxSelectionId id,
                                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_selection_bones(ArxModel* model, ArxSelectionId id,
                                                               ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_selection_action_points(ArxModel* model, ArxSelectionId id,
                                                                       ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_set_selection_includes_origin(ArxModel* model, ArxSelectionId id,
                                                                       uint8_t includes, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_remove_selection(ArxModel* model, ArxSelectionId id,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_model_clear_selections(ArxModel* model, ArxError* error) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_MODEL_H */
