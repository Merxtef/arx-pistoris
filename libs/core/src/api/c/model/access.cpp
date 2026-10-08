// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/texture.h"

#include "api/c/internal.h"
#include "api/c/model/internal.h"  // IWYU pragma: keep

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace {

template <class T>
ArxReturnCode validateOutputPointer(T* data, std::size_t count) noexcept {
  if (!data) return count == 0 ? ARX_OK : ARX_INVALID_DATA_POINTER;
  if (count == 0) return ARX_OK;
  if (reinterpret_cast<std::uintptr_t>(data) % alignof(T) != 0 ||
      count > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / sizeof(T) ||
      count > std::numeric_limits<std::uintptr_t>::max() / sizeof(T)) {
    return ARX_INVALID_DATA_POINTER;
  }
  const std::uintptr_t begin = reinterpret_cast<std::uintptr_t>(data);
  const std::uintptr_t bytes = count * sizeof(T);
  if (begin > std::numeric_limits<std::uintptr_t>::max() - bytes) return ARX_INVALID_DATA_POINTER;
  return ARX_OK;
}

template <class T>
std::optional<std::span<T>> outputSpan(T* data, std::size_t count) noexcept {
  if (!data) return std::nullopt;
  return std::span<T>(data, count);
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_model_resource_path(const ArxModel* model, ArxStringView* out_path,
                                               ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_path) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_path = pistoris::c_api::view(model->value.resourcePath());
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_inventory_icon(const ArxModel* model, ArxModelInventoryIconView* out_icon,
                                                ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_icon) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Model::InventoryIconView icon = model->value.inventoryIcon();
  *out_icon = {
      .encoded_image = icon.encoded_image,
      .width_slots = icon.width_slots,
      .height_slots = icon.height_slots,
  };
  return pistoris::c_api::publishCode(ARX_OK, error);
}

#define ARX_MODEL_COUNT(name, method)                                                     \
  ArxReturnCode arx_pistoris_model_##name##_count(                                        \
      const ArxModel* model, size_t* out_count, ArxError* error) noexcept {               \
    if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);           \
    if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error); \
    *out_count = model->value.method();                                                   \
    return pistoris::c_api::publishCode(ARX_OK, error);                                   \
  }

ARX_MODEL_COUNT(vertex, vertexCount)
ARX_MODEL_COUNT(face, faceCount)
ARX_MODEL_COUNT(texture, textureCount)
ARX_MODEL_COUNT(bone, boneCount)
ARX_MODEL_COUNT(action_point, actionPointCount)
ARX_MODEL_COUNT(selection, selectionCount)

#undef ARX_MODEL_COUNT

#define ARX_MODEL_COPY(name, method, type)                                                               \
  ArxReturnCode arx_pistoris_model_copy_##name(                                                          \
      const ArxModel* model, size_t offset, size_t count, type(*out_values), ArxError* error) noexcept { \
    if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                          \
    const auto values = model->value.method();                                                           \
    if (offset > values.size() || count > values.size() - offset)                                        \
      return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);                                \
    if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error); \
    for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];      \
    return pistoris::c_api::publishCode(ARX_OK, error);                                                  \
  }

ARX_MODEL_COPY(vertices, vertices, ArxModelVertex)
ARX_MODEL_COPY(faces, faces, ArxModelFace)
ARX_MODEL_COPY(texture_views, textures, ArxTextureView)
ARX_MODEL_COPY(bones, bones, ArxModelBone)
ARX_MODEL_COPY(action_points, actionPoints, ArxModelActionPoint)

#undef ARX_MODEL_COPY

ArxReturnCode arx_pistoris_model_copy_face_data(const ArxModel* model, const ArxModelFacesOutput* output,
                                                ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!output) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
#define ARX_MODEL_VALIDATE_COPY_OUTPUT(field, count_field, type)                              \
  do {                                                                                        \
    const ArxReturnCode rc = validateOutputPointer<type>(output->field, output->count_field); \
    if (rc != ARX_OK) return pistoris::c_api::publishCode(rc, error);                         \
  } while (false)
  ARX_MODEL_VALIDATE_COPY_OUTPUT(vertex_indices, vertex_index_count, std::uint32_t);
  ARX_MODEL_VALIDATE_COPY_OUTPUT(uvs, uv_count, float);
  ARX_MODEL_VALIDATE_COPY_OUTPUT(corner_normals, corner_normal_count, float);
  ARX_MODEL_VALIDATE_COPY_OUTPUT(textures, texture_count, ArxTextureIndex);
  ARX_MODEL_VALIDATE_COPY_OUTPUT(transvals, transval_count, float);
  ARX_MODEL_VALIDATE_COPY_OUTPUT(face_normals, face_normal_count, float);
  ARX_MODEL_VALIDATE_COPY_OUTPUT(flags, flag_count, ArxFaceType);
