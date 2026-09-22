// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "api/c/cinematic/internal.h"  // IWYU pragma: keep
#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_cinematic_resource_path(const ArxCinematic* cinematic, ArxStringView* out_path) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = pistoris::c_api::view(cinematic->value.resourcePath());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_end_frame(const ArxCinematic* cinematic, int32_t* out_frame) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_frame) return ARX_INVALID_DATA_POINTER;
  *out_frame = cinematic->value.endFrame();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_fps(const ArxCinematic* cinematic, float* out_fps) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_fps) return ARX_INVALID_DATA_POINTER;
  *out_fps = cinematic->value.fps();
  return ARX_OK;
}

#define ARX_CINEMATIC_COUNT(name, method)                                                                          \
  ArxReturnCode arx_pistoris_cinematic_##name##_count(const ArxCinematic* cinematic, size_t* out_count) noexcept { \
    if (!cinematic) return ARX_INVALID_HANDLE;                                                                     \
    if (!out_count) return ARX_INVALID_DATA_POINTER;                                                               \
    *out_count = cinematic->value.method##Count();                                                                 \
    return ARX_OK;                                                                                                 \
  }

ARX_CINEMATIC_COUNT(illustration, illustration)
ARX_CINEMATIC_COUNT(keyframe, keyframe)
ARX_CINEMATIC_COUNT(texture, texture)
ARX_CINEMATIC_COUNT(language, language)
ARX_CINEMATIC_COUNT(sound_encoding, soundEncoding)

#undef ARX_CINEMATIC_COUNT

ArxReturnCode arx_pistoris_cinematic_sound_count(const ArxCinematic* cinematic, ArxSoundKind kind,
                                                 size_t* out_count) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  const auto cpp_kind = static_cast<pistoris::SoundKind>(kind);
  if (cpp_kind != pistoris::SoundKind::kEffect && cpp_kind != pistoris::SoundKind::kSpeech) return ARX_INVALID_OPTIONS;
  *out_count = cinematic->value.soundCount(cpp_kind);
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_copy_illustrations(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                        ArxCinematicIllustration* out_values) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.copyIllustrations(offset, count, out_values);
}

ArxReturnCode arx_pistoris_cinematic_copy_keyframes(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                    ArxCinematicKeyframe* out_values) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.copyKeyframes(offset, count, out_values);
}

ArxReturnCode arx_pistoris_cinematic_copy_texture_views(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                        ArxTextureView* out_values) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.copyTextureViews(offset, count, out_values);
}

ArxReturnCode arx_pistoris_cinematic_copy_sound_views(const ArxCinematic* cinematic, ArxSoundKind kind, size_t offset,
                                                      size_t count, ArxCinematicSoundView* out_values) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.copySoundViews(static_cast<pistoris::SoundKind>(kind), offset, count, out_values);
}

ArxReturnCode arx_pistoris_cinematic_copy_languages(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                    ArxCinematicLanguageView* out_values) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.copyLanguages(offset, count, out_values);
}

ArxReturnCode arx_pistoris_cinematic_copy_sound_encodings(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                          ArxCinematicSoundEncodingView* out_values) noexcept {
  if (!cinematic) return ARX_INVALID_HANDLE;
  return cinematic->value.copySoundEncodings(offset, count, out_values);
}

ArxReturnCode arx_pistoris_cinematic_sound_files_count(const ArxCinematicSoundFiles* files,
                                                       size_t* out_count) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = files->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_sound_files_get(const ArxCinematicSoundFiles* files, size_t index,
                                                     ArxCinematicSoundFile* out_file) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_file) return ARX_INVALID_DATA_POINTER;
  *out_file = {};
  if (index >= files->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::CinematicSoundFile& file = files->value[index];
  *out_file = {file.source_sound,
               file.language,
               pistoris::c_api::view(file.path),
               pistoris::c_api::audioView(file.encoded_audio)};
  return ARX_OK;
}

void arx_pistoris_cinematic_sound_files_destroy(ArxCinematicSoundFiles* files) noexcept { delete files; }

ArxReturnCode arx_pistoris_cinematic_sound_source_references_count(const ArxCinematicSoundSourceReferences* references,
                                                                   size_t* out_count) noexcept {
  if (!references) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = references->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_cinematic_sound_source_references_get(
    const ArxCinematicSoundSourceReferences* references, size_t index,
    ArxCinematicSoundSourceReference* out_reference) noexcept {
  if (!references) return ARX_INVALID_HANDLE;
  if (!out_reference) return ARX_INVALID_DATA_POINTER;
  *out_reference = {};
  if (index >= references->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::CinematicSoundSourceReference& reference = references->value[index];
  *out_reference = {reference.sound, pistoris::c_api::view(reference.path)};
  return ARX_OK;
}

void arx_pistoris_cinematic_sound_source_references_destroy(ArxCinematicSoundSourceReferences* references) noexcept {
  delete references;
}

// NOLINTEND(readability-identifier-naming)
