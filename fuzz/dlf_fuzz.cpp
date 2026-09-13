// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxDlf* dlf = nullptr;
  ArxLlf* embedded_lighting = nullptr;
  const ArxReturnCode read_rc = arx_pistoris_dlf_read(data, size, &dlf, &embedded_lighting);
  arx_fuzz::DlfHandle parsed_dlf(dlf);
  arx_fuzz::LlfHandle parsed_lighting(embedded_lighting);
  if (read_rc != ARX_OK) {
    if (parsed_dlf.get() || parsed_lighting.get()) std::abort();
    return 0;
  }
  if (!parsed_dlf.get()) std::abort();
  return 0;
}
