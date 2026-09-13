// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "external/glb/container.h"
#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <span>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  pistoris::glb::Asset asset;
  const ArxReturnCode rc = pistoris::glb::parse(std::span<const std::uint8_t>(data, size), asset);
  if ((rc == ARX_OK) != (asset.data() != nullptr)) std::abort();
  return 0;
}
