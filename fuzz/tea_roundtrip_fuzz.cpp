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
  ArxTea* raw_source = nullptr;
  const ArxReturnCode source_rc = arx_pistoris_tea_read(data, size, &raw_source);
  arx_fuzz::TeaHandle source(raw_source);
  if (source_rc != ARX_OK) {
    if (source.get()) std::abort();
    return 0;
  }
  if (!source.get()) std::abort();

  arx_fuzz::ByteBuffer bytes1;
  if (arx_pistoris_tea_write(source.get(), &bytes1.value, &bytes1.byte_count) != ARX_OK) std::abort();

  ArxTea* raw_roundtrip = nullptr;
  const ArxReturnCode reparse_rc = arx_pistoris_tea_read(bytes1.get(), bytes1.size(), &raw_roundtrip);
  arx_fuzz::TeaHandle roundtrip(raw_roundtrip);
  if (reparse_rc != ARX_OK || !roundtrip.get()) std::abort();

  arx_fuzz::ByteBuffer bytes2;
  if (arx_pistoris_tea_write(roundtrip.get(), &bytes2.value, &bytes2.byte_count) != ARX_OK) std::abort();
  if (bytes1.size() != bytes2.size()) std::abort();
  if (bytes1.size() != 0 && std::memcmp(bytes1.get(), bytes2.get(), bytes1.size()) != 0) std::abort();

  return 0;
}
