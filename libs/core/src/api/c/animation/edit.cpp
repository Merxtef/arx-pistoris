// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/animation/internal.h"
#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_scale(ArxAnimation* animation, float factor, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.scale(factor), error);
}

ArxReturnCode arx_pistoris_animation_rotate(ArxAnimation* animation, ArxQuat rotation, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.rotate(rotation), error);
}

ArxReturnCode arx_pistoris_animation_set_name(ArxAnimation* animation, ArxStringView name, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(name)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.setName(pistoris::c_api::stringView(name)), error);
}

ArxReturnCode arx_pistoris_animation_set_resource_path(ArxAnimation* animation, ArxStringView path,
                                                       ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.setResourcePath(pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_animation_set_frame_length(ArxAnimation* animation, uint32_t frame_length,
                                                      ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.setFrameLength(frame_length), error);
}

ArxReturnCode arx_pistoris_animation_set_keyframe(ArxAnimation* animation, size_t index,
                                                  const ArxAnimationKeyframeInput* keyframe, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!keyframe || !pistoris::c_api::valid(*keyframe))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.setKeyframe(index, *keyframe), error);
}

ArxReturnCode arx_pistoris_animation_add_keyframe(ArxAnimation* animation, const ArxAnimationKeyframeInput* keyframe,
                                                  size_t* out_index, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!keyframe || !out_index || !pistoris::c_api::valid(*keyframe))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = SIZE_MAX;
  auto result = animation->value.addKeyframe(*keyframe);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_remove_keyframe(ArxAnimation* animation, size_t index, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.removeKeyframe(index), error);
}

ArxReturnCode arx_pistoris_animation_replace_keyframes(ArxAnimation* animation, uint32_t frame_length,
                                                       const ArxAnimationKeyframeInput* keyframes,
                                                       size_t keyframe_count, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(keyframes, keyframe_count))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (size_t index = 0; index < keyframe_count; ++index)
    if (!pistoris::c_api::valid(keyframes[index])) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.replaceKeyframes(frame_length, keyframes, keyframe_count), error);
}

ArxReturnCode arx_pistoris_animation_clear_keyframes(ArxAnimation* animation, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  animation->value.clearKeyframes();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_claim_group(ArxAnimation* animation, size_t group, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.claimGroup(group), error);
}

ArxReturnCode arx_pistoris_animation_unclaim_group(ArxAnimation* animation, size_t group, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.unclaimGroup(group), error);
}

ArxReturnCode arx_pistoris_animation_void_group(ArxAnimation* animation, size_t group, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.voidGroup(group), error);
}

ArxReturnCode arx_pistoris_animation_compact_sounds(ArxAnimation* animation, size_t* out_removed,
                                                    ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = animation->value.compactSounds();
  if (!result) return pistoris::c_api::publish(result, error);
  if (out_removed) *out_removed = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_rebase_sound_paths(ArxAnimation* animation, ArxStringView directory,
                                                        ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(directory)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.rebaseSoundPaths(pistoris::c_api::stringView(directory)), error);
}

ArxReturnCode arx_pistoris_animation_set_sound(ArxAnimation* animation, ArxSoundIndex index, const ArxSoundView* sound,
                                               ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!sound || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.setSound(index, *sound), error);
}

ArxReturnCode arx_pistoris_animation_add_sound(ArxAnimation* animation, const ArxSoundView* sound,
                                               ArxSoundIndex* out_index, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!sound || !out_index || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_index = ARX_NO_SOUND;
  auto result = animation->value.addSound(*sound);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_index = *result;
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_set_sound_path(ArxAnimation* animation, ArxSoundIndex index, ArxStringView path,
                                                    ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(path)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.setSoundPath(index, pistoris::c_api::stringView(path)), error);
}

ArxReturnCode arx_pistoris_animation_set_sound_data(ArxAnimation* animation, ArxSoundIndex index,
                                                    ArxEncodedAudioView encoded_audio, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(encoded_audio)) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  return pistoris::c_api::publish(animation->value.setSoundData(index, encoded_audio), error);
}

ArxReturnCode arx_pistoris_animation_clear_sound_data(ArxAnimation* animation, ArxSoundIndex index,
                                                      ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.clearSoundData(index), error);
}

ArxReturnCode arx_pistoris_animation_remove_sound(ArxAnimation* animation, ArxSoundIndex index,
                                                  ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.removeSound(index), error);
}

// NOLINTEND(readability-identifier-naming)
