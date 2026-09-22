// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxCin* raw_source = nullptr;
  const ArxReturnCode source_rc = arx_pistoris_cin_read(data, size, &raw_source);
  arx_fuzz::CinHandle source(raw_source);
  if (source_rc != ARX_OK) {
    if (source.get()) std::abort();
    return 0;
  }
  if (!source.get()) std::abort();

  arx_fuzz::ByteBuffer bytes1;
  if (arx_pistoris_cin_write(source.get(), &bytes1.value, &bytes1.byte_count) != ARX_OK) std::abort();

  ArxCin* raw_roundtrip = nullptr;
  const ArxReturnCode roundtrip_rc = arx_pistoris_cin_read(bytes1.get(), bytes1.size(), &raw_roundtrip);
  arx_fuzz::CinHandle roundtrip(raw_roundtrip);
  if (roundtrip_rc != ARX_OK || !roundtrip.get()) std::abort();

  arx_fuzz::ByteBuffer bytes2;
  if (arx_pistoris_cin_write(roundtrip.get(), &bytes2.value, &bytes2.byte_count) != ARX_OK) std::abort();
  if (bytes1.size() != bytes2.size()) std::abort();
  if (bytes1.size() != 0 && std::memcmp(bytes1.get(), bytes2.get(), bytes1.size()) != 0) std::abort();
  return 0;
}
