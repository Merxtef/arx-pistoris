// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"

#include "api/c/animation/internal.h"
#include "api/c/error_internal.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace pistoris::c_api {

std::unique_ptr<ArxAnimationList> makeAnimationList(std::vector<pistoris::Animation>&& animations) {
  auto result = std::make_unique<ArxAnimationList>();
  result->value.reserve(animations.size());
  for (pistoris::Animation& animation : animations) {
    auto handle = std::make_unique<ArxAnimation>();
    handle->value.swap(animation);
    result->value.push_back(std::move(handle));
  }
  return result;
}

}  // namespace pistoris::c_api

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_list_count(const ArxAnimationList* list, size_t* out_count,
                                                ArxError* error) noexcept {
  if (!list) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = list->value.size();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_list_get(ArxAnimationList* list, size_t index, ArxAnimation** out_animation,
                                              ArxError* error) noexcept {
  if (!list) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_animation) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_animation = nullptr;
  if (index >= list->value.size()) return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  *out_animation = list->value[index].get();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

void arx_pistoris_animation_list_destroy(ArxAnimationList* list) noexcept { delete list; }

// NOLINTEND(readability-identifier-naming)
