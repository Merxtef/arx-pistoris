// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_pistoris.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  ArxFtlHandle h   = nullptr;
  ArxReturnCode rc = arx_pistoris_ftl_parse(data, size, &h);
  if (rc == ARX_OK) {
    arx_pistoris_ftl_free(h);
  }
  return 0;
}
