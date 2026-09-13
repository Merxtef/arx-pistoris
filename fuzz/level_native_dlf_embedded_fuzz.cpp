// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  const ArxFts* fixed_fts = arx_fuzz::fixtureLevelFts();

  ArxDlf* raw_dlf = nullptr;
  ArxLlf* raw_embedded_lighting = nullptr;
  const ArxReturnCode rc = arx_pistoris_dlf_read(data, size, &raw_dlf, &raw_embedded_lighting);
  arx_fuzz::DlfHandle dlf(raw_dlf);
  arx_fuzz::LlfHandle embedded_lighting(raw_embedded_lighting);
  if (rc != ARX_OK) {
    if (dlf.get() || embedded_lighting.get()) std::abort();
    return 0;
  }
  if (!dlf.get()) std::abort();
  arx_fuzz::exerciseLevelFromNative(fixed_fts, embedded_lighting.get(), dlf.get());
  return 0;
}