#undef ARX_MODEL_VALIDATE_COPY_OUTPUT
  const pistoris::Model::FacesOutput destinations{
      .vertex_indices = outputSpan(output->vertex_indices, output->vertex_index_count),
      .uvs = outputSpan(output->uvs, output->uv_count),
      .corner_normals = outputSpan(output->corner_normals, output->corner_normal_count),
      .textures = outputSpan(output->textures, output->texture_count),
      .transvals = outputSpan(output->transvals, output->transval_count),
      .face_normals = outputSpan(output->face_normals, output->face_normal_count),
      .flags = outputSpan(output->flags, output->flag_count),
  };
  return pistoris::c_api::publish(model->value.copyFaces(destinations), error);
}

#define ARX_MODEL_COPY_BULK(name, method, type)                                                      \
  ArxReturnCode arx_pistoris_model_copy_##name(                                                      \
      const ArxModel* model, type(*output), size_t count, ArxError* error) noexcept {                \
    if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                      \
    if (const ArxReturnCode rc = validateOutputPointer<type>(output, count); rc != ARX_OK)           \
      return pistoris::c_api::publishCode(rc, error);                                                \
    const std::span<type> destination = output ? std::span<type>(output, count) : std::span<type>{}; \
    return pistoris::c_api::publish(model->value.method(destination), error);                        \
  }

ARX_MODEL_COPY_BULK(vertex_positions, copyVertexPositions, float)
ARX_MODEL_COPY_BULK(face_textures, copyFaceTextures, ArxTextureIndex)
ARX_MODEL_COPY_BULK(vertex_bones, copyVertexBones, ArxBoneIndex)
ARX_MODEL_COPY_BULK(action_point_bones, copyActionPointBones, ArxBoneIndex)
ARX_MODEL_COPY_BULK(vertex_selection_masks, copyVertexSelectionMasks, ArxSelectionMask)
ARX_MODEL_COPY_BULK(bone_selection_masks, copyBoneSelectionMasks, ArxSelectionMask)
ARX_MODEL_COPY_BULK(action_point_selection_masks, copyActionPointSelectionMasks, ArxSelectionMask)

#undef ARX_MODEL_COPY_BULK

ArxReturnCode arx_pistoris_model_origin(const ArxModel* model, ArxModelOrigin* out_origin, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_origin) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_origin = model->value.origin();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_copy_selection_ids(const ArxModel* model, size_t offset, size_t count,
                                                    ArxSelectionId* out_ids, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Model::SelectionIdsView values = model->value.selectionIds();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_ids) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_ids[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_model_selection(const ArxModel* model, ArxSelectionId id, ArxModelSelection* out_selection,
                                           ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_selection) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  auto result = model->value.selection(id);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_selection = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

#define ARX_MODEL_SELECTION_COUNT(name, method)                                                \
  ArxReturnCode arx_pistoris_model_selection_##name##_count(                                   \
      const ArxModel* model, ArxSelectionId id, size_t* out_count, ArxError* error) noexcept { \
    if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                \
    if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);      \
    const auto result = model->value.method(id);                                               \
    if (!result) return pistoris::c_api::publish(result, error);                               \
    *out_count = result->size();                                                               \
    return pistoris::c_api::publishCode(ARX_OK, error);                                        \
  }

ARX_MODEL_SELECTION_COUNT(vertex, selectionVertices)
ARX_MODEL_SELECTION_COUNT(bone, selectionBones)
ARX_MODEL_SELECTION_COUNT(action_point, selectionActionPoints)

#undef ARX_MODEL_SELECTION_COUNT

#define ARX_MODEL_SELECTION_COPY(name, method, type)                                                     \
  ArxReturnCode arx_pistoris_model_copy_selection_##name(const ArxModel* model,                          \
                                                         ArxSelectionId id,                              \
                                                         size_t offset,                                  \
                                                         size_t count,                                   \
                                                         type(*out_values),                              \
                                                         ArxError* error) noexcept {                     \
    if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                          \
    const auto result = model->value.method(id);                                                         \
    if (!result) return pistoris::c_api::publish(result, error);                                         \
    if (offset > result->size() || count > result->size() - offset)                                      \
      return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);                                \
    if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error); \
    for (std::size_t index = 0; index < count; ++index) out_values[index] = (*result)[offset + index];   \
    return pistoris::c_api::publishCode(ARX_OK, error);                                                  \
  }

ARX_MODEL_SELECTION_COPY(vertices, selectionVertices, ArxVertexIndex)
ARX_MODEL_SELECTION_COPY(bones, selectionBones, ArxBoneIndex)
ARX_MODEL_SELECTION_COPY(action_points, selectionActionPoints, ArxActionPointIndex)

#undef ARX_MODEL_SELECTION_COPY

ArxReturnCode arx_pistoris_model_selection_includes_origin(const ArxModel* model, ArxSelectionId id,
                                                           uint8_t* out_includes, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_includes) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  auto result = model->value.selectionIncludesOrigin(id);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_includes = *result ? 1U : 0U;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

// NOLINTEND(readability-identifier-naming)
