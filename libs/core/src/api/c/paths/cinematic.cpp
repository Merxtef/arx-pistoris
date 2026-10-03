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

ArxReturnCode arx_pistoris_path_cinematic_illustration_directory(ArxStringView* out_directory) noexcept {
  if (!out_directory) return ARX_INVALID_DATA_POINTER;
  *out_directory = pistoris::c_api::view(pistoris::paths::cinematicIllustrationDirectory());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_path_cinematic_cin(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                              size_t* out_size) noexcept {
  if (!valid(cinematic)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::cinematicCin({pistoris::c_api::stringView(cinematic.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_cinematic_from_cin(ArxStringView path, ArxCinematicPathView* out_cinematic) noexcept {
  if (!pistoris::c_api::valid(path) || !out_cinematic) return ARX_INVALID_DATA_POINTER;
  *out_cinematic = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::CinematicPathView result;
    if (!pistoris::paths::cinematicFromCin(pistoris::c_api::stringView(path), result)) return ARX_INVALID_IDENTIFIER;
    *out_cinematic = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_cinematic_selector(ArxCinematicPathView cinematic, char* out, size_t capacity,
                                                   size_t* out_size) noexcept {
  if (!valid(cinematic)) return ARX_INVALID_DATA_POINTER;
  return buildPath(out, capacity, out_size, [&](std::string& result) {
    return pistoris::paths::cinematicSelector({pistoris::c_api::stringView(cinematic.name)}, result);
  });
}

ArxReturnCode arx_pistoris_path_cinematic_from_selector(ArxStringView selector,
                                                        ArxCinematicPathView* out_cinematic) noexcept {
  if (!pistoris::c_api::valid(selector) || !out_cinematic) return ARX_INVALID_DATA_POINTER;
  *out_cinematic = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::paths::CinematicPathView result;
    if (!pistoris::paths::cinematicFromSelector(pistoris::c_api::stringView(selector), result))
      return ARX_INVALID_IDENTIFIER;
    *out_cinematic = cView(result);
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_path_cinematic_search_location(ArxResourceSearchLocation* out_location) noexcept {
  if (!out_location) return ARX_INVALID_DATA_POINTER;
  *out_location = cView(pistoris::paths::cinematicSearchLocation());
  return ARX_OK;
}

// NOLINTEND(readability-identifier-naming)
