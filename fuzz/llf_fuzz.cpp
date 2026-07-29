// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxLlf* raw_llf = nullptr;
  const ArxReturnCode rc = arx_pistoris_llf_parse(data, size, &raw_llf);
  if (rc == ARX_OK) {
    arx_fuzz::LlfHandle llf(raw_llf);
    if (!llf.get()) std::abort();
  }
  return 0;
}
