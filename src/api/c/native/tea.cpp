// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/tea.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/json_internal.h"
#include "external/json.h"
#include "native/tea.h"
#include "utils/cursor.h"

#include <cstddef>
#include <cstdint>
#include <memory>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_tea_read(const uint8_t* data, size_t size, ArxTea** out_tea) noexcept {
  if (!data || !out_tea) return ARX_INVALID_DATA_POINTER;
  *out_tea = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxTea>();
    pistoris::ReadCursor cursor(data, size);
    ArxReturnCode rc = pistoris::loadTea(&result->value, cursor);
    if (rc != ARX_OK) return rc;
    *out_tea = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_tea_write(const ArxTea* tea, uint8_t** out_data, size_t* out_size) noexcept {
  if (!tea) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::WriteCursor cursor;
    ArxReturnCode rc = pistoris::saveTea(&tea->value, cursor);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(cursor.take(), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_tea_to_json(const ArxTea* tea, std::uint32_t pretty, char** out_json) noexcept {
  return pistoris::c_api::toJson(tea, pretty, out_json, pistoris::exportTeaToJson);
}

ArxReturnCode arx_pistoris_tea_from_json(const std::uint8_t* data, std::size_t size, ArxTea** out_tea) noexcept {
  return pistoris::c_api::fromJson(data, size, out_tea, pistoris::importJsonToTea);
}

ArxReturnCode arx_pistoris_tea_validate(const ArxTea* tea) noexcept {
  if (!tea) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateTea(&tea->value); });
}

void arx_pistoris_tea_destroy(ArxTea* tea) noexcept { delete tea; }

// NOLINTEND(readability-identifier-naming)
