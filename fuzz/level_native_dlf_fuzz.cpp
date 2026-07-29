// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  const ArxFts* fixed_fts = arx_fuzz::level40Fts();
  const ArxLlf* fixed_llf = arx_fuzz::level40Llf();
  if (!fixed_fts || !fixed_llf) return 0;

  ArxDlf* raw_dlf = nullptr;
  ArxLlf* raw_embedded_lighting = nullptr;
  if (arx_pistoris_dlf_parse(data, size, &raw_dlf, &raw_embedded_lighting) != ARX_OK) return 0;
  arx_fuzz::DlfHandle dlf(raw_dlf);
  arx_fuzz::LlfHandle embedded_lighting(raw_embedded_lighting);
  if (!dlf.get()) std::abort();
  arx_fuzz::exerciseLevelFromNative(fixed_fts, fixed_llf, dlf.get());
  return 0;
}
