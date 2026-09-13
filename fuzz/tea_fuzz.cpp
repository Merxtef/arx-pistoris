// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxTea* raw_tea = nullptr;
  const ArxReturnCode rc = arx_pistoris_tea_read(data, size, &raw_tea);
  arx_fuzz::TeaHandle tea(raw_tea);
  if (rc != ARX_OK) {
    if (tea.get()) std::abort();
    return 0;
  }
  if (!tea.get()) std::abort();
  return 0;
}
