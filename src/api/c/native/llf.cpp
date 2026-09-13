// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/llf.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/json_internal.h"
#include "external/json.h"
#include "native/storage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_llf_read(const uint8_t* data, size_t size, ArxLlf** out_llf) noexcept {
  if (!data || !out_llf) return ARX_INVALID_DATA_POINTER;
  *out_llf = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto llf = std::make_unique<ArxLlf>();
    ArxReturnCode rc = pistoris::loadLlfStorage(&llf->value, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;
    *out_llf = llf.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_llf_write(const ArxLlf* llf, const ArxLlfWriteOptions* options, uint32_t compress,
                                     uint8_t** out_data, size_t* out_size) noexcept {
  if (!llf) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  if (options && !pistoris::c_api::valid(options->signer)) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> result;
    const std::string_view signer = options ? pistoris::c_api::stringView(options->signer) : std::string_view{};
    ArxReturnCode rc = pistoris::saveLlfStorage(&llf->value, signer, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_llf_to_json(const ArxLlf* llf, std::uint32_t pretty, ArxStringView signer,
                                       char** out_json) noexcept {
  return pistoris::c_api::toJson(llf, pretty, signer, out_json, pistoris::exportLlfToJson);
}

ArxReturnCode arx_pistoris_llf_from_json(const std::uint8_t* data, std::size_t size, ArxLlf** out_llf) noexcept {
  return pistoris::c_api::fromJson(data, size, out_llf, pistoris::importJsonToLlf);
}

ArxReturnCode arx_pistoris_llf_validate(const ArxLlf* llf) noexcept {
  if (!llf) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateLlf(&llf->value); });
}

void arx_pistoris_llf_destroy(ArxLlf* llf) noexcept { delete llf; }

// NOLINTEND(readability-identifier-naming)
