// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_CINEMATIC_H
#define ARX_PISTORIS_CINEMATIC_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native/text.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/texture.h"

#include <stddef.h>
#include <stdint.h>

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming)

typedef struct arx_pistoris_cin ArxCin;
typedef struct arx_pistoris_cinematic ArxCinematic;
typedef struct arx_pistoris_cinematic_sound_files ArxCinematicSoundFiles;
typedef struct arx_pistoris_cinematic_sound_source_references ArxCinematicSoundSourceReferences;

typedef struct ArxNativeCinematicBakeOptions {
  uint8_t include_illustration_files;
  uint8_t include_sound_files;
  /* UNKNOWN retains BMP and game-compatible TGA, using TGA otherwise; BMP or TGA forces that format. */
  ArxImageFormat illustration_format;
  ArxNativeTextMode text_mode;
} ArxNativeCinematicBakeOptions;

#define ARX_NATIVE_CINEMATIC_BAKE_OPTIONS_INIT {1U, 1U, ARX_IMAGE_FORMAT_UNKNOWN, ARX_NATIVE_TEXT_AUTO}

typedef struct ArxCinematicSoundSourceReference {
  ArxSoundHandle sound;
  ArxStringView path;
} ArxCinematicSoundSourceReference;

typedef struct ArxCinematicSoundFile {
  ArxSoundHandle source_sound;
  ArxLanguageId language;
  ArxStringView path;
  ArxEncodedAudioView encoded_audio;
} ArxCinematicSoundFile;

/* Input string and encoded-data storage is required only for call duration */
/* Non-const calls invalidate collection indices, SoundHandles, and borrowed views */

ARX_EXTERN_C_BEGIN

// --- Lifetime ---

ARX_API ArxReturnCode arx_pistoris_cinematic_create(ArxCinematic** out_cinematic) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_clone(const ArxCinematic* cinematic,
                                                   ArxCinematic** out_cinematic) ARX_NOEXCEPT;
ARX_API void arx_pistoris_cinematic_destroy(ArxCinematic* cinematic) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_reset(ArxCinematic* cinematic) ARX_NOEXCEPT;

// --- Conversion ---

ARX_API ArxReturnCode arx_pistoris_cinematic_import_native(const ArxCin* native, ArxCinematic** out_cinematic,
                                                           ArxTextureSourcePaths** out_illustration_sources,
                                                           ArxCinematicSoundSourceReferences** out_sound_sources,
                                                           ArxNativeTextMode text_mode) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_import_glb(const uint8_t* data, size_t size, ArxCinematic** out_cinematic,
                                                        ArxCinematicSoundSourceReferences** out_sound_sources)
    ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_bake_native(const ArxCinematic* cinematic,
                                                         const ArxNativeCinematicBakeOptions* options,
                                                         ArxCin** out_native, ArxNativeTextureFiles** out_illustrations,
                                                         ArxCinematicSoundFiles** out_sounds) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_export_glb(const ArxCinematic* cinematic, uint8_t** out_data,
                                                        size_t* out_size,
                                                        ArxCinematicSoundFiles** out_sounds) ARX_NOEXCEPT;

// --- Validation ---

ARX_API ArxReturnCode arx_pistoris_cinematic_validate(const ArxCinematic* cinematic) ARX_NOEXCEPT;

// --- Resource data ---

ARX_API ArxReturnCode arx_pistoris_cinematic_resource_path(const ArxCinematic* cinematic,
                                                           ArxStringView* out_path) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_set_resource_path(ArxCinematic* cinematic,
                                                               ArxStringView path) ARX_NOEXCEPT;

// --- Inspection ---

