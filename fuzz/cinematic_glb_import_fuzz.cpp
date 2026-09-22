// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <vector>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  std::vector<std::uint8_t> glb = arx_fuzz::buildGlbFromFuzzInput(data, size);
  if (glb.empty()) return 0;

  ArxCinematic* raw_cinematic = nullptr;
  ArxCinematicSoundSourceReferences* raw_sources = nullptr;
  const ArxReturnCode rc = arx_pistoris_cinematic_import_glb(glb.data(), glb.size(), &raw_cinematic, &raw_sources);
  arx_fuzz::CinematicHandle cinematic(raw_cinematic);
  arx_fuzz::CinematicSoundSourceReferencesHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (cinematic.get() || sources.get()) std::abort();
    return 0;
  }
  if (!cinematic.get() || !sources.get()) std::abort();
  if (arx_pistoris_cinematic_validate(cinematic.get()) != ARX_OK) std::abort();
  arx_fuzz::validateCinematicSoundSourceReferences(sources.get(), cinematic.get());
  return 0;
}
