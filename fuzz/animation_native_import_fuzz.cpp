// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxTea* raw_native = nullptr;
  const ArxReturnCode native_rc = arx_pistoris_tea_read(data, size, &raw_native);
  arx_fuzz::TeaHandle native(raw_native);
  if (native_rc != ARX_OK) {
    if (native.get()) std::abort();
    return 0;
  }
  if (!native.get()) std::abort();

  ArxAnimation* raw_animation = nullptr;
  ArxSoundSourceReferences* raw_sources = nullptr;
  const ArxReturnCode rc = arx_pistoris_animation_import_native(native.get(), &raw_animation, &raw_sources);
  arx_fuzz::AnimationHandle animation(raw_animation);
  arx_fuzz::SoundSourceReferencesHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (animation.get() || sources.get()) std::abort();
    return 0;
  }
  if (!animation.get() || !sources.get()) std::abort();
  if (arx_pistoris_animation_validate(animation.get()) != ARX_OK) std::abort();
  std::size_t sound_count = 0;
  if (arx_pistoris_animation_sound_count(animation.get(), &sound_count) != ARX_OK) std::abort();
  arx_fuzz::validateSoundSourceReferences(sources.get(), sound_count);
  return 0;
}
