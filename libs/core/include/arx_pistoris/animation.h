// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_ANIMATION_H
#define ARX_PISTORIS_ANIMATION_H

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/text.h"

#include <stddef.h>
#include <stdint.h>

// Public C ABI naming
// NOLINTBEGIN(readability-identifier-naming)

typedef struct arx_pistoris_tea ArxTea;
typedef struct arx_pistoris_animation ArxAnimation;
typedef struct arx_pistoris_animation_list ArxAnimationList;
typedef struct arx_pistoris_sound_files ArxSoundFiles;
typedef struct arx_pistoris_sound_source_references ArxSoundSourceReferences;
typedef struct ArxSoundView ArxSoundView;

typedef struct ArxNativeAnimationBakeOptions {
  uint8_t include_sound_files;
  ArxNativeTextMode text_mode;
} ArxNativeAnimationBakeOptions;

#define ARX_NATIVE_ANIMATION_BAKE_OPTIONS_INIT {1U, ARX_NATIVE_TEXT_AUTO}

/*
 * Input string, encoded-audio, and group-transform storage required only for call duration
 * Functions taking ArxAnimation* invalidate Sound and keyframe indices and borrowed views
 */

ARX_EXTERN_C_BEGIN

// --- Lifetime ---

ARX_API ArxReturnCode arx_pistoris_animation_create(ArxAnimation** out_animation, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_clone(const ArxAnimation* animation, ArxAnimation** out_animation,
                                                   ArxError* error) ARX_NOEXCEPT;
ARX_API void arx_pistoris_animation_destroy(ArxAnimation* animation) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_reset(ArxAnimation* animation, ArxError* error) ARX_NOEXCEPT;
/* Borrowed ArxAnimation handles; valid until list destruction */
ARX_API ArxReturnCode arx_pistoris_animation_list_count(const ArxAnimationList* list, size_t* out_count,
                                                        ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_list_get(ArxAnimationList* list, size_t index,
                                                      ArxAnimation** out_animation, ArxError* error) ARX_NOEXCEPT;
ARX_API void arx_pistoris_animation_list_destroy(ArxAnimationList* list) ARX_NOEXCEPT;

// --- Conversion ---

ARX_API ArxReturnCode arx_pistoris_animation_import_native(const ArxTea* native, ArxAnimation** out_animation,
                                                           ArxSoundSourceReferences** out_sound_sources,
                                                           ArxNativeTextMode text_mode, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_bake_native(const ArxAnimation* animation,
                                                         const ArxNativeAnimationBakeOptions* options,
                                                         ArxTea** out_native, ArxSoundFiles** out_sounds,
                                                         ArxError* error) ARX_NOEXCEPT;

// --- Validation ---

ARX_API ArxReturnCode arx_pistoris_animation_validate(const ArxAnimation* animation, ArxError* error) ARX_NOEXCEPT;

// --- Transformation ---

ARX_API ArxReturnCode arx_pistoris_animation_scale(ArxAnimation* animation, float factor, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_rotate(ArxAnimation* animation, ArxQuat rotation,
                                                    ArxError* error) ARX_NOEXCEPT;

// --- Resource data ---

ARX_API ArxReturnCode arx_pistoris_animation_name(const ArxAnimation* animation, ArxStringView* out_name,
                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_set_name(ArxAnimation* animation, ArxStringView name,
                                                      ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_resource_path(const ArxAnimation* animation, ArxStringView* out_path,
                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_set_resource_path(ArxAnimation* animation, ArxStringView path,
                                                               ArxError* error) ARX_NOEXCEPT;

// --- Inspection ---

ARX_API ArxReturnCode arx_pistoris_animation_frame_length(const ArxAnimation* animation, uint32_t* out_length,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_group_count(const ArxAnimation* animation, size_t* out_count,
                                                         ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_is_group_void(const ArxAnimation* animation, size_t group,
                                                           uint8_t* out_void, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_is_group_claimed(const ArxAnimation* animation, size_t group,
                                                              uint8_t* out_claimed, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_keyframe_count(const ArxAnimation* animation, size_t* out_count,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_sound_count(const ArxAnimation* animation, size_t* out_count,
                                                         ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_copy_sound_views(const ArxAnimation* animation, size_t offset,
                                                              size_t count, ArxSoundView* out_sounds,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_copy_keyframes(const ArxAnimation* animation, size_t offset, size_t count,
                                                            ArxAnimationKeyframe* out_keyframes,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_copy_group_transforms(const ArxAnimation* animation, size_t keyframe,
                                                                   size_t offset, size_t count,
                                                                   ArxAnimationGroupTransform* out_transforms,
                                                                   ArxError* error) ARX_NOEXCEPT;

// --- Timeline editing ---

ARX_API ArxReturnCode arx_pistoris_animation_set_frame_length(ArxAnimation* animation, uint32_t frame_length,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_set_keyframe(ArxAnimation* animation, size_t index,
                                                          const ArxAnimationKeyframeInput* keyframe,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_add_keyframe(ArxAnimation* animation,
                                                          const ArxAnimationKeyframeInput* keyframe, size_t* out_index,
                                                          ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_remove_keyframe(ArxAnimation* animation, size_t index,
                                                             ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_replace_keyframes(ArxAnimation* animation, uint32_t frame_length,
                                                               const ArxAnimationKeyframeInput* keyframes,
                                                               size_t keyframe_count, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_clear_keyframes(ArxAnimation* animation, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_claim_group(ArxAnimation* animation, size_t group,
                                                         ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_unclaim_group(ArxAnimation* animation, size_t group,
                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_void_group(ArxAnimation* animation, size_t group,
                                                        ArxError* error) ARX_NOEXCEPT;

// --- Sounds ---

ARX_API ArxReturnCode arx_pistoris_animation_compact_sounds(ArxAnimation* animation, size_t* out_removed,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_rebase_sound_paths(ArxAnimation* animation, ArxStringView directory,
                                                                ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_set_sound(ArxAnimation* animation, ArxSoundIndex index,
                                                       const ArxSoundView* sound, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_add_sound(ArxAnimation* animation, const ArxSoundView* sound,
                                                       ArxSoundIndex* out_index, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_set_sound_path(ArxAnimation* animation, ArxSoundIndex index,
                                                            ArxStringView path, ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_set_sound_data(ArxAnimation* animation, ArxSoundIndex index,
                                                            ArxEncodedAudioView encoded_audio,
                                                            ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_clear_sound_data(ArxAnimation* animation, ArxSoundIndex index,
                                                              ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_animation_remove_sound(ArxAnimation* animation, ArxSoundIndex index,
                                                          ArxError* error) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming)

#endif /* ARX_PISTORIS_ANIMATION_H */
