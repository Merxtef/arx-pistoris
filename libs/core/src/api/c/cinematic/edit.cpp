// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/error.h"
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

ArxReturnCode arx_pistoris_cinematic_set_resource_path(ArxCinematic* cinematic, ArxStringView path,
                                                       ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(cinematic->value.setResourcePath(pistoris::c_api::stringView(path)), error);
  });
}

ArxReturnCode arx_pistoris_cinematic_set_timeline(ArxCinematic* cinematic, int32_t end_frame, float fps,
                                                  ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.setTimeline(end_frame, fps), error);
}

ArxReturnCode arx_pistoris_cinematic_set_keyframe(ArxCinematic* cinematic, size_t index,
                                                  const ArxCinematicKeyframe* keyframe, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!keyframe) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(cinematic->value.setKeyframe(index, *keyframe), error);
}

ArxReturnCode arx_pistoris_cinematic_add_keyframe(ArxCinematic* cinematic, const ArxCinematicKeyframe* keyframe,
                                                  size_t* out_index, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!keyframe || !out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = static_cast<std::size_t>(-1);
  auto result = cinematic->value.addKeyframe(*keyframe);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_remove_keyframe(ArxCinematic* cinematic, size_t index, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.removeKeyframe(index), error);
}

ArxReturnCode arx_pistoris_cinematic_clear_keyframes(ArxCinematic* cinematic, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  cinematic->value.clearKeyframes();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_set_illustration(ArxCinematic* cinematic, ArxCinematicIllustrationIndex index,
                                                      ArxCinematicIllustration illustration, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.setIllustration(index, illustration), error);
}

ArxReturnCode arx_pistoris_cinematic_add_illustration(ArxCinematic* cinematic, ArxCinematicIllustration illustration,
                                                      ArxCinematicIllustrationIndex* out_index,
                                                      ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_index) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_INVALID_CINEMATIC_ILLUSTRATION;
  auto result = cinematic->value.addIllustration(illustration);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_remove_illustration(ArxCinematic* cinematic, ArxCinematicIllustrationIndex index,
                                                         ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.removeIllustration(index), error);
}

ArxReturnCode arx_pistoris_cinematic_clear_illustrations(ArxCinematic* cinematic, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  cinematic->value.clearIllustrations();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_compact_illustrations(ArxCinematic* cinematic, size_t* out_removed,
                                                           ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = cinematic->value.compactIllustrations();
  if (!result) return pistoris::c_api::publish(result, error);
  if (out_removed) *out_removed = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_rebase_texture_paths(ArxCinematic* cinematic, ArxStringView directory,
                                                          ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(directory)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(cinematic->value.rebaseTexturePaths(pistoris::c_api::stringView(directory)), error);
  });
}

ArxReturnCode arx_pistoris_cinematic_set_texture(ArxCinematic* cinematic, ArxTextureIndex index,
                                                 const ArxTextureView* texture, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!texture || !pistoris::c_api::valid(texture->path) || !pistoris::c_api::valid(texture->encoded_image) ||
      !pistoris::c_api::valid(texture->external_image_extension))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(cinematic->value.setTexture(index, *texture), error);
}

ArxReturnCode arx_pistoris_cinematic_add_texture(ArxCinematic* cinematic, const ArxTextureView* texture,
                                                 ArxTextureIndex* out_index, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!texture || !out_index || !pistoris::c_api::valid(texture->path) ||
      !pistoris::c_api::valid(texture->encoded_image) || !pistoris::c_api::valid(texture->external_image_extension))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_NO_TEXTURE;
  auto result = cinematic->value.addTexture(*texture);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_set_texture_path(ArxCinematic* cinematic, ArxTextureIndex index,
                                                      ArxStringView path, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(cinematic->value.setTexturePath(index, pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_cinematic_set_texture_external_image_extension(ArxCinematic* cinematic,
                                                                          ArxTextureIndex index,
                                                                          ArxStringView extension,
                                                                          ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(extension)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(
      cinematic->value.setTextureExternalImageExtension(index, pistoris::c_api::stringView(extension)), error);
}

ArxReturnCode arx_pistoris_cinematic_set_texture_image(ArxCinematic* cinematic, ArxTextureIndex index,
                                                       ArxEncodedImageView image, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(image)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(cinematic->value.setTextureImage(index, image), error);
}

ArxReturnCode arx_pistoris_cinematic_clear_texture_image(ArxCinematic* cinematic, ArxTextureIndex index,
                                                         ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.clearTextureImage(index), error);
}

ArxReturnCode arx_pistoris_cinematic_compact_sounds(ArxCinematic* cinematic, ArxSoundKind kind, size_t* out_removed,
                                                    ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (kind != ARX_SOUND_EFFECT && kind != ARX_SOUND_SPEECH)
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  auto result = cinematic->value.compactSounds(static_cast<pistoris::SoundKind>(kind));
  if (!result) return pistoris::c_api::publish(result, error);
  if (out_removed) *out_removed = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_rebase_sound_paths(ArxCinematic* cinematic, ArxSoundKind kind,
                                                        ArxStringView directory, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(directory)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (kind != ARX_SOUND_EFFECT && kind != ARX_SOUND_SPEECH)
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::guard(error, [&] {
    auto result = cinematic->value.rebaseSoundPaths(static_cast<pistoris::SoundKind>(kind),
                                                    pistoris::c_api::stringView(directory));
    return pistoris::c_api::publish(result, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_set_sound_path(ArxCinematic* cinematic, ArxSoundHandle sound, ArxStringView path,
                                                    ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(cinematic->value.setSoundPath(sound, pistoris::c_api::stringView(path)), error);
  });
}

ArxReturnCode arx_pistoris_cinematic_add_sound(ArxCinematic* cinematic, ArxSoundKind kind, ArxStringView path,
                                               ArxSoundHandle* out_sound, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_sound || !pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_sound = ARX_NO_SOUND_HANDLE;
  if (kind != ARX_SOUND_EFFECT && kind != ARX_SOUND_SPEECH)
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = cinematic->value.addSound(static_cast<pistoris::SoundKind>(kind), pistoris::c_api::stringView(path));
    if (!result) return pistoris::c_api::publish(result, error);
    *out_sound = *result;
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_remove_sound(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                  ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.removeSound(sound), error);
}

ArxReturnCode arx_pistoris_cinematic_set_sound_data(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                    ArxLanguageId language, ArxEncodedAudioView encoded_audio,
                                                    ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(encoded_audio)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(cinematic->value.setSoundData(sound, language, encoded_audio), error);
}

ArxReturnCode arx_pistoris_cinematic_clear_sound_data(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                      ArxLanguageId language, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.clearSoundData(sound, language), error);
}

ArxReturnCode arx_pistoris_cinematic_set_language(ArxCinematic* cinematic, ArxLanguageId language, ArxStringView name,
                                                  ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(name)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::guard(error, [&] {
    return pistoris::c_api::publish(cinematic->value.setLanguage(language, pistoris::c_api::stringView(name)), error);
  });
}

ArxReturnCode arx_pistoris_cinematic_add_language(ArxCinematic* cinematic, ArxStringView name,
                                                  ArxLanguageId* out_language, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_language || !pistoris::c_api::valid(name))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_language = ARX_INVALID_LANGUAGE_ID;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = cinematic->value.addLanguage(pistoris::c_api::stringView(name));
    if (!result) return pistoris::c_api::publish(result, error);
    *out_language = *result;
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_remove_language(ArxCinematic* cinematic, ArxLanguageId language,
                                                     ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.removeLanguage(language), error);
}

// NOLINTEND(readability-identifier-naming)
