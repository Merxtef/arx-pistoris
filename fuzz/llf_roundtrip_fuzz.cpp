// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxLlf* raw_source = nullptr;
  const ArxReturnCode source_rc = arx_pistoris_llf_read(data, size, &raw_source);
  arx_fuzz::LlfHandle source(raw_source);
  if (source_rc != ARX_OK) {
    if (source.get()) std::abort();
    return 0;
  }
  if (!source.get()) std::abort();

  arx_fuzz::ByteBuffer bytes;
  const ArxLlfWriteOptions options = ARX_LLF_WRITE_OPTIONS_INIT;
  if (arx_pistoris_llf_write(source.get(), &options, 0, &bytes.value, &bytes.byte_count) != ARX_OK) std::abort();

  ArxLlf* raw_roundtrip = nullptr;
  const ArxReturnCode read_rc = arx_pistoris_llf_read(bytes.get(), bytes.size(), &raw_roundtrip);
  arx_fuzz::LlfHandle roundtrip(raw_roundtrip);
  if (read_rc != ARX_OK || !roundtrip.get()) std::abort();
  return 0;
}
