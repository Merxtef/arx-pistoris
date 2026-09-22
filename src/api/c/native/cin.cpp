// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/cin.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"

#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "native/cin.h"
#include "utils/cursor.h"

#include <cstddef>
#include <cstdint>
#include <memory>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_cin_read(const uint8_t* data, size_t size, ArxCin** out_cin) noexcept {
  if (!data || !out_cin) return ARX_INVALID_DATA_POINTER;
  *out_cin = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxCin>();
    pistoris::ReadCursor cursor(data, size);
    const ArxReturnCode rc = pistoris::loadCin(&result->value, cursor);
    if (rc != ARX_OK) return rc;
    *out_cin = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_cin_write(const ArxCin* cin, uint8_t** out_data, size_t* out_size) noexcept {
  if (!cin) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::WriteCursor cursor;
    const ArxReturnCode rc = pistoris::saveCin(&cin->value, cursor);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(cursor.take(), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_cin_validate(const ArxCin* cin) noexcept {
  if (!cin) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateCin(&cin->value); });
}

void arx_pistoris_cin_destroy(ArxCin* cin) noexcept { delete cin; }

// NOLINTEND(readability-identifier-naming)
