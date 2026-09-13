// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxAmb* raw_amb = nullptr;
  const ArxReturnCode rc = arx_pistoris_amb_read(data, size, &raw_amb);
  arx_fuzz::AmbHandle amb(raw_amb);
  if (rc != ARX_OK) {
    if (amb.get()) std::abort();
    return 0;
  }
  if (!amb.get()) std::abort();
  return 0;
}
