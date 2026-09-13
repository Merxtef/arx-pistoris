// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths.h"
#include "arx_pistoris/paths.hpp"

#include "api/c/internal.h"
#include "api/c/paths/internal.h"

#include <cstddef>
#include <string>

using namespace pistoris::c_paths;

// NOLINTBEGIN(readability-identifier-naming)

size_t arx_pistoris_path_animation_selector_type_count(void) noexcept {
  return pistoris::paths::animationSelectorTypes().size();
}

ArxReturnCode arx_pistoris_path_animation_selector_type(size_t index, ArxStringView* out_type) noexcept {
  return typeAt(pistoris::paths::animationSelectorTypes(), index, out_type);
}

ArxReturnCode arx_pistoris_path_animation_directory(ArxStringView interactive_type, char* out, size_t capacity,
                                                    size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(interactive_type)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationDirectory(pistoris::c_api::stringView(interactive_type), result);
  });
}

ArxReturnCode arx_pistoris_path_animation_tea(ArxAnimationPathView animation, char* out, size_t capacity,
                                              size_t* out_size) noexcept {
  if (!valid(animation)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationTea(animationView(animation), result);
  });
}

ArxReturnCode arx_pistoris_path_animation_from_tea(ArxStringView path, ArxAnimationPathView* out_animation) noexcept {
  if (!pistoris::c_api::valid(path) || !out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AnimationPathView result;
    if (!pistoris::paths::animationFromTea(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_animation = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_animation_selector(ArxAnimationPathView animation, char* out, size_t capacity,
                                                   size_t* out_size) noexcept {
  if (!valid(animation)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationSelector(animationView(animation), result);
  });
}

ArxReturnCode arx_pistoris_path_animation_from_selector(ArxStringView selector,
                                                        ArxAnimationPathView* out_animation) noexcept {
  if (!pistoris::c_api::valid(selector) || !out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AnimationPathView result;
    if (!pistoris::paths::animationFromSelector(pistoris::c_api::stringView(selector), result))
      return ARX_INVALID_IDENTIFIER;
    *out_animation = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_animation_search_location(ArxStringView type,
                                                          ArxResourceSearchLocation* out_location) noexcept {
  if (!pistoris::c_api::valid(type) || !out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = {};
  pistoris::paths::ResourceSearchLocation result;
  if (!pistoris::paths::animationSearchLocation(pistoris::c_api::stringView(type), result))
    return ARX_INVALID_IDENTIFIER;
  *out_location = cView(result);
  return ARX_OK;
}

// NOLINTEND(readability-identifier-naming)
