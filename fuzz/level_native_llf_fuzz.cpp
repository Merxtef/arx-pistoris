// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  const ArxFts* fixed_fts = arx_fuzz::level40Fts();
  const ArxDlf* fixed_dlf = arx_fuzz::level40Dlf();
  if (!fixed_fts || !fixed_dlf) return 0;

  ArxLlf* raw_llf = nullptr;
  if (arx_pistoris_llf_parse(data, size, &raw_llf) != ARX_OK) return 0;
  arx_fuzz::LlfHandle llf(raw_llf);
  if (!llf.get()) std::abort();
  arx_fuzz::exerciseLevelFromNative(fixed_fts, llf.get(), fixed_dlf);
  return 0;
}
