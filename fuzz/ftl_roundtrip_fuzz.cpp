// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxFtlHandle raw_source = nullptr;
  if (arx_pistoris_ftl_parse(data, size, &raw_source) != ARX_OK) return 0;
  arx_fuzz::FtlHandle source(raw_source);
  if (!source.get()) std::abort();

  arx_fuzz::ByteBuffer bytes1;
  if (arx_pistoris_ftl_write(source.get(), 0, &bytes1.value, &bytes1.byte_count) != ARX_OK) return 0;

  ArxFtlHandle raw_roundtrip = nullptr;
  const ArxReturnCode roundtrip_rc = arx_pistoris_ftl_parse(bytes1.get(), bytes1.size(), &raw_roundtrip);
  if (roundtrip_rc != ARX_OK) std::abort();
  arx_fuzz::FtlHandle roundtrip(raw_roundtrip);
  if (!roundtrip.get()) std::abort();

  arx_fuzz::ByteBuffer bytes2;
  if (arx_pistoris_ftl_write(roundtrip.get(), 0, &bytes2.value, &bytes2.byte_count) != ARX_OK) std::abort();
  if (bytes1.size() != bytes2.size()) std::abort();
  if (bytes1.size() != 0 && std::memcmp(bytes1.get(), bytes2.get(), bytes1.size()) != 0) std::abort();

  return 0;
}
