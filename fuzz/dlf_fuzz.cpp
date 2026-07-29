// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxDlf* dlf = nullptr;
  ArxLlf* embedded_lighting = nullptr;
  const ArxReturnCode read_rc = arx_pistoris_dlf_parse(data, size, &dlf, &embedded_lighting);
  if (read_rc == ARX_OK) {
    arx_fuzz::DlfHandle parsed_dlf(dlf);
    arx_fuzz::LlfHandle parsed_lighting(embedded_lighting);
    if (!parsed_dlf.get()) std::abort();
  }
  return 0;
}
