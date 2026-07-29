// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxTeaHandle raw_tea = nullptr;

  const ArxReturnCode rc = arx_pistoris_tea_from_json(data, size, &raw_tea);
  if (rc == ARX_OK) {
    arx_fuzz::TeaHandle tea(raw_tea);
    if (!tea.get()) std::abort();
  }
  return 0;
}
