// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_fts_read(const uint8_t* data, size_t size, ArxFts** out_fts, ArxError* error) noexcept {
  return pistoris::c_api::readNative(data, size, out_fts, error, pistoris::readFts);
}

ArxReturnCode arx_pistoris_fts_write(const ArxFts* fts, uint32_t compress, uint8_t** out_data, size_t* out_size,
                                     ArxError* error) noexcept {
  return pistoris::c_api::writeNative(fts, out_data, out_size, error, [&](const pistoris::Fts& value) {
    return pistoris::writeFts(value, compress != 0);
  });
}

ArxReturnCode arx_pistoris_fts_to_json(const ArxFts* fts, std::uint32_t level, std::uint32_t pretty,
                                       ArxNativeTextMode text_mode, char** out_json, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::nativeToJson(fts, out_json, error, [&](const pistoris::Fts& value) {
    return pistoris::toFtsJson(value, level, pretty != 0, pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_fts_from_json(const std::uint8_t* data, std::size_t size, ArxNativeTextMode text_mode,
                                         ArxFts** out_fts, std::uint32_t* out_level, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_level) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_level = 0;
  std::uint32_t imported_level = 0;
  const ArxReturnCode code = pistoris::c_api::nativeFromJson(data, size, out_fts, error, [&](std::string_view json) {
    auto imported = pistoris::fromFtsJson(json, pistoris::c_api::nativeTextMode(text_mode));
    if (!imported) return std::move(imported).template propagate<pistoris::Fts>();
    imported_level = imported->level;
    return pistoris::JsonResult<pistoris::Fts>::success(std::move(imported->fts));
  });
  if (code == ARX_OK) *out_level = imported_level;
  return code;
}

ArxReturnCode arx_pistoris_fts_validate(const ArxFts* fts, ArxError* error) noexcept {
  return pistoris::c_api::validateNative(
      fts, error, [](const pistoris::Fts& value) { return pistoris::validate(value); });
}

void arx_pistoris_fts_destroy(ArxFts* fts) noexcept { delete fts; }

// NOLINTEND(readability-identifier-naming)
