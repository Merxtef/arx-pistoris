// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxFts* raw_source = nullptr;
  const ArxReturnCode source_rc = arx_pistoris_fts_read(data, size, &raw_source);
  arx_fuzz::FtsHandle source(raw_source);
  if (source_rc != ARX_OK) {
    if (source.get()) std::abort();
    return 0;
  }
  if (!source.get()) std::abort();

  arx_fuzz::ByteBuffer bytes;
  if (arx_pistoris_fts_write(source.get(), 0, &bytes.value, &bytes.byte_count) != ARX_OK) std::abort();

  ArxFts* raw_roundtrip = nullptr;
  const ArxReturnCode read_rc = arx_pistoris_fts_read(bytes.get(), bytes.size(), &raw_roundtrip);
  arx_fuzz::FtsHandle roundtrip(raw_roundtrip);
  if (read_rc != ARX_OK || !roundtrip.get()) std::abort();
  return 0;
}
