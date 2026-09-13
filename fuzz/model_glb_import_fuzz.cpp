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

  ArxModel* raw_model = nullptr;
  ArxAnimationList* raw_animations = nullptr;
  ArxTextureSourcePaths* raw_texture_sources = nullptr;
  ArxAnimationSoundSourceReferences* raw_sound_sources = nullptr;
  const ArxModelGlbImportOptions options = ARX_MODEL_GLB_IMPORT_OPTIONS_INIT;
  const ArxReturnCode rc = arx_pistoris_model_import_glb(
      glb.data(), glb.size(), &options, &raw_model, &raw_animations, nullptr, &raw_texture_sources, &raw_sound_sources);
  arx_fuzz::ModelHandle model(raw_model);
  arx_fuzz::AnimationListHandle animations(raw_animations);
  arx_fuzz::TextureSourcePathsHandle texture_sources(raw_texture_sources);
  arx_fuzz::AnimationSoundSourceReferencesHandle sound_sources(raw_sound_sources);
  if (rc != ARX_OK) {
    if (model.get() || animations.get() || texture_sources.get() || sound_sources.get()) std::abort();
    return 0;
  }
  if (!model.get() || !animations.get() || !texture_sources.get() || !sound_sources.get()) std::abort();
  if (arx_pistoris_model_validate(model.get()) != ARX_OK) std::abort();
  std::size_t texture_count = 0;
  if (arx_pistoris_model_texture_count(model.get(), &texture_count) != ARX_OK) std::abort();
  arx_fuzz::validateTextureSourcePaths(texture_sources.get(), texture_count);
  std::size_t animation_count = 0;
  if (arx_pistoris_animation_list_count(animations.get(), &animation_count) != ARX_OK) std::abort();
  for (std::size_t index = 0; index < animation_count; ++index) {
    ArxAnimation* animation = nullptr;
    if (arx_pistoris_animation_list_get(animations.get(), index, &animation) != ARX_OK || !animation) std::abort();
    if (arx_pistoris_animation_validate(animation) != ARX_OK) std::abort();
  }
  arx_fuzz::validateAnimationSoundSourceReferences(sound_sources.get(), animations.get());
  return 0;
}
