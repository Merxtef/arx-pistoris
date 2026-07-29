// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxFts* raw_fts = nullptr;
  const ArxReturnCode rc = arx_pistoris_fts_parse(data, size, &raw_fts);
  if (rc == ARX_OK) {
    arx_fuzz::FtsHandle fts(raw_fts);
    if (!fts.get()) std::abort();
  }
  return 0;
}
