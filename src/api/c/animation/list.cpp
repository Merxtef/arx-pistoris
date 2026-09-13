// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/base/status.h"

#include "api/c/animation/internal.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace pistoris::c_api {

std::unique_ptr<ArxAnimationList> makeAnimationList(std::vector<std::unique_ptr<pistoris::Animation>>&& animations) {
  auto result = std::make_unique<ArxAnimationList>();
  result->value.reserve(animations.size());
  for (std::unique_ptr<pistoris::Animation>& animation : animations) {
    auto handle = std::make_unique<ArxAnimation>();
    handle->value.swap(*animation);
    result->value.push_back(std::move(handle));
  }
  return result;
}

}  // namespace pistoris::c_api

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_list_count(const ArxAnimationList* list, size_t* out_count) noexcept {
  if (!list) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = list->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_list_get(ArxAnimationList* list, size_t index,
                                              ArxAnimation** out_animation) noexcept {
  if (!list) return ARX_INVALID_HANDLE;
  if (!out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = nullptr;
  if (index >= list->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  *out_animation = list->value[index].get();
  return ARX_OK;
}

void arx_pistoris_animation_list_destroy(ArxAnimationList* list) noexcept { delete list; }

// NOLINTEND(readability-identifier-naming)
