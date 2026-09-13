// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/texture.h"

#include "api/c/internal.h"
#include "api/c/model/internal.h"
#include "api/c/texture/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_model_scale(ArxModel* model, float factor) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.scale(factor); });
}

ArxReturnCode arx_pistoris_model_rotate(ArxModel* model, ArxQuat rotation) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.rotate(rotation); });
}

ArxReturnCode arx_pistoris_model_translate(ArxModel* model, ArxVector3 offset) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.translate(offset); });
}

ArxReturnCode arx_pistoris_model_apply_reference(ArxModel* model, const ArxModel* reference,
                                                 const ArxModelReferenceOptions* options) noexcept {
  if (!model || !reference) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  return pistoris::c_api::guard([&] {
    const pistoris::Model::ReferenceOptions cpp_options{
        .snap_bone_origins = options->snap_bone_origins != 0U,
        .copy_bone_origin_selections = options->copy_bone_origin_selections != 0U,
        .copy_action_point_selections = options->copy_action_point_selections != 0U,
    };
    return model->value.applyReference(reference->value, cpp_options);
  });
}

ArxReturnCode arx_pistoris_model_infer_bone_origin_selections(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.inferBoneOriginSelections(); });
}

ArxReturnCode arx_pistoris_model_set_resource_path(ArxModel* model, ArxStringView path) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setResourcePath(pistoris::c_api::stringView(path)); });
}

ArxReturnCode arx_pistoris_model_set_inventory_icon(ArxModel* model, ArxEncodedImageView encoded_image,
                                                    const ArxModelInventoryIconSetOptions* options) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!pistoris::c_api::valid(encoded_image)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] {
    return model->value.setInventoryIcon(encoded_image,
                                         {
                                             .width_slots = options->width_slots,
                                             .height_slots = options->height_slots,
                                         });
  });
}

ArxReturnCode arx_pistoris_model_clear_inventory_icon(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  model->value.clearInventoryIcon();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_model_set_vertex(ArxModel* model, ArxVertexIndex index,
                                            const ArxModelVertex* vertex) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!vertex) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setVertex(index, *vertex); });
}

ArxReturnCode arx_pistoris_model_add_vertex(ArxModel* model, const ArxModelVertex* vertex,
                                            ArxVertexIndex* out_index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!vertex || !out_index) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return model->value.addVertex(*vertex, *out_index); });
}

ArxReturnCode arx_pistoris_model_add_vertices(ArxModel* model, const ArxModelVertex* vertices, size_t count,
                                              ArxVertexIndex* out_first_index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_first_index || !pistoris::c_api::valid(vertices, count)) return ARX_INVALID_DATA_POINTER;
  *out_first_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return model->value.addVertices(vertices, count, *out_first_index); });
}

ArxReturnCode arx_pistoris_model_set_face(ArxModel* model, ArxFaceIndex index, const ArxModelFace* face) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!face) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setFace(index, *face); });
}

ArxReturnCode arx_pistoris_model_add_face(ArxModel* model, const ArxModelFace* face, ArxFaceIndex* out_index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!face || !out_index) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return model->value.addFace(*face, *out_index); });
}

ArxReturnCode arx_pistoris_model_remove_face(ArxModel* model, ArxFaceIndex index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.removeFace(index); });
}

ArxReturnCode arx_pistoris_model_compact_vertices(ArxModel* model, size_t* out_removed) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_removed) return ARX_INVALID_DATA_POINTER;
  *out_removed = 0;
  return pistoris::c_api::guard([&] { return model->value.compactVertices(out_removed); });
}

ArxReturnCode arx_pistoris_model_compact_textures(ArxModel* model, size_t* out_removed) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_removed) return ARX_INVALID_DATA_POINTER;
  *out_removed = 0;
  return pistoris::c_api::guard([&] { return model->value.compactTextures(out_removed); });
}

ArxReturnCode arx_pistoris_model_rebase_texture_paths(ArxModel* model, ArxStringView directory) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(directory)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return model->value.rebaseTexturePaths(pistoris::c_api::stringView(directory)); });
}

ArxReturnCode arx_pistoris_model_set_texture(ArxModel* model, ArxTextureIndex index,
                                             const ArxTextureView* texture) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!texture || !pistoris::c_api::valid(*texture)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setTexture(index, *texture); });
}

ArxReturnCode arx_pistoris_model_add_texture(ArxModel* model, const ArxTextureView* texture,
                                             ArxTextureIndex* out_index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!texture || !out_index || !pistoris::c_api::valid(*texture)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_NO_TEXTURE;
  return pistoris::c_api::guard([&] { return model->value.addTexture(*texture, *out_index); });
}

ArxReturnCode arx_pistoris_model_set_texture_image(ArxModel* model, ArxTextureIndex index, const uint8_t* data,
                                                   size_t size) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(data, size)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setTextureImage(index, {data, size}); });
}

