// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/ftl.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/json_internal.h"
#include "external/json.h"
#include "native/ftl.h"
#include "native/storage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ftl_read(const uint8_t* data, size_t size, ArxFtl** out_ftl) noexcept {
  if (!data || !out_ftl) return ARX_INVALID_DATA_POINTER;
  *out_ftl = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxFtl>();
    ArxReturnCode rc = pistoris::loadFtlStorage(&result->value, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;
    *out_ftl = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ftl_write(const ArxFtl* ftl, uint32_t compress, uint8_t** out_data,
                                     size_t* out_size) noexcept {
  if (!ftl) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::vector<std::uint8_t> result;
    ArxReturnCode rc = pistoris::saveFtlStorage(&ftl->value, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_ftl_to_json(const ArxFtl* ftl, std::uint32_t pretty, char** out_json) noexcept {
  return pistoris::c_api::toJson(ftl, pretty, out_json, pistoris::exportFtlToJson);
}

ArxReturnCode arx_pistoris_ftl_from_json(const std::uint8_t* data, std::size_t size, ArxFtl** out_ftl) noexcept {
  return pistoris::c_api::fromJson(data, size, out_ftl, pistoris::importJsonToFtl);
}

ArxReturnCode arx_pistoris_ftl_validate(const ArxFtl* ftl) noexcept {
  if (!ftl) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateFtl(&ftl->value); });
}

void arx_pistoris_ftl_destroy(ArxFtl* ftl) noexcept { delete ftl; }

// NOLINTEND(readability-identifier-naming)
