// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
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
#include <optional>
#include <span>

namespace {

bool validWeldOptions(const ArxModelVertexWeldOptions& options) noexcept {
  const bool valid_metric = options.metric == ARX_MODEL_WELD_EUCLIDEAN || options.metric == ARX_MODEL_WELD_AXIS_ALIGNED;
  const bool valid_policy = options.degenerate_faces == ARX_MODEL_DEGENERATE_FACE_PRESERVE ||
                            options.degenerate_faces == ARX_MODEL_DEGENERATE_FACE_REJECT ||
                            options.degenerate_faces == ARX_MODEL_DEGENERATE_FACE_DISCARD;
  return valid_metric && valid_policy;
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_model_scale(ArxModel* model, float factor, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.scale(factor), error);
}

ArxReturnCode arx_pistoris_model_rotate(ArxModel* model, ArxQuat rotation, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.rotate(rotation), error);
}

ArxReturnCode arx_pistoris_model_translate(ArxModel* model, ArxVector3 offset, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.translate(offset), error);
}

ArxReturnCode arx_pistoris_model_apply_reference(ArxModel* model, const ArxModel* reference,
                                                 const ArxModelReferenceOptions* options, ArxError* error) noexcept {
  if (!model || !reference) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  const pistoris::Model::ReferenceOptions cpp_options{
      .snap_bone_positions = options->snap_bone_positions != 0U,
      .copy_bone_selection_memberships = options->copy_bone_selection_memberships != 0U,
      .copy_action_point_selections = options->copy_action_point_selections != 0U,
  };
  return pistoris::c_api::publish(model->value.applyReference(reference->value, cpp_options), error);
}

ArxReturnCode arx_pistoris_model_infer_bone_selection_memberships(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.inferBoneSelectionMemberships(), error);
}

ArxReturnCode arx_pistoris_model_set_resource_path(ArxModel* model, ArxStringView path, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setResourcePath(pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_model_set_inventory_icon(ArxModel* model, ArxEncodedImageView encoded_image,
                                                    const ArxModelInventoryIconSetOptions* options,
                                                    ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!pistoris::c_api::valid(encoded_image)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  auto result = model->value.setInventoryIcon(
      encoded_image,
      {
          .width_slots = options->width_slots == 0 ? std::nullopt : std::optional(options->width_slots),
          .height_slots = options->height_slots == 0 ? std::nullopt : std::optional(options->height_slots),
      });
  return pistoris::c_api::publish(result, error);
}

ArxReturnCode arx_pistoris_model_clear_inventory_icon(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  model->value.clearInventoryIcon();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_set_vertex(ArxModel* model, ArxVertexIndex index, const ArxModelVertex* vertex,
                                            ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!vertex) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setVertex(index, *vertex), error);
}

ArxReturnCode arx_pistoris_model_add_vertex(ArxModel* model, const ArxModelVertex* vertex, ArxVertexIndex* out_index,
                                            ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!vertex || !out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  auto result = model->value.addVertex(*vertex);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_add_vertices(ArxModel* model, const ArxModelVertex* vertices, size_t count,
                                              ArxVertexIndex* out_first_index, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_first_index || !pistoris::c_api::valid(vertices, count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_first_index = ARX_INVALID_INDEX;
  auto result = model->value.addVertices(std::span<const ArxModelVertex>(vertices, count));
  if (!result) return pistoris::c_api::publish(result, error);
  *out_first_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_set_face(ArxModel* model, ArxFaceIndex index, const ArxModelFace* face,
                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!face) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setFace(index, *face), error);
}

ArxReturnCode arx_pistoris_model_add_face(ArxModel* model, const ArxModelFace* face, ArxFaceIndex* out_index,
                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!face || !out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  auto result = model->value.addFace(*face);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_remove_face(ArxModel* model, ArxFaceIndex index, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.removeFace(index), error);
}

ArxReturnCode arx_pistoris_model_compact_vertices(ArxModel* model, size_t* out_removed, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_removed) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_removed = 0;
  auto result = model->value.compactVertices();
  if (!result) return pistoris::c_api::publish(result, error);
  *out_removed = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_weld_vertices(ArxModel* model, const ArxModelVertexWeldOptions* options,
                                               ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publish(model->value.weldVertices(), error);
  if (!validWeldOptions(*options)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  const pistoris::Model::VertexWeldOptions converted{
      options->radius,
      static_cast<pistoris::Model::PositionWeldMetric>(options->metric),
      static_cast<pistoris::Model::DegenerateFacePolicy>(options->degenerate_faces),
  };
  return pistoris::c_api::publish(model->value.weldVertices(converted), error);
}

ArxReturnCode arx_pistoris_model_compact_textures(ArxModel* model, size_t* out_removed, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_removed) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_removed = 0;
  auto result = model->value.compactTextures();
  if (!result) return pistoris::c_api::publish(result, error);
  *out_removed = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_rebase_texture_paths(ArxModel* model, ArxStringView directory,
                                                      ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(directory)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.rebaseTexturePaths(pistoris::c_api::stringView(directory)), error);
}

ArxReturnCode arx_pistoris_model_set_texture(ArxModel* model, ArxTextureIndex index, const ArxTextureView* texture,
                                             ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!texture || !pistoris::c_api::valid(*texture))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setTexture(index, *texture), error);
}

