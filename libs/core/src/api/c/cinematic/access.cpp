// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
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

ArxReturnCode arx_pistoris_cinematic_resource_path(const ArxCinematic* cinematic, ArxStringView* out_path,
                                                   ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_path) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_path = pistoris::c_api::view(cinematic->value.resourcePath());
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_end_frame(const ArxCinematic* cinematic, int32_t* out_frame,
                                               ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_frame) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_frame = cinematic->value.endFrame();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_fps(const ArxCinematic* cinematic, float* out_fps, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_fps) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_fps = cinematic->value.fps();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

#define ARX_CINEMATIC_COUNT(name, method)                                                 \
  ArxReturnCode arx_pistoris_cinematic_##name##_count(                                    \
      const ArxCinematic* cinematic, size_t* out_count, ArxError* error) noexcept {       \
    if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);       \
    if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error); \
    *out_count = cinematic->value.method##Count();                                        \
    return pistoris::c_api::publishCode(ARX_OK, error);                                   \
  }

ARX_CINEMATIC_COUNT(illustration, illustration)
ARX_CINEMATIC_COUNT(keyframe, keyframe)
ARX_CINEMATIC_COUNT(texture, texture)
ARX_CINEMATIC_COUNT(language, language)
ARX_CINEMATIC_COUNT(sound_encoding, soundEncoding)

#undef ARX_CINEMATIC_COUNT

ArxReturnCode arx_pistoris_cinematic_sound_count(const ArxCinematic* cinematic, ArxSoundKind kind, size_t* out_count,
                                                 ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const auto cpp_kind = static_cast<pistoris::SoundKind>(kind);
  if (cpp_kind != pistoris::SoundKind::kEffect && cpp_kind != pistoris::SoundKind::kSpeech)
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  *out_count = cinematic->value.soundCount(cpp_kind);
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_copy_illustrations(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                        ArxCinematicIllustration* out_values,
                                                        ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Cinematic::IllustrationsView values = cinematic->value.illustrations();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_copy_keyframes(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                    ArxCinematicKeyframe* out_values, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Cinematic::KeyframesView values = cinematic->value.keyframes();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_copy_texture_views(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                        ArxTextureView* out_values, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Cinematic::TexturesView values = cinematic->value.textures();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_copy_sound_views(const ArxCinematic* cinematic, ArxSoundKind kind, size_t offset,
                                                      size_t count, ArxCinematicSoundView* out_values,
                                                      ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const auto cpp_kind = static_cast<pistoris::SoundKind>(kind);
  if (cpp_kind != pistoris::SoundKind::kEffect && cpp_kind != pistoris::SoundKind::kSpeech)
    return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  const pistoris::Cinematic::SoundsView values = cinematic->value.sounds(cpp_kind);
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_copy_languages(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                    ArxCinematicLanguageView* out_values, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Cinematic::LanguagesView values = cinematic->value.languages();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_copy_sound_encodings(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                          ArxCinematicSoundEncodingView* out_values,
                                                          ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  const pistoris::Cinematic::SoundEncodingsView values = cinematic->value.soundEncodings();
  if (offset > values.size() || count > values.size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_values) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_values[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_sound_files_count(const ArxCinematicSoundFiles* files, size_t* out_count,
                                                       ArxError* error) noexcept {
  if (!files) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = files->value.size();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_sound_files_get(const ArxCinematicSoundFiles* files, size_t index,
                                                     ArxCinematicSoundFile* out_file, ArxError* error) noexcept {
  if (!files) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_file) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_file = {};
  if (index >= files->value.size()) return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  const pistoris::CinematicSoundFile& file = files->value[index];
  *out_file = {file.source_sound,
               file.language,
               pistoris::c_api::view(file.path),
               pistoris::c_api::audioView(file.encoded_audio)};
  return pistoris::c_api::publishCode(ARX_OK, error);
}

void arx_pistoris_cinematic_sound_files_destroy(ArxCinematicSoundFiles* files) noexcept { delete files; }

ArxReturnCode arx_pistoris_cinematic_sound_source_references_count(const ArxCinematicSoundSourceReferences* references,
                                                                   size_t* out_count, ArxError* error) noexcept {
  if (!references) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = references->value.size();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_cinematic_sound_source_references_get(const ArxCinematicSoundSourceReferences* references,
                                                                 size_t index,
                                                                 ArxCinematicSoundSourceReference* out_reference,
                                                                 ArxError* error) noexcept {
  if (!references) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_reference) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_reference = {};
  if (index >= references->value.size()) return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  const pistoris::CinematicSoundSourceReference& reference = references->value[index];
  *out_reference = {reference.sound, pistoris::c_api::view(reference.path)};
  return pistoris::c_api::publishCode(ARX_OK, error);
}

void arx_pistoris_cinematic_sound_source_references_destroy(ArxCinematicSoundSourceReferences* references) noexcept {
  delete references;
}

// NOLINTEND(readability-identifier-naming)