ARX_API ArxReturnCode arx_pistoris_cinematic_end_frame(const ArxCinematic* cinematic, int32_t* out_frame) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_fps(const ArxCinematic* cinematic, float* out_fps) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_illustration_count(const ArxCinematic* cinematic,
                                                                size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_keyframe_count(const ArxCinematic* cinematic,
                                                            size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_texture_count(const ArxCinematic* cinematic,
                                                           size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_sound_count(const ArxCinematic* cinematic, ArxSoundKind kind,
                                                         size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_language_count(const ArxCinematic* cinematic,
                                                            size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_sound_encoding_count(const ArxCinematic* cinematic,
                                                                  size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_copy_illustrations(const ArxCinematic* cinematic, size_t offset,
                                                                size_t count,
                                                                ArxCinematicIllustration* out_values) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_copy_keyframes(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                            ArxCinematicKeyframe* out_values) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_copy_texture_views(const ArxCinematic* cinematic, size_t offset,
                                                                size_t count, ArxTextureView* out_values) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_copy_sound_views(const ArxCinematic* cinematic, ArxSoundKind kind,
                                                              size_t offset, size_t count,
                                                              ArxCinematicSoundView* out_values) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_copy_languages(const ArxCinematic* cinematic, size_t offset, size_t count,
                                                            ArxCinematicLanguageView* out_values) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_copy_sound_encodings(
    const ArxCinematic* cinematic, size_t offset, size_t count, ArxCinematicSoundEncodingView* out_values) ARX_NOEXCEPT;

// --- Timeline ---

ARX_API ArxReturnCode arx_pistoris_cinematic_set_timeline(ArxCinematic* cinematic, int32_t end_frame,
                                                          float fps) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_set_keyframe(ArxCinematic* cinematic, size_t index,
                                                          const ArxCinematicKeyframe* keyframe) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_add_keyframe(ArxCinematic* cinematic, const ArxCinematicKeyframe* keyframe,
                                                          size_t* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_remove_keyframe(ArxCinematic* cinematic, size_t index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_clear_keyframes(ArxCinematic* cinematic) ARX_NOEXCEPT;

// --- Illustrations ---

ARX_API ArxReturnCode arx_pistoris_cinematic_set_illustration(ArxCinematic* cinematic,
                                                              ArxCinematicIllustrationIndex index,
                                                              ArxCinematicIllustration illustration) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_add_illustration(ArxCinematic* cinematic,
                                                              ArxCinematicIllustration illustration,
                                                              ArxCinematicIllustrationIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_remove_illustration(ArxCinematic* cinematic,
                                                                 ArxCinematicIllustrationIndex index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_clear_illustrations(ArxCinematic* cinematic) ARX_NOEXCEPT;

// --- Textures ---

ARX_API ArxReturnCode arx_pistoris_cinematic_compact_textures(ArxCinematic* cinematic,
                                                              size_t* out_removed) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_rebase_texture_paths(ArxCinematic* cinematic,
                                                                  ArxStringView directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_set_texture(ArxCinematic* cinematic, ArxTextureIndex index,
                                                         const ArxTextureView* texture) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_add_texture(ArxCinematic* cinematic, const ArxTextureView* texture,
                                                         ArxTextureIndex* out_index) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_set_texture_image(ArxCinematic* cinematic, ArxTextureIndex index,
                                                               ArxEncodedImageView image) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_clear_texture_image(ArxCinematic* cinematic,
                                                                 ArxTextureIndex index) ARX_NOEXCEPT;

// --- Sounds ---

ARX_API ArxReturnCode arx_pistoris_cinematic_compact_sounds(ArxCinematic* cinematic, ArxSoundKind kind,
                                                            size_t* out_removed) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_rebase_sound_paths(ArxCinematic* cinematic, ArxSoundKind kind,
                                                                ArxStringView directory) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_set_sound_path(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                            ArxStringView path) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_add_sound(ArxCinematic* cinematic, ArxSoundKind kind, ArxStringView path,
                                                       ArxSoundHandle* out_sound) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_remove_sound(ArxCinematic* cinematic, ArxSoundHandle sound) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_set_sound_data(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                            ArxLanguageId language,
                                                            ArxEncodedAudioView encoded_audio) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_clear_sound_data(ArxCinematic* cinematic, ArxSoundHandle sound,
                                                              ArxLanguageId language) ARX_NOEXCEPT;

// --- Languages ---

ARX_API ArxReturnCode arx_pistoris_cinematic_set_language(ArxCinematic* cinematic, ArxLanguageId language,
                                                          ArxStringView name) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_add_language(ArxCinematic* cinematic, ArxStringView name,
                                                          ArxLanguageId* out_language) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_remove_language(ArxCinematic* cinematic,
                                                             ArxLanguageId language) ARX_NOEXCEPT;

// --- Conversion collections ---

ARX_API ArxReturnCode arx_pistoris_cinematic_sound_files_count(const ArxCinematicSoundFiles* files,
                                                               size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_sound_files_get(const ArxCinematicSoundFiles* files, size_t index,
                                                             ArxCinematicSoundFile* out_file) ARX_NOEXCEPT;
ARX_API void arx_pistoris_cinematic_sound_files_destroy(ArxCinematicSoundFiles* files) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_cinematic_sound_source_references_count(
    const ArxCinematicSoundSourceReferences* references, size_t* out_count) ARX_NOEXCEPT;
ARX_API ArxReturnCode
arx_pistoris_cinematic_sound_source_references_get(const ArxCinematicSoundSourceReferences* references, size_t index,
                                                   ArxCinematicSoundSourceReference* out_reference) ARX_NOEXCEPT;
ARX_API void arx_pistoris_cinematic_sound_source_references_destroy(ArxCinematicSoundSourceReferences* references)
    ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_CINEMATIC_H */