ArxReturnCode arx_pistoris_model_add_texture(ArxModel* model, const ArxTextureView* texture, ArxTextureIndex* out_index,
                                             ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!texture || !out_index || !pistoris::c_api::valid(*texture))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_NO_TEXTURE;
  auto result = model->value.addTexture(*texture);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_set_texture_path(ArxModel* model, ArxTextureIndex index, ArxStringView path,
                                                  ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setTexturePath(index, pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_model_set_texture_external_image_extension(ArxModel* model, ArxTextureIndex index,
                                                                      ArxStringView extension,
                                                                      ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(extension)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(
      model->value.setTextureExternalImageExtension(index, pistoris::c_api::stringView(extension)), error);
}

ArxReturnCode arx_pistoris_model_set_texture_image(ArxModel* model, ArxTextureIndex index, const uint8_t* data,
                                                   size_t size, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(data, size)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setTextureImage(index, {data, size}), error);
}

ArxReturnCode arx_pistoris_model_clear_texture_image(ArxModel* model, ArxTextureIndex index, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.clearTextureImage(index), error);
}

ArxReturnCode arx_pistoris_model_replace_mesh(ArxModel* model, const ArxModelMeshInput* mesh,
                                              ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!mesh || (pistoris::c_api::validModelMeshCounts(*mesh) && !pistoris::c_api::valid(*mesh)))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.replaceMesh(*mesh), error);
}

ArxReturnCode arx_pistoris_model_clear_mesh(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    model->value.clearMesh();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_set_bone(ArxModel* model, ArxBoneIndex index, const ArxModelBone* bone,
                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!bone || !pistoris::c_api::valid(*bone)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setBone(index, *bone), error);
}

ArxReturnCode arx_pistoris_model_add_bone(ArxModel* model, const ArxModelBone* bone, ArxBoneIndex* out_index,
                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!bone || !out_index || !pistoris::c_api::valid(*bone))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  auto result = model->value.addBone(*bone);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_remove_bone(ArxModel* model, ArxBoneIndex index, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.removeBone(index), error);
}

ArxReturnCode arx_pistoris_model_replace_skeleton(ArxModel* model, const ArxModelSkeletonInput* skeleton,
                                                  ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!skeleton || (pistoris::c_api::validModelSkeletonCount(*skeleton) && !pistoris::c_api::valid(*skeleton)))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.replaceSkeleton(*skeleton), error);
}

ArxReturnCode arx_pistoris_model_set_origin(ArxModel* model, ArxModelOrigin origin, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.setOrigin(origin), error);
}

ArxReturnCode arx_pistoris_model_clear_skeleton(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    model->value.clearSkeleton();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_set_action_point(ArxModel* model, ArxActionPointIndex index,
                                                  const ArxModelActionPoint* point, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!point || !pistoris::c_api::valid(*point)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setActionPoint(index, *point), error);
}

ArxReturnCode arx_pistoris_model_add_action_point(ArxModel* model, const ArxModelActionPoint* point,
                                                  ArxActionPointIndex* out_index, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!point || !out_index || !pistoris::c_api::valid(*point))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_INDEX;
  auto result = model->value.addActionPoint(*point);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_remove_action_point(ArxModel* model, ArxActionPointIndex index,
                                                     ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.removeActionPoint(index), error);
}

ArxReturnCode arx_pistoris_model_replace_action_points(ArxModel* model, const ArxModelActionPointsInput* points,
                                                       ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!points || (pistoris::c_api::validModelActionPointCount(*points) && !pistoris::c_api::valid(*points)))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.replaceActionPoints(*points), error);
}

ArxReturnCode arx_pistoris_model_clear_action_points(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    model->value.clearActionPoints();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_add_selection(ArxModel* model, const ArxModelSelection* selection,
                                               ArxSelectionId* out_id, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!selection || !out_id || !pistoris::c_api::valid(*selection))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_id = ARX_INVALID_SELECTION_ID;
  auto result = model->value.addSelection(*selection);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_id = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_set_selection(ArxModel* model, ArxSelectionId id, const ArxModelSelection* selection,
                                               ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!selection || !pistoris::c_api::valid(*selection))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.setSelection(id, *selection), error);
}

ArxReturnCode arx_pistoris_model_update_selection_members(ArxModel* model, ArxSelectionId id,
                                                          const ArxModelSelectionMembersInput* members,
                                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!members || !pistoris::c_api::valid(*members))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(model->value.updateSelectionMembers(id, *members), error);
}

ArxReturnCode arx_pistoris_model_clear_selection_vertices(ArxModel* model, ArxSelectionId id,
                                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.clearSelectionVertices(id), error);
}

ArxReturnCode arx_pistoris_model_clear_selection_bones(ArxModel* model, ArxSelectionId id, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.clearSelectionBones(id), error);
}

ArxReturnCode arx_pistoris_model_clear_selection_action_points(ArxModel* model, ArxSelectionId id,
                                                               ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.clearSelectionActionPoints(id), error);
}

ArxReturnCode arx_pistoris_model_set_selection_includes_origin(ArxModel* model, ArxSelectionId id, uint8_t includes,
                                                               ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.setSelectionIncludesOrigin(id, includes != 0U), error);
}

ArxReturnCode arx_pistoris_model_remove_selection(ArxModel* model, ArxSelectionId id, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.removeSelection(id), error);
}

ArxReturnCode arx_pistoris_model_clear_selections(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    model->value.clearSelections();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

// NOLINTEND(readability-identifier-naming)
