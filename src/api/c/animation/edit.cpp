// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation/types.h"
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

ArxReturnCode arx_pistoris_animation_scale(ArxAnimation* animation, float factor) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.scale(factor); });
}

ArxReturnCode arx_pistoris_animation_rotate(ArxAnimation* animation, ArxQuat rotation) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.rotate(rotation); });
}

ArxReturnCode arx_pistoris_animation_set_name(ArxAnimation* animation, ArxStringView name) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(name)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return animation->value.setName(pistoris::c_api::stringView(name)); });
}

ArxReturnCode arx_pistoris_animation_set_resource_path(ArxAnimation* animation, ArxStringView path) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(path)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return animation->value.setResourcePath(pistoris::c_api::stringView(path)); });
}

ArxReturnCode arx_pistoris_animation_set_frame_length(ArxAnimation* animation, uint32_t frame_length) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.setFrameLength(frame_length); });
}

ArxReturnCode arx_pistoris_animation_set_keyframe(ArxAnimation* animation, size_t index,
                                                  const ArxAnimationKeyframeInput* keyframe) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!keyframe || !pistoris::c_api::valid(*keyframe)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return animation->value.setKeyframe(index, *keyframe); });
}

ArxReturnCode arx_pistoris_animation_add_keyframe(ArxAnimation* animation, const ArxAnimationKeyframeInput* keyframe,
                                                  size_t* out_index) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!keyframe || !out_index || !pistoris::c_api::valid(*keyframe)) return ARX_INVALID_DATA_POINTER;
  *out_index = SIZE_MAX;
  return pistoris::c_api::guard([&] { return animation->value.addKeyframe(*keyframe, *out_index); });
}

ArxReturnCode arx_pistoris_animation_remove_keyframe(ArxAnimation* animation, size_t index) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.removeKeyframe(index); });
}

ArxReturnCode arx_pistoris_animation_replace_keyframes(ArxAnimation* animation, uint32_t frame_length,
                                                       const ArxAnimationKeyframeInput* keyframes,
                                                       size_t keyframe_count) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(keyframes, keyframe_count)) return ARX_INVALID_DATA_POINTER;
  for (size_t index = 0; index < keyframe_count; ++index)
    if (!pistoris::c_api::valid(keyframes[index])) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return animation->value.replaceKeyframes(frame_length, keyframes, keyframe_count); });
}

ArxReturnCode arx_pistoris_animation_clear_keyframes(ArxAnimation* animation) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  animation->value.clearKeyframes();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_claim_group(ArxAnimation* animation, size_t group) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return animation->value.claimGroup(group);
}

ArxReturnCode arx_pistoris_animation_unclaim_group(ArxAnimation* animation, size_t group) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return animation->value.unclaimGroup(group);
}

ArxReturnCode arx_pistoris_animation_void_group(ArxAnimation* animation, size_t group) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return animation->value.voidGroup(group);
}

ArxReturnCode arx_pistoris_animation_compact_sounds(ArxAnimation* animation, size_t* out_removed) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.compactSounds(out_removed); });
}

ArxReturnCode arx_pistoris_animation_rebase_sound_paths(ArxAnimation* animation, ArxStringView directory) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(directory)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard(
      [&] { return animation->value.rebaseSoundPaths(pistoris::c_api::stringView(directory)); });
}

ArxReturnCode arx_pistoris_animation_set_sound(ArxAnimation* animation, ArxSoundIndex index,
                                               const ArxSoundView* sound) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!sound || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return animation->value.setSound(index, *sound); });
}

ArxReturnCode arx_pistoris_animation_add_sound(ArxAnimation* animation, const ArxSoundView* sound,
                                               ArxSoundIndex* out_index) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!sound || !out_index || !pistoris::c_api::valid(sound->path) || !pistoris::c_api::valid(sound->encoded_audio))
    return ARX_INVALID_DATA_POINTER;
  *out_index = ARX_NO_SOUND;
  return pistoris::c_api::guard([&] { return animation->value.addSound(*sound, *out_index); });
}

ArxReturnCode arx_pistoris_animation_set_sound_data(ArxAnimation* animation, ArxSoundIndex index,
                                                    ArxEncodedAudioView encoded_audio) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(encoded_audio)) return ARX_INVALID_DATA_POINTER;
  return pistoris::c_api::guard([&] { return animation->value.setSoundData(index, encoded_audio); });
}

ArxReturnCode arx_pistoris_animation_clear_sound_data(ArxAnimation* animation, ArxSoundIndex index) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return animation->value.clearSoundData(index);
}

ArxReturnCode arx_pistoris_animation_remove_sound(ArxAnimation* animation, ArxSoundIndex index) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return animation->value.removeSound(index);
}

// NOLINTEND(readability-identifier-naming)
