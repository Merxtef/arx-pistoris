// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/amb.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/json_internal.h"
#include "external/json.h"
#include "utils/cursor.h"

#include <cstddef>
#include <cstdint>
#include <memory>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_amb_read(const uint8_t* data, size_t size, ArxAmb** out_amb) noexcept {
  if (!data || !out_amb) return ARX_INVALID_DATA_POINTER;
  *out_amb = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto amb = std::make_unique<ArxAmb>();
    pistoris::ReadCursor cursor(data, size);
    ArxReturnCode rc = pistoris::loadAmb(&amb->value, cursor);
    if (rc != ARX_OK) return rc;
    *out_amb = amb.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_amb_write(const ArxAmb* amb, uint8_t** out_data, size_t* out_size) noexcept {
  if (!amb) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    pistoris::WriteCursor cursor;
    ArxReturnCode rc = pistoris::saveAmb(&amb->value, cursor);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(cursor.take(), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_amb_to_json(const ArxAmb* amb, std::uint32_t pretty, char** out_json) noexcept {
  return pistoris::c_api::toJson(amb, pretty, out_json, pistoris::exportAmbToJson);
}

ArxReturnCode arx_pistoris_amb_from_json(const std::uint8_t* data, std::size_t size, ArxAmb** out_amb) noexcept {
  return pistoris::c_api::fromJson(data, size, out_amb, pistoris::importJsonToAmb);
}

ArxReturnCode arx_pistoris_amb_validate(const ArxAmb* amb) noexcept {
  if (!amb) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateAmb(&amb->value); });
}

void arx_pistoris_amb_destroy(ArxAmb* amb) noexcept { delete amb; }

// NOLINTEND(readability-identifier-naming)
