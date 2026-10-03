// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
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
