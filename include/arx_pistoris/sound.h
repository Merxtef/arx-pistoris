// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_SOUND_H
#define ARX_PISTORIS_SOUND_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef struct arx_pistoris_sound_files ArxSoundFiles;
typedef struct arx_pistoris_sound_source_references ArxSoundSourceReferences;
typedef struct arx_pistoris_animation_sound_files ArxAnimationSoundFiles;
typedef struct arx_pistoris_animation_sound_source_references ArxAnimationSoundSourceReferences;

typedef struct ArxSoundView {
  ArxStringView path;
  ArxEncodedAudioView encoded_audio;
} ArxSoundView;

typedef struct ArxNativeSoundBakeOptions {
  // Include encoded sidecars in output bundle
  uint8_t include_files;
} ArxNativeSoundBakeOptions;

#define ARX_NATIVE_SOUND_BAKE_OPTIONS_INIT {1U}

typedef struct ArxSoundFile {
  ArxSoundIndex source_sound;
  ArxStringView path;
  ArxEncodedAudioView encoded_audio;
} ArxSoundFile;

typedef struct ArxSoundSourceReference {
  ArxSoundIndex sound;
  ArxStringView path;
} ArxSoundSourceReference;

typedef struct ArxAnimationSoundFile {
  size_t animation_index;
  ArxSoundFile file;
} ArxAnimationSoundFile;

typedef struct ArxAnimationSoundSourceReference {
  size_t animation_index;
  ArxSoundSourceReference reference;
} ArxAnimationSoundSourceReference;

/* Returned views remain valid until their owning sound-files or source-references handle is destroyed */

ARX_EXTERN_C_BEGIN

ARX_API ArxReturnCode arx_pistoris_sound_files_count(const ArxSoundFiles* files, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_sound_files_get(const ArxSoundFiles* files, size_t index,
                                                   ArxSoundFile* out_file) ARX_NOEXCEPT;
ARX_API void arx_pistoris_sound_files_destroy(ArxSoundFiles* files) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_sound_source_references_count(const ArxSoundSourceReferences* references,
                                                                 size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_sound_source_references_get(const ArxSoundSourceReferences* references, size_t index,
                                                               ArxSoundSourceReference* out_reference) ARX_NOEXCEPT;
ARX_API void arx_pistoris_sound_source_references_destroy(ArxSoundSourceReferences* references) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_sound_files_count(const ArxAnimationSoundFiles* files,
                                                               size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_sound_files_get(const ArxAnimationSoundFiles* files, size_t index,
                                                             ArxAnimationSoundFile* out_file) ARX_NOEXCEPT;
ARX_API void arx_pistoris_animation_sound_files_destroy(ArxAnimationSoundFiles* files) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_sound_source_references_count(
    const ArxAnimationSoundSourceReferences* references, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode
arx_pistoris_animation_sound_source_references_get(const ArxAnimationSoundSourceReferences* references, size_t index,
                                                   ArxAnimationSoundSourceReference* out_reference) ARX_NOEXCEPT;
ARX_API void arx_pistoris_animation_sound_source_references_destroy(ArxAnimationSoundSourceReferences* references)
    ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_SOUND_H */
