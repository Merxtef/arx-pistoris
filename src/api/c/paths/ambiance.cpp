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

ArxReturnCode arx_pistoris_path_ambiance_sound_directory(ArxStringView* out_directory) noexcept {
  if (!out_directory) return ARX_INVALID_DATA_POINTER;
  *out_directory = pistoris::c_api::view(pistoris::paths::ambianceSoundDirectory());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_ambiance_amb(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                             size_t* out_size) noexcept {
  if (!valid(ambiance)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ambianceAmb({pistoris::c_api::stringView(ambiance.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_ambiance_from_amb(ArxStringView path, ArxAmbiancePathView* out_ambiance) noexcept {
  if (!pistoris::c_api::valid(path) || !out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AmbiancePathView result;
    if (!pistoris::paths::ambianceFromAmb(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_ambiance = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_ambiance_selector(ArxAmbiancePathView ambiance, char* out, size_t capacity,
                                                  size_t* out_size) noexcept {
  if (!valid(ambiance)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::ambianceSelector({pistoris::c_api::stringView(ambiance.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_ambiance_from_selector(ArxStringView selector,
                                                       ArxAmbiancePathView* out_ambiance) noexcept {
  if (!pistoris::c_api::valid(selector) || !out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::AmbiancePathView result;
    if (!pistoris::paths::ambianceFromSelector(pistoris::c_api::stringView(selector), result))
      return ARX_INVALID_IDENTIFIER;
    *out_ambiance = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_ambiance_search_location(ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = cView(pistoris::paths::ambianceSearchLocation());
  return ARX_OK;
}

// NOLINTEND(readability-identifier-naming)
