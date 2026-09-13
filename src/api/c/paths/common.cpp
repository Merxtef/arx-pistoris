// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/paths.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "api/c/internal.h"
#include "api/c/paths/internal.h"

#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

namespace pistoris::c_paths {

bool validOutput(char* out, std::size_t capacity, std::size_t* out_size) noexcept {
  return out_size && (out || capacity == 0);
}

ArxReturnCode publishPath(std::string_view value, char* out, std::size_t capacity, std::size_t* out_size) noexcept {
  *out_size = value.size();
  if (!out) return ARX_OK;
  if (capacity <= value.size()) return ARX_BUFFER_TOO_SMALL;
  if (!value.empty()) std::memcpy(out, value.data(), value.size());
  out[value.size()] = '\0';
  return ARX_OK;
}

bool valid(ArxModelPathView value) noexcept {
  return c_api::valid(value.type) && c_api::valid(value.name) && c_api::valid(value.tweak);
}

bool valid(ArxAnimationPathView value) noexcept { return c_api::valid(value.type) && c_api::valid(value.name); }

bool valid(ArxCinematicPathView value) noexcept { return c_api::valid(value.name); }

bool valid(ArxAmbiancePathView value) noexcept { return c_api::valid(value.name); }

paths::ModelPathView modelView(ArxModelPathView value) noexcept {
  return {
      c_api::stringView(value.type),
      c_api::stringView(value.name),
      c_api::stringView(value.tweak),
  };
}

paths::AnimationPathView animationView(ArxAnimationPathView value) noexcept {
  return {c_api::stringView(value.type), c_api::stringView(value.name)};
}

ArxModelPathView cView(paths::ModelPathView value) noexcept {
  return {
      c_api::view(value.type),
      c_api::view(value.name),
      c_api::view(value.tweak),
  };
}

ArxAnimationPathView cView(paths::AnimationPathView value) noexcept {
  return {c_api::view(value.type), c_api::view(value.name)};
}

ArxCinematicPathView cView(paths::CinematicPathView value) noexcept { return {c_api::view(value.name)}; }

ArxAmbiancePathView cView(paths::AmbiancePathView value) noexcept { return {c_api::view(value.name)}; }

ArxResourceSearchLocation cView(paths::ResourceSearchLocation value) noexcept {
  return {c_api::view(value.base_path), value.max_discovery_depth};
}

ArxReturnCode typeAt(std::span<const std::string_view> types, std::size_t index, ArxStringView* out_type) noexcept {
  if (!out_type) return ARX_INVALID_DATA_POINTER;
  *out_type = {};
  if (index >= types.size()) return ARX_INDEX_OUT_OF_RANGE;
  *out_type = c_api::view(types[index]);
  return ARX_OK;
}

}  // namespace pistoris::c_paths

using namespace pistoris::c_paths;

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_path_resource_selector_kind(ArxStringView selector, ArxResourceKind* out_kind) noexcept {
  if (!pistoris::c_api::valid(selector) || !out_kind) return ARX_INVALID_DATA_POINTER;
  *out_kind = pistoris::paths::resourceSelectorKind(pistoris::c_api::stringView(selector));
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_is_portable_filename(ArxStringView filename, uint32_t* out_portable) noexcept {
  if (!pistoris::c_api::valid(filename) || !out_portable) return ARX_INVALID_DATA_POINTER;
  *out_portable = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    *out_portable = pistoris::paths::isPortableFilename(pistoris::c_api::stringView(filename)) ? 1U : 0U;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_sanitize_portable_filename(ArxStringView filename, char* out, size_t capacity,
                                                           size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(filename)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::sanitizePortableFilename(pistoris::c_api::stringView(filename));
    return true;
  });
}

ArxReturnCode arx_pistoris_path_is_portable_resource_path_component(ArxStringView component,
                                                                    uint32_t* out_portable) noexcept {
  if (!pistoris::c_api::valid(component) || !out_portable) return ARX_INVALID_DATA_POINTER;
  *out_portable = pistoris::paths::isPortableResourcePathComponent(pistoris::c_api::stringView(component)) ? 1U : 0U;
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_texture_directory(ArxStringView* out_directory) noexcept {
  if (!out_directory) return ARX_INVALID_DATA_POINTER;
  *out_directory = pistoris::c_api::view(pistoris::paths::textureDirectory());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_sound_directory(ArxStringView* out_directory) noexcept {
  if (!out_directory) return ARX_INVALID_DATA_POINTER;
  *out_directory = pistoris::c_api::view(pistoris::paths::soundDirectory());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_normalize_zone_ambiance(ArxStringView path, char* out, size_t capacity,
                                                        size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::normalizeZoneAmbiance(pistoris::c_api::stringView(path), result);
  });
}

ArxReturnCode arx_pistoris_path_amb_from_zone_ambiance(ArxStringView ambiance, char* out, size_t capacity,
                                                       size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(ambiance)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ambFromZoneAmbiance(pistoris::c_api::stringView(ambiance), result);
  });
}

// NOLINTEND(readability-identifier-naming)
