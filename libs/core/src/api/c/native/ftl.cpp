// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/ftl.hpp"

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

ArxReturnCode arx_pistoris_ftl_read(const uint8_t* data, size_t size, ArxFtl** out_ftl, ArxError* error) noexcept {
  return pistoris::c_api::readNative(data, size, out_ftl, error, pistoris::readFtl);
}

ArxReturnCode arx_pistoris_ftl_write(const ArxFtl* ftl, uint32_t compress, uint8_t** out_data, size_t* out_size,
                                     ArxError* error) noexcept {
  return pistoris::c_api::writeNative(ftl, out_data, out_size, error, [&](const pistoris::Ftl& value) {
    return pistoris::writeFtl(value, compress != 0);
  });
}

ArxReturnCode arx_pistoris_ftl_to_json(const ArxFtl* ftl, std::uint32_t pretty, ArxNativeTextMode text_mode,
                                       char** out_json, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::nativeToJson(ftl, out_json, error, [&](const pistoris::Ftl& value) {
    return pistoris::toFtlJson(value, pretty != 0, pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_ftl_from_json(const std::uint8_t* data, std::size_t size, ArxNativeTextMode text_mode,
                                         ArxFtl** out_ftl, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::nativeFromJson(data, size, out_ftl, error, [&](std::string_view json) {
    return pistoris::fromFtlJson(json, pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_ftl_validate(const ArxFtl* ftl, ArxError* error) noexcept {
  return pistoris::c_api::validateNative(
      ftl, error, [](const pistoris::Ftl& value) { return pistoris::validate(value); });
}

void arx_pistoris_ftl_destroy(ArxFtl* ftl) noexcept { delete ftl; }

// NOLINTEND(readability-identifier-naming)
