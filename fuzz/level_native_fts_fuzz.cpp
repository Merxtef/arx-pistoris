// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  const ArxLlf* fixed_llf = arx_fuzz::level40Llf();
  const ArxDlf* fixed_dlf = arx_fuzz::level40Dlf();
  if (!fixed_llf || !fixed_dlf) return 0;

  ArxFts* raw_fts = nullptr;
  if (arx_pistoris_fts_parse(data, size, &raw_fts) != ARX_OK) return 0;
  arx_fuzz::FtsHandle fts(raw_fts);
  if (!fts.get()) std::abort();
  arx_fuzz::exerciseLevelFromNative(fts.get(), fixed_llf, fixed_dlf);
  return 0;
}
