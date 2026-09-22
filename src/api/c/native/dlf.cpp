// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/dlf.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/text.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/json_internal.h"
#include "api/c/native/text_internal.h"
#include "external/json.h"
#include "native/storage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_dlf_read(const uint8_t* data, size_t size, ArxDlf** out_dlf,
                                    ArxLlf** out_embedded_llf) noexcept {
  if (!data || !out_dlf) return ARX_INVALID_DATA_POINTER;
  *out_dlf = nullptr;
  if (out_embedded_llf) *out_embedded_llf = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto dlf = std::make_unique<ArxDlf>();
    std::optional<pistoris::llf::Data> embedded;
    ArxReturnCode rc = pistoris::loadDlfStorage(
        &dlf->value, out_embedded_llf ? &embedded : nullptr, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;

    std::unique_ptr<ArxLlf> llf;
    if (embedded) {
      llf = std::make_unique<ArxLlf>();
      llf->value = std::move(*embedded);
    }
    *out_dlf = dlf.release();
    if (out_embedded_llf) *out_embedded_llf = llf.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_dlf_write(const ArxDlf* dlf, const ArxDlfWriteOptions* options, uint32_t compress,
                                     uint8_t** out_data, size_t* out_size) noexcept {
  if (!dlf) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  if (options && !pistoris::c_api::valid(options->signer)) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> result;
    const pistoris::llf::Data* lighting = options && options->embedded_llf ? &options->embedded_llf->value : nullptr;
    const std::string_view signer = options ? pistoris::c_api::stringView(options->signer) : std::string_view{};
    ArxReturnCode rc = pistoris::saveDlfStorage(&dlf->value, lighting, signer, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_dlf_to_json(const ArxDlf* dlf, std::uint32_t pretty, ArxStringView signer,
                                       ArxNativeTextMode text_mode, char** out_json) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return ARX_INVALID_OPTIONS;
  return pistoris::c_api::toJson(
      dlf, pretty, signer, pistoris::c_api::nativeTextMode(text_mode), out_json, pistoris::exportDlfToJson);
}

ArxReturnCode arx_pistoris_dlf_from_json(const std::uint8_t* data, std::size_t size, ArxNativeTextMode text_mode,
                                         ArxDlf** out_dlf) noexcept {
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return ARX_INVALID_OPTIONS;
  return pistoris::c_api::fromJson(
      data, size, pistoris::c_api::nativeTextMode(text_mode), out_dlf, pistoris::importJsonToDlf);
}

ArxReturnCode arx_pistoris_dlf_validate(const ArxDlf* dlf) noexcept {
  if (!dlf) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateDlf(&dlf->value); });
}

void arx_pistoris_dlf_destroy(ArxDlf* dlf) noexcept { delete dlf; }

// NOLINTEND(readability-identifier-naming)
