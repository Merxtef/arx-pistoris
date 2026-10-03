// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/cin.hpp"

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native.hpp"

#include "api/c/native/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_cin_read(const uint8_t* data, size_t size, ArxCin** out_cin, ArxError* error) noexcept {
  return pistoris::c_api::readNative(data, size, out_cin, error, pistoris::readCin);
}

ArxReturnCode arx_pistoris_cin_write(const ArxCin* cin, uint8_t** out_data, size_t* out_size,
                                     ArxError* error) noexcept {
  return pistoris::c_api::writeNative(
      cin, out_data, out_size, error, [](const pistoris::Cin& value) { return pistoris::writeCin(value); });
}

ArxReturnCode arx_pistoris_cin_validate(const ArxCin* cin, ArxError* error) noexcept {
  return pistoris::c_api::validateNative(
      cin, error, [](const pistoris::Cin& value) { return pistoris::validate(value); });
}

void arx_pistoris_cin_destroy(ArxCin* cin) noexcept { delete cin; }

// NOLINTEND(readability-identifier-naming)
