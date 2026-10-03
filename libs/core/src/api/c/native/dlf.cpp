// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/text.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_dlf_read(const uint8_t* data, size_t size, ArxDlf** out_dlf, ArxLlf** out_embedded_llf,
                                    ArxError* error) noexcept {
  if (!data || !out_dlf) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_dlf = nullptr;
  if (out_embedded_llf) *out_embedded_llf = nullptr;

  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto loaded = pistoris::readDlf(std::span<const std::uint8_t>(data, size));
    if (!loaded) return pistoris::c_api::publish(loaded, error);
    auto dlf = std::make_unique<ArxDlf>();
    dlf->value = std::move(loaded->dlf);

    std::unique_ptr<ArxLlf> llf;
    if (out_embedded_llf && loaded->embedded_lighting) {
      llf = std::make_unique<ArxLlf>();
      llf->value = std::move(*loaded->embedded_lighting);
    }
    *out_dlf = dlf.release();
    if (out_embedded_llf) *out_embedded_llf = llf.release();
    return pistoris::c_api::publish(loaded, error);
  });
}

ArxReturnCode arx_pistoris_dlf_write(const ArxDlf* dlf, const ArxDlfWriteOptions* options, uint32_t compress,
                                     uint8_t** out_data, size_t* out_size, ArxError* error) noexcept {
  if (options && !pistoris::c_api::valid(options->signer))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::writeNative(dlf, out_data, out_size, error, [&](const pistoris::Dlf& value) {
    pistoris::DlfWriteOptions cpp_options;
    if (options) {
      cpp_options.embedded_lighting = options->embedded_llf ? &options->embedded_llf->value : nullptr;
      cpp_options.signer = pistoris::c_api::stringView(options->signer);
    }
    return pistoris::writeDlf(value, cpp_options, compress != 0);
  });
}

ArxReturnCode arx_pistoris_dlf_to_json(const ArxDlf* dlf, std::uint32_t pretty, ArxStringView signer,
                                       ArxNativeTextMode text_mode, char** out_json, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!pistoris::c_api::valid(signer)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::nativeToJson(dlf, out_json, error, [&](const pistoris::Dlf& value) {
    return pistoris::toDlfJson(
        value, pretty != 0, pistoris::c_api::stringView(signer), pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_dlf_from_json(const std::uint8_t* data, std::size_t size, ArxNativeTextMode text_mode,
                                         ArxDlf** out_dlf, ArxError* error) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::nativeFromJson(data, size, out_dlf, error, [&](std::string_view json) {
    return pistoris::fromDlfJson(json, pistoris::c_api::nativeTextMode(text_mode));
  });
}

ArxReturnCode arx_pistoris_dlf_validate(const ArxDlf* dlf, ArxError* error) noexcept {
  return pistoris::c_api::validateNative(
      dlf, error, [](const pistoris::Dlf& value) { return pistoris::validate(value); });
}

void arx_pistoris_dlf_destroy(ArxDlf* dlf) noexcept { delete dlf; }

// NOLINTEND(readability-identifier-naming)
