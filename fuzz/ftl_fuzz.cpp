// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxFtl* raw_ftl = nullptr;
  const ArxReturnCode rc = arx_pistoris_ftl_read(data, size, &raw_ftl);
  arx_fuzz::FtlHandle ftl(raw_ftl);
  if (rc != ARX_OK) {
    if (ftl.get()) std::abort();
    return 0;
  }
  if (!ftl.get()) std::abort();
  return 0;
}
