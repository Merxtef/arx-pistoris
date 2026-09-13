// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths.h"
#include "arx_pistoris/paths.hpp"

#include "api/c/internal.h"
#include "api/c/paths/internal.h"

#include <cstddef>
#include <cstdint>
#include <string>

using namespace pistoris::c_paths;

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_path_level_dlf(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelDlf(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_llf(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelLlf(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_fts(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelFts(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_minimap_resource_level(uint32_t level, uint32_t* out_resource_level) noexcept {
  if (!out_resource_level) return ARX_INVALID_DATA_POINTER;
  *out_resource_level = pistoris::paths::minimapResourceLevel(level);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_level_minimap(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelMinimap(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_loading_screen(uint32_t level, char* out, size_t capacity,
                                                     size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelLoadingScreen(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_minimap_offsets_file(ArxStringView* out_path) noexcept {
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = pistoris::c_api::view(pistoris::paths::minimapOffsetsFile());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_level_from_dlf(ArxStringView path, uint32_t* out_level) noexcept {
  return parseLevel(path, out_level, pistoris::paths::levelFromDlf);
}

ArxReturnCode arx_pistoris_path_level_from_llf(ArxStringView path, uint32_t* out_level) noexcept {
  return parseLevel(path, out_level, pistoris::paths::levelFromLlf);
}

ArxReturnCode arx_pistoris_path_level_from_fts(ArxStringView path, uint32_t* out_level) noexcept {
  return parseLevel(path, out_level, pistoris::paths::levelFromFts);
}

ArxReturnCode arx_pistoris_path_dlf_scene_from_level_name(ArxStringView name, char* out, size_t capacity,
                                                          size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(name)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::dlfSceneFromLevelName(pistoris::c_api::stringView(name), result);
  });
}

ArxReturnCode arx_pistoris_path_level_selector(uint32_t level, char* out, size_t capacity, size_t* out_size) noexcept {
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    result = pistoris::paths::levelSelector(level);
    return true;
  });
}

ArxReturnCode arx_pistoris_path_level_from_selector(ArxStringView selector, uint32_t* out_level) noexcept {
  return parseLevel(selector, out_level, pistoris::paths::levelFromSelector);
}

ArxReturnCode arx_pistoris_path_level_search_location(ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = cView(pistoris::paths::levelSearchLocation());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_fts_from_dlf_scene(ArxStringView scene_path, char* out, size_t capacity,
                                                   size_t* out_size) noexcept {
  if (!pistoris::c_api::valid(scene_path)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ftsFromDlfScene(pistoris::c_api::stringView(scene_path), result);
  });
}

// NOLINTEND(readability-identifier-naming)
