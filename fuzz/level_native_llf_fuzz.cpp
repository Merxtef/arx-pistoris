// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  const ArxFts* fixed_fts = arx_fuzz::fixtureLevelFts();
  const ArxDlf* fixed_dlf = arx_fuzz::fixtureLevelDlf();

  ArxLlf* raw_llf = nullptr;
  const ArxReturnCode rc = arx_pistoris_llf_read(data, size, &raw_llf);
  arx_fuzz::LlfHandle llf(raw_llf);
  if (rc != ARX_OK) {
    if (llf.get()) std::abort();
    return 0;
  }
  if (!llf.get()) std::abort();
  arx_fuzz::exerciseLevelFromNative(fixed_fts, llf.get(), fixed_dlf);
  return 0;
}
