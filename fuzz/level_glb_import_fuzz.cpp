// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  std::vector<std::uint8_t> glb = arx_fuzz::buildGlbFromFuzzInput(data, size);
  if (glb.empty()) return 0;

  ArxLevel* raw_level = nullptr;
  const ArxReturnCode rc = arx_pistoris_level_from_glb(glb.data(), glb.size(), nullptr, nullptr, &raw_level);
  if (rc == ARX_OK) {
    arx_fuzz::LevelHandle level(raw_level);
    if (!level.get()) std::abort();
    if (arx_pistoris_level_validate(level.get()) != ARX_OK) std::abort();
  }
  return 0;
}
