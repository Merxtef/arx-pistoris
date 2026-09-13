// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

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

ArxReturnCode arx_pistoris_model_resource_path(const ArxModel* model, ArxStringView* out_path) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = pistoris::c_api::view(model->value.resourcePath());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_model_inventory_icon(const ArxModel* model, ArxModelInventoryIconView* out_icon) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_icon) return ARX_INVALID_DATA_POINTER;
  const pistoris::Model::InventoryIconView icon = model->value.inventoryIcon();
  *out_icon = {
      .encoded_image = icon.encoded_image,
      .width_slots = icon.width_slots,
      .height_slots = icon.height_slots,
  };
  return ARX_OK;
}

#define ARX_MODEL_COUNT(name, method)                                                                  \
  ArxReturnCode arx_pistoris_model_##name##_count(const ArxModel* model, size_t* out_count) noexcept { \
    if (!model) return ARX_INVALID_HANDLE;                                                             \
    if (!out_count) return ARX_INVALID_DATA_POINTER;                                                   \
    *out_count = model->value.method();                                                                \
    return ARX_OK;                                                                                     \
  }

ARX_MODEL_COUNT(vertex, vertexCount)
ARX_MODEL_COUNT(face, faceCount)
ARX_MODEL_COUNT(texture, textureCount)
ARX_MODEL_COUNT(bone, boneCount)
ARX_MODEL_COUNT(action_point, actionPointCount)
ARX_MODEL_COUNT(selection, selectionCount)

#undef ARX_MODEL_COUNT

#define ARX_MODEL_COPY(name, method, type)                                                         \
  ArxReturnCode arx_pistoris_model_copy_##name(                                                    \
      const ArxModel* model, size_t offset, size_t count, type(*out_values)) noexcept {            \
    if (!model) return ARX_INVALID_HANDLE;                                                         \
    return pistoris::c_api::guard([&] { return model->value.method(offset, count, out_values); }); \
  }

ARX_MODEL_COPY(vertices, copyVertices, ArxModelVertex)
ARX_MODEL_COPY(faces, copyFaces, ArxModelFace)
ARX_MODEL_COPY(texture_views, copyTextureViews, ArxTextureView)
ARX_MODEL_COPY(bones, copyBones, ArxModelBone)
ARX_MODEL_COPY(action_points, copyActionPoints, ArxModelActionPoint)

#undef ARX_MODEL_COPY

ArxReturnCode arx_pistoris_model_origin(const ArxModel* model, ArxModelOrigin* out_origin) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_origin) return ARX_INVALID_DATA_POINTER;
  *out_origin = model->value.origin();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_model_copy_selection_ids(const ArxModel* model, size_t offset, size_t count,
                                                    ArxSelectionId* out_ids) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.copySelectionIds(offset, count, out_ids); });
}

ArxReturnCode arx_pistoris_model_selection(const ArxModel* model, ArxSelectionId id,
                                           ArxModelSelection* out_selection) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_selection) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return model->value.selection(id, *out_selection); });
}

#define ARX_MODEL_SELECTION_COUNT(name, method)                                         \
  ArxReturnCode arx_pistoris_model_selection_##name##_count(                            \
      const ArxModel* model, ArxSelectionId id, size_t* out_count) noexcept {           \
    if (!model) return ARX_INVALID_HANDLE;                                              \
    if (!out_count) return ARX_INVALID_DATA_POINTER;                                    \
    return pistoris::c_api::guard([&] { return model->value.method(id, *out_count); }); \
  }

ARX_MODEL_SELECTION_COUNT(vertex, selectionVertexCount)
ARX_MODEL_SELECTION_COUNT(bone, selectionBoneCount)
ARX_MODEL_SELECTION_COUNT(action_point, selectionActionPointCount)

#undef ARX_MODEL_SELECTION_COUNT

#define ARX_MODEL_SELECTION_COPY(name, method, type)                                                       \
  ArxReturnCode arx_pistoris_model_copy_selection_##name(                                                  \
      const ArxModel* model, ArxSelectionId id, size_t offset, size_t count, type(*out_values)) noexcept { \
    if (!model) return ARX_INVALID_HANDLE;                                                                 \
    return pistoris::c_api::guard([&] { return model->value.method(id, offset, count, out_values); });     \
  }

ARX_MODEL_SELECTION_COPY(vertices, copySelectionVertices, ArxVertexIndex)
ARX_MODEL_SELECTION_COPY(bones, copySelectionBones, ArxBoneIndex)
ARX_MODEL_SELECTION_COPY(action_points, copySelectionActionPoints, ArxActionPointIndex)

#undef ARX_MODEL_SELECTION_COPY

ArxReturnCode arx_pistoris_model_selection_includes_origin(const ArxModel* model, ArxSelectionId id,
                                                           uint8_t* out_includes) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_includes) return ARX_INVALID_DATA_POINTER;
  bool includes = false;
  const ArxReturnCode rc = pistoris::c_api::guard([&] { return model->value.selectionIncludesOrigin(id, includes); });
  *out_includes = includes ? 1U : 0U;
  return rc;
}

// NOLINTEND(readability-identifier-naming)
