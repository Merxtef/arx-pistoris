// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native.hpp"

#include "api/c/internal.h"
#include "api/c/native/internal.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_llf_read(const uint8_t* data, size_t size, ArxLlf** out_llf, ArxError* error) noexcept {
  return pistoris::c_api::readNative(data, size, out_llf, error, pistoris::readLlf);
}

ArxReturnCode arx_pistoris_llf_write(const ArxLlf* llf, const ArxLlfWriteOptions* options, uint32_t compress,
                                     uint8_t** out_data, size_t* out_size, ArxError* error) noexcept {
  if (options && !pistoris::c_api::valid(options->signer))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::writeNative(llf, out_data, out_size, error, [&](const pistoris::Llf& value) {
    pistoris::LlfWriteOptions cpp_options;
    if (options) cpp_options.signer = pistoris::c_api::stringView(options->signer);
    return pistoris::writeLlf(value, cpp_options, compress != 0);
  });
}

ArxReturnCode arx_pistoris_llf_to_json(const ArxLlf* llf, std::uint32_t pretty, ArxStringView signer, char** out_json,
                                       ArxError* error) noexcept {
  if (!pistoris::c_api::valid(signer)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::nativeToJson(llf, out_json, error, [&](const pistoris::Llf& value) {
    return pistoris::toLlfJson(value, pretty != 0, pistoris::c_api::stringView(signer));
  });
}

ArxReturnCode arx_pistoris_llf_from_json(const std::uint8_t* data, std::size_t size, ArxLlf** out_llf,
                                         ArxError* error) noexcept {
  return pistoris::c_api::nativeFromJson(
      data, size, out_llf, error, [](std::string_view json) { return pistoris::fromLlfJson(json); });
}

ArxReturnCode arx_pistoris_llf_validate(const ArxLlf* llf, ArxError* error) noexcept {
  return pistoris::c_api::validateNative(
      llf, error, [](const pistoris::Llf& value) { return pistoris::validate(value); });
}

void arx_pistoris_llf_destroy(ArxLlf* llf) noexcept { delete llf; }

// NOLINTEND(readability-identifier-naming)
