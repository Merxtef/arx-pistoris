// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxCin* raw_native = nullptr;
  const ArxReturnCode native_rc = arx_pistoris_cin_read(data, size, &raw_native);
  arx_fuzz::CinHandle native(raw_native);
  if (native_rc != ARX_OK) {
    if (native.get()) std::abort();
    return 0;
  }
  if (!native.get()) std::abort();

  ArxCinematic* raw_cinematic = nullptr;
  ArxTextureSourcePaths* raw_illustration_sources = nullptr;
  ArxCinematicSoundSourceReferences* raw_sound_sources = nullptr;
  const ArxReturnCode rc = arx_pistoris_cinematic_import_native(
      native.get(), &raw_cinematic, &raw_illustration_sources, &raw_sound_sources, ARX_NATIVE_TEXT_AUTO);
  arx_fuzz::CinematicHandle cinematic(raw_cinematic);
  arx_fuzz::TextureSourcePathsHandle illustration_sources(raw_illustration_sources);
  arx_fuzz::CinematicSoundSourceReferencesHandle sound_sources(raw_sound_sources);
  if (rc != ARX_OK) {
    if (cinematic.get() || illustration_sources.get() || sound_sources.get()) std::abort();
    return 0;
  }
  if (!cinematic.get() || !illustration_sources.get() || !sound_sources.get()) std::abort();
  if (arx_pistoris_cinematic_validate(cinematic.get()) != ARX_OK) std::abort();
  std::size_t texture_count = 0;
  if (arx_pistoris_cinematic_texture_count(cinematic.get(), &texture_count) != ARX_OK) std::abort();
  arx_fuzz::validateTextureSourcePaths(illustration_sources.get(), texture_count);
  arx_fuzz::validateCinematicSoundSourceReferences(sound_sources.get(), cinematic.get());
  return 0;
}
