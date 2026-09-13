// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  std::vector<std::uint8_t> glb = arx_fuzz::buildGlbFromFuzzInput(data, size);
  if (glb.empty()) return 0;

  ArxLevel* raw_level = nullptr;
  ArxTextureSourcePaths* raw_sources = nullptr;
  const ArxReturnCode rc =
      arx_pistoris_level_import_glb(glb.data(), glb.size(), nullptr, &raw_level, nullptr, &raw_sources);
  arx_fuzz::LevelHandle level(raw_level);
  arx_fuzz::TextureSourcePathsHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (level.get() || sources.get()) std::abort();
    return 0;
  }
  if (!level.get() || !sources.get()) std::abort();
  if (arx_pistoris_level_validate(level.get()) != ARX_OK) std::abort();
  std::size_t texture_count = 0;
  if (arx_pistoris_level_texture_count(level.get(), &texture_count) != ARX_OK) std::abort();
  arx_fuzz::validateTextureSourcePaths(sources.get(), texture_count);
  return 0;
}
