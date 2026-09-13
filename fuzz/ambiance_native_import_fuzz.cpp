// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxAmb* raw_native = nullptr;
  const ArxReturnCode native_rc = arx_pistoris_amb_read(data, size, &raw_native);
  arx_fuzz::AmbHandle native(raw_native);
  if (native_rc != ARX_OK) {
    if (native.get()) std::abort();
    return 0;
  }
  if (!native.get()) std::abort();

  ArxAmbiance* raw_ambiance = nullptr;
  ArxSoundSourceReferences* raw_sources = nullptr;
  const ArxReturnCode rc = arx_pistoris_ambiance_import_native(native.get(), &raw_ambiance, &raw_sources);
  arx_fuzz::AmbianceHandle ambiance(raw_ambiance);
  arx_fuzz::SoundSourceReferencesHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (ambiance.get() || sources.get()) std::abort();
    return 0;
  }
  if (!ambiance.get() || !sources.get()) std::abort();
  if (arx_pistoris_ambiance_validate(ambiance.get()) != ARX_OK) std::abort();
  std::size_t sound_count = 0;
  if (arx_pistoris_ambiance_sound_count(ambiance.get(), &sound_count) != ARX_OK) std::abort();
  arx_fuzz::validateSoundSourceReferences(sources.get(), sound_count);
  return 0;
}
