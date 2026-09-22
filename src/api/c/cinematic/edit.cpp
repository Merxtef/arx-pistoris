// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "api/c/cinematic/internal.h"  // IWYU pragma: keep
#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_cinematic_set_resource_path(ArxCinematic* cinematic, ArxStringView path) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return cinematic->value.setResourcePath(pistoris::c_api::stringView(path)); });
}

ArxReturnCode arx_pistoris_cinematic_set_timeline(ArxCinematic* cinematic, int32_t end_frame, float fps) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.setTimeline(end_frame, fps);
}

ArxReturnCode arx_pistoris_cinematic_set_keyframe(ArxCinematic* cinematic, size_t index,
                                                  const ArxCinematicKeyframe* keyframe) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!keyframe) return ARX_INVALID_DATA_POINTER;
  return cinematic->value.setKeyframe(index, *keyframe);
}

ArxReturnCode arx_pistoris_cinematic_add_keyframe(ArxCinematic* cinematic, const ArxCinematicKeyframe* keyframe,
                                                  size_t* out_index) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!keyframe || !out_index) return ARX_INVALID_DATA_POINTER;
  return cinematic->value.addKeyframe(*keyframe, *out_index);
}

ArxReturnCode arx_pistoris_cinematic_remove_keyframe(ArxCinematic* cinematic, size_t index) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.removeKeyframe(index);
}

ArxReturnCode arx_pistoris_cinematic_clear_keyframes(ArxCinematic* cinematic) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  cinematic->value.clearKeyframes();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_set_illustration(ArxCinematic* cinematic, ArxCinematicIllustrationIndex index,
                                                      ArxCinematicIllustration illustration) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.setIllustration(index, illustration);
}

ArxReturnCode arx_pistoris_cinematic_add_illustration(ArxCinematic* cinematic, ArxCinematicIllustration illustration,
                                                      ArxCinematicIllustrationIndex* out_index) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_index) return ARX_INVALID_DATA_POINTER;
  return cinematic->value.addIllustration(illustration, *out_index);
}

ArxReturnCode arx_pistoris_cinematic_remove_illustration(ArxCinematic* cinematic,
                                                         ArxCinematicIllustrationIndex index) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.removeIllustration(index);
}

ArxReturnCode arx_pistoris_cinematic_clear_illustrations(ArxCinematic* cinematic) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  cinematic->value.clearIllustrations();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_compact_textures(ArxCinematic* cinematic, size_t* out_removed) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.compactTextures(out_removed);
}

ArxReturnCode arx_pistoris_cinematic_rebase_texture_paths(ArxCinematic* cinematic, ArxStringView directory) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(directory)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return cinematic->value.rebaseTexturePaths(pistoris::c_api::stringView(directory)); });
}

ArxReturnCode arx_pistoris_cinematic_set_texture(ArxCinematic* cinematic, ArxTextureIndex index,
                                                 const ArxTextureView* texture) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!texture || !pistoris::c_api::valid(texture->path) || !pistoris::c_api::valid(texture->encoded_image) ||
      !pistoris::c_api::valid(texture->external_image_extension))
    return ARX_INVALID_DATA_POINTER;
  return cinematic->value.setTexture(index, *texture);
}

ArxReturnCode arx_pistoris_cinematic_add_texture(ArxCinematic* cinematic, const ArxTextureView* texture,
                                                 ArxTextureIndex* out_index) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!texture || !out_index || !pistoris::c_api::valid(texture->path) ||
      !pistoris::c_api::valid(texture->encoded_image) || !pistoris::c_api::valid(texture->external_image_extension))
    return ARX_INVALID_DATA_POINTER;
  return cinematic->value.addTexture(*texture, *out_index);
}

ArxReturnCode arx_pistoris_cinematic_set_texture_image(ArxCinematic* cinematic, ArxTextureIndex index,
                                                       ArxEncodedImageView image) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(image)) return ARX_INVALID_DATA_POINTER;
  return cinematic->value.setTextureImage(index, image);
}

ArxReturnCode arx_pistoris_cinematic_clear_texture_image(ArxCinematic* cinematic, ArxTextureIndex index) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.clearTextureImage(index);
}

ArxReturnCode arx_pistoris_cinematic_compact_sounds(ArxCinematic* cinematic, ArxSoundKind kind,
                                                    size_t* out_removed) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.compactSounds(static_cast<pistoris::SoundKind>(kind), out_removed);
}

ArxReturnCode arx_pistoris_cinematic_rebase_sound_paths(ArxCinematic* cinematic, ArxSoundKind kind,
                                                        ArxStringView directory) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(directory)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] {
    return cinematic->value.rebaseSoundPaths(static_cast<pistoris::SoundKind>(kind),
                                             pistoris::c_api::stringView(directory));
  });
}

ArxReturnCode arx_pistoris_cinematic_set_sound_path(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                    ArxStringView path) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return cinematic->value.setSoundPath(sound, pistoris::c_api::stringView(path)); });
}

ArxReturnCode arx_pistoris_cinematic_add_sound(ArxCinematic* cinematic, ArxSoundKind kind, ArxStringView path,
                                               ArxSoundHandle* out_sound) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_sound || !pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] {
    return cinematic->value.addSound(
        static_cast<pistoris::SoundKind>(kind), pistoris::c_api::stringView(path), *out_sound);
  });
}

ArxReturnCode arx_pistoris_cinematic_remove_sound(ArxCinematic* cinematic, ArxSoundHandle sound) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.removeSound(sound);
}

ArxReturnCode arx_pistoris_cinematic_set_sound_data(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                    ArxLanguageId language,
                                                    ArxEncodedAudioView encoded_audio) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(encoded_audio)) return ARX_INVALID_DATA_POINTER;
  return cinematic->value.setSoundData(sound, language, encoded_audio);
}

ArxReturnCode arx_pistoris_cinematic_clear_sound_data(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                      ArxLanguageId language) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.clearSoundData(sound, language);
}

ArxReturnCode arx_pistoris_cinematic_set_language(ArxCinematic* cinematic, ArxLanguageId language,
                                                  ArxStringView name) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(name)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return cinematic->value.setLanguage(language, pistoris::c_api::stringView(name)); });
}

ArxReturnCode arx_pistoris_cinematic_add_language(ArxCinematic* cinematic, ArxStringView name,
                                                  ArxLanguageId* out_language) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_language || !pistoris::c_api::valid(name)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return cinematic->value.addLanguage(pistoris::c_api::stringView(name), *out_language); });
}

ArxReturnCode arx_pistoris_cinematic_remove_language(ArxCinematic* cinematic, ArxLanguageId language) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.removeLanguage(language);
}

// NOLINTEND(readability-identifier-naming)
