// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_pistoris.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  ArxFtlHandle h1 = nullptr;
  if (arx_pistoris_ftl_parse(data, size, &h1) != ARX_OK) return 0;

  uint8_t* buf1    = nullptr;
  std::size_t len1 = 0;
  if (arx_pistoris_ftl_write(h1, &buf1, &len1) != ARX_OK) {
    arx_pistoris_ftl_free(h1);
    return 0;
  }

  // written bytes must re-parse cleanly
  ArxFtlHandle h2 = nullptr;
  if (arx_pistoris_ftl_parse(buf1, len1, &h2) != ARX_OK) abort();

  // second write must be byte-identical (write is deterministic)
  uint8_t* buf2    = nullptr;
  std::size_t len2 = 0;
  if (arx_pistoris_ftl_write(h2, &buf2, &len2) != ARX_OK) abort();
  if (len1 != len2 || std::memcmp(buf1, buf2, len1) != 0) abort();

  arx_pistoris_free_bytes(buf2);
  arx_pistoris_free_bytes(buf1);
  arx_pistoris_ftl_free(h2);
  arx_pistoris_ftl_free(h1);
  return 0;
}
