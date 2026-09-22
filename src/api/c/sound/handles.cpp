// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "api/c/internal.h"
#include "api/c/sound/internal.h"  // IWYU pragma: keep

#include <cstddef>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_sound_handle(ArxSoundKind kind, ArxSoundIndex index, ArxSoundHandle* out_handle) noexcept {
  if (!out_handle) return ARX_INVALID_DATA_POINTER;
  return pistoris::soundHandle(static_cast<pistoris::SoundKind>(kind), index, *out_handle);
}

ArxReturnCode arx_pistoris_sound_handle_kind(ArxSoundHandle handle, ArxSoundKind* out_kind) noexcept {
  if (!out_kind) return ARX_INVALID_DATA_POINTER;
  pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
  const ArxReturnCode rc = pistoris::soundHandleKind(handle, kind);
  if (rc == ARX_OK) *out_kind = static_cast<ArxSoundKind>(kind);
  return rc;
}

ArxReturnCode arx_pistoris_sound_handle_index(ArxSoundHandle handle, ArxSoundIndex* out_index) noexcept {
  if (!out_index) return ARX_INVALID_DATA_POINTER;
  return pistoris::soundHandleIndex(handle, *out_index);
}

ArxReturnCode arx_pistoris_sound_files_count(const ArxSoundFiles* files, size_t* out_count) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = files->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_sound_files_get(const ArxSoundFiles* files, size_t index, ArxSoundFile* out_file) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_file) return ARX_INVALID_DATA_POINTER;
  *out_file = {};
  if (index >= files->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::SoundFile& file = files->value[index];
  *out_file = {file.source_sound, pistoris::c_api::view(file.path), pistoris::c_api::audioView(file.encoded_audio)};
  return ARX_OK;
}

void arx_pistoris_sound_files_destroy(ArxSoundFiles* files) noexcept { delete files; }

ArxReturnCode arx_pistoris_sound_source_references_count(const ArxSoundSourceReferences* references,
                                                         size_t* out_count) noexcept {
  if (!references) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = references->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_sound_source_references_get(const ArxSoundSourceReferences* references, size_t index,
                                                       ArxSoundSourceReference* out_reference) noexcept {
  if (!references) return ARX_INVALID_HANDLE;
  if (!out_reference) return ARX_INVALID_DATA_POINTER;
  *out_reference = {};
  if (index >= references->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::SoundSourceReference& reference = references->value[index];
  *out_reference = {reference.sound, pistoris::c_api::view(reference.path)};
  return ARX_OK;
}

void arx_pistoris_sound_source_references_destroy(ArxSoundSourceReferences* references) noexcept { delete references; }

ArxReturnCode arx_pistoris_animation_sound_files_count(const ArxAnimationSoundFiles* files,
                                                       size_t* out_count) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = files->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_sound_files_get(const ArxAnimationSoundFiles* files, size_t index,
                                                     ArxAnimationSoundFile* out_file) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_file) return ARX_INVALID_DATA_POINTER;
  *out_file = {};
  if (index >= files->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::AnimationSoundFile& source = files->value[index];
  out_file->animation_index = source.animation_index;
  out_file->file = {source.file.source_sound,
                    pistoris::c_api::view(source.file.path),
                    pistoris::c_api::audioView(source.file.encoded_audio)};
  return ARX_OK;
}

void arx_pistoris_animation_sound_files_destroy(ArxAnimationSoundFiles* files) noexcept { delete files; }

ArxReturnCode arx_pistoris_animation_sound_source_references_count(const ArxAnimationSoundSourceReferences* references,
                                                                   size_t* out_count) noexcept {
  if (!references) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = references->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_sound_source_references_get(
    const ArxAnimationSoundSourceReferences* references, size_t index,
    ArxAnimationSoundSourceReference* out_reference) noexcept {
  if (!references) return ARX_INVALID_HANDLE;
  if (!out_reference) return ARX_INVALID_DATA_POINTER;
  *out_reference = {};
  if (index >= references->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::AnimationSoundSourceReference& source = references->value[index];
  out_reference->animation_index = source.animation_index;
  out_reference->reference = {source.reference.sound, pistoris::c_api::view(source.reference.path)};
  return ARX_OK;
}

void arx_pistoris_animation_sound_source_references_destroy(ArxAnimationSoundSourceReferences* references) noexcept {
  delete references;
}

// NOLINTEND(readability-identifier-naming)
