// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "api/c/internal.h"
#include "api/c/paths/internal.h"

#include <cstddef>
#include <string>
#include <string_view>

using namespace pistoris::c_paths;

// NOLINTBEGIN(readability-identifier-naming)

size_t arx_pistoris_path_animation_type_count(void) noexcept { return pistoris::paths::animationPathTypes().size(); }

ArxReturnCode arx_pistoris_path_animation_type_at(size_t index, ArxAnimationPathType* out_type) noexcept {
  if (!out_type) return ARX_INVALID_DATA_POINTER;
  *out_type = ARX_ANIMATION_PATH_TYPE_NONE;
  const auto types = pistoris::paths::animationPathTypes();
  if (index >= types.size()) return ARX_INDEX_OUT_OF_RANGE;
  *out_type = static_cast<ArxAnimationPathType>(types[index]);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_animation_type_name(ArxAnimationPathType type, ArxStringView* out_name) noexcept {
  if (!out_name) return ARX_INVALID_DATA_POINTER;
  *out_name = {};
  const std::string_view name =
      pistoris::paths::animationPathTypeName(static_cast<pistoris::paths::AnimationPathType>(type));
  if (name.empty()) return ARX_INVALID_IDENTIFIER;
  *out_name = pistoris::c_api::view(name);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_animation_type_from_name(ArxStringView name, ArxAnimationPathType* out_type) noexcept {
  if (!pistoris::c_api::valid(name) || !out_type) return ARX_INVALID_DATA_POINTER;
  *out_type = ARX_ANIMATION_PATH_TYPE_NONE;
  pistoris::paths::AnimationPathType result = pistoris::paths::AnimationPathType::kNone;
  if (!pistoris::paths::animationPathTypeFromName(pistoris::c_api::stringView(name), result))
    return ARX_INVALID_IDENTIFIER;
  *out_type = static_cast<ArxAnimationPathType>(result);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_animation_directory(ArxAnimationPathType type, char* out, size_t capacity,
                                                    size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationDirectory(static_cast<pistoris::paths::AnimationPathType>(type), result);
  });
}

ArxReturnCode arx_pistoris_path_model_animation_directory(ArxModelPathType type, char* out, size_t capacity,
                                                          size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::animationDirectory(static_cast<pistoris::paths::ModelPathType>(type), result);
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

ArxReturnCode arx_pistoris_path_animation_search_location(ArxAnimationPathType type,
                                                          ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = {};
  pistoris::paths::ResourceSearchLocation result;
  if (!pistoris::paths::animationSearchLocation(static_cast<pistoris::paths::AnimationPathType>(type), result))
    return ARX_INVALID_IDENTIFIER;
  *out_location = cView(result);
  return ARX_OK;
}

// NOLINTEND(readability-identifier-naming)
