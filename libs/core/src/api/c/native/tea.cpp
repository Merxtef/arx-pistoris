// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/tea.hpp"

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_tea_read(const uint8_t* data, size_t size, ArxTea** out_tea, ArxError* error) noexcept {
  return pistoris::c_api::readNative(data, size, out_tea, error, pistoris::readTea);
}

ArxReturnCode arx_pistoris_tea_write(const ArxTea* tea, uint8_t** out_data, size_t* out_size,
                                     ArxError* error) noexcept {
  return pistoris::c_api::writeNative(
      tea, out_data, out_size, error, [](const pistoris::Tea& value) { return pistoris::writeTea(value); });
}

ArxReturnCode arx_pistoris_tea_to_json(const ArxTea* tea, std::uint32_t pretty, ArxNativeTextMode text_mode,
                                       char** out_json, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::nativeToJson(tea, out_json, error, [&](const pistoris::Tea& value) {
    return pistoris::toTeaJson(value, pretty != 0, pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_tea_from_json(const std::uint8_t* data, std::size_t size, ArxNativeTextMode text_mode,
                                         ArxTea** out_tea, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::nativeFromJson(data, size, out_tea, error, [&](std::string_view json) {
    return pistoris::fromTeaJson(json, pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_tea_validate(const ArxTea* tea, ArxError* error) noexcept {
  return pistoris::c_api::validateNative(
      tea, error, [](const pistoris::Tea& value) { return pistoris::validate(value); });
}

void arx_pistoris_tea_destroy(ArxTea* tea) noexcept { delete tea; }

// NOLINTEND(readability-identifier-naming)
