// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxCin* raw_cin = nullptr;
  const ArxReturnCode rc = arx_pistoris_cin_read(data, size, &raw_cin);
  arx_fuzz::CinHandle cin(raw_cin);
  if (rc != ARX_OK) {
    if (cin.get()) std::abort();
    return 0;
  }
  if (!cin.get()) std::abort();
  if (arx_pistoris_cin_validate(cin.get()) != ARX_OK) std::abort();
  return 0;
}
