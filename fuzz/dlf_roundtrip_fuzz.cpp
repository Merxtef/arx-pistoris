// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxDlf* raw_source = nullptr;
  ArxLlf* raw_embedded_lighting = nullptr;
  const ArxReturnCode source_rc = arx_pistoris_dlf_read(data, size, &raw_source, &raw_embedded_lighting);
  arx_fuzz::DlfHandle source(raw_source);
  arx_fuzz::LlfHandle embedded_lighting(raw_embedded_lighting);
  if (source_rc != ARX_OK) {
    if (source.get() || embedded_lighting.get()) std::abort();
    return 0;
  }
  if (!source.get()) std::abort();

  arx_fuzz::ByteBuffer bytes;
  ArxDlfWriteOptions options = ARX_DLF_WRITE_OPTIONS_INIT;
  options.embedded_llf = embedded_lighting.get();
  if (arx_pistoris_dlf_write(source.get(), &options, 0, &bytes.value, &bytes.byte_count) != ARX_OK) std::abort();

  ArxDlf* raw_roundtrip = nullptr;
  ArxLlf* raw_roundtrip_lighting = nullptr;
  const ArxReturnCode read_rc =
      arx_pistoris_dlf_read(bytes.get(), bytes.size(), &raw_roundtrip, &raw_roundtrip_lighting);
  arx_fuzz::DlfHandle roundtrip(raw_roundtrip);
  arx_fuzz::LlfHandle roundtrip_lighting(raw_roundtrip_lighting);
  if (read_rc != ARX_OK || !roundtrip.get()) std::abort();
  return 0;
}
