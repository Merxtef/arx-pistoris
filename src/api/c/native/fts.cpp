// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/fts.h"

#include "arx_pistoris/base/status.h"
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
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_fts_read(const uint8_t* data, size_t size, ArxFts** out_fts) noexcept {
  if (!data || !out_fts) return ARX_INVALID_DATA_POINTER;
  *out_fts = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto fts = std::make_unique<ArxFts>();
    ArxReturnCode rc = pistoris::loadFtsStorage(&fts->value, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;
    *out_fts = fts.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_fts_write(const ArxFts* fts, uint32_t compress, uint8_t** out_data,
                                     size_t* out_size) noexcept {
  if (!fts) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> result;
    ArxReturnCode rc = pistoris::saveFtsStorage(&fts->value, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_fts_to_json(const ArxFts* fts, std::uint32_t pretty, char** out_json) noexcept {
  return pistoris::c_api::toJson(fts, pretty, out_json, pistoris::exportFtsToJson);
}

ArxReturnCode arx_pistoris_fts_from_json(const std::uint8_t* data, std::size_t size, ArxFts** out_fts) noexcept {
  return pistoris::c_api::fromJson(data, size, out_fts, pistoris::importJsonToFts);
}

ArxReturnCode arx_pistoris_fts_validate(const ArxFts* fts) noexcept {
  if (!fts) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateFts(&fts->value); });
}

void arx_pistoris_fts_destroy(ArxFts* fts) noexcept { delete fts; }

// NOLINTEND(readability-identifier-naming)
