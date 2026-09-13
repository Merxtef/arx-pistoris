// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  const ArxEncodedImageView encoded{data, size};
  const ArxReturnCode validate_rc = arx_pistoris_binary_validate_encoded_image(encoded);
  ArxImageInfo info{};
  const ArxReturnCode inspect_rc = arx_pistoris_binary_inspect_encoded_image(encoded, &info);
  if (validate_rc != inspect_rc) std::abort();
  return 0;
}