ArxReturnCode arx_pistoris_model_clear_texture_image(ArxModel* model, ArxTextureIndex index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.clearTextureImage(index); });
}

ArxReturnCode arx_pistoris_model_replace_mesh(ArxModel* model, const ArxModelMeshInput* mesh) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!mesh || (pistoris::c_api::validModelMeshCounts(*mesh) && !pistoris::c_api::valid(*mesh)))
    return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.replaceMesh(*mesh); });
}

ArxReturnCode arx_pistoris_model_clear_mesh(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    model->value.clearMesh();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_set_bone(ArxModel* model, ArxBoneIndex index, const ArxModelBone* bone) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!bone || !pistoris::c_api::valid(*bone)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setBone(index, *bone); });
}

ArxReturnCode arx_pistoris_model_add_bone(ArxModel* model, const ArxModelBone* bone, ArxBoneIndex* out_index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!bone || !out_index || !pistoris::c_api::valid(*bone)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return model->value.addBone(*bone, *out_index); });
}

ArxReturnCode arx_pistoris_model_remove_bone(ArxModel* model, ArxBoneIndex index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.removeBone(index); });
}

ArxReturnCode arx_pistoris_model_replace_skeleton(ArxModel* model, const ArxModelSkeletonInput* skeleton) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!skeleton || (pistoris::c_api::validModelSkeletonCount(*skeleton) && !pistoris::c_api::valid(*skeleton)))
    return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.replaceSkeleton(*skeleton); });
}

ArxReturnCode arx_pistoris_model_set_origin(ArxModel* model, ArxModelOrigin origin) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.setOrigin(origin); });
}

ArxReturnCode arx_pistoris_model_clear_skeleton(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    model->value.clearSkeleton();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_set_action_point(ArxModel* model, ArxActionPointIndex index,
                                                  const ArxModelActionPoint* point) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!point || !pistoris::c_api::valid(*point)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setActionPoint(index, *point); });
}

ArxReturnCode arx_pistoris_model_add_action_point(ArxModel* model, const ArxModelActionPoint* point,
                                                  ArxActionPointIndex* out_index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!point || !out_index || !pistoris::c_api::valid(*point)) return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_INVALID_INDEX;
  return pistoris::c_api::guard([&] { return model->value.addActionPoint(*point, *out_index); });
}

ArxReturnCode arx_pistoris_model_remove_action_point(ArxModel* model, ArxActionPointIndex index) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.removeActionPoint(index); });
}

ArxReturnCode arx_pistoris_model_replace_action_points(ArxModel* model,
                                                       const ArxModelActionPointsInput* points) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!points || (pistoris::c_api::validModelActionPointCount(*points) && !pistoris::c_api::valid(*points)))
    return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.replaceActionPoints(*points); });
}

ArxReturnCode arx_pistoris_model_clear_action_points(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    model->value.clearActionPoints();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_add_selection(ArxModel* model, const ArxModelSelection* selection,
                                               ArxSelectionId* out_id) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!selection || !out_id || !pistoris::c_api::valid(*selection)) return ARX_INVALID_DATA_POINTER;
  *out_id = ARX_INVALID_SELECTION_ID;
  return pistoris::c_api::guard([&] { return model->value.addSelection(*selection, *out_id); });
}

ArxReturnCode arx_pistoris_model_set_selection(ArxModel* model, ArxSelectionId id,
                                               const ArxModelSelection* selection) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!selection || !pistoris::c_api::valid(*selection)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.setSelection(id, *selection); });
}

ArxReturnCode arx_pistoris_model_update_selection_members(ArxModel* model, ArxSelectionId id,
                                                          const ArxModelSelectionMembersInput* members) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!members || !pistoris::c_api::valid(*members)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.updateSelectionMembers(id, *members); });
}

ArxReturnCode arx_pistoris_model_clear_selection_vertices(ArxModel* model, ArxSelectionId id) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.clearSelectionVertices(id); });
}

ArxReturnCode arx_pistoris_model_clear_selection_bones(ArxModel* model, ArxSelectionId id) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.clearSelectionBones(id); });
}

ArxReturnCode arx_pistoris_model_clear_selection_action_points(ArxModel* model, ArxSelectionId id) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.clearSelectionActionPoints(id); });
}

ArxReturnCode arx_pistoris_model_set_selection_includes_origin(ArxModel* model, ArxSelectionId id,
                                                               uint8_t includes) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.setSelectionIncludesOrigin(id, includes != 0U); });
}

ArxReturnCode arx_pistoris_model_remove_selection(ArxModel* model, ArxSelectionId id) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.removeSelection(id); });
}

ArxReturnCode arx_pistoris_model_clear_selections(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    model->value.clearSelections();
    return ARX_OK;
  });
}

// NOLINTEND(readability-identifier-naming)
