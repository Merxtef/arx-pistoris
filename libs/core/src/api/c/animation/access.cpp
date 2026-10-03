// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/animation/internal.h"  // IWYU pragma: keep
#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_name(const ArxAnimation* animation, ArxStringView* out_name,
                                          ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_name) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_name = pistoris::c_api::view(animation->value.name());
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_resource_path(const ArxAnimation* animation, ArxStringView* out_path,
                                                   ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_path) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_path = pistoris::c_api::view(animation->value.resourcePath());
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_frame_length(const ArxAnimation* animation, uint32_t* out_length,
                                                  ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_length) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_length = animation->value.frameLength();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_group_count(const ArxAnimation* animation, size_t* out_count,
                                                 ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = animation->value.groupCount();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_is_group_void(const ArxAnimation* animation, size_t group, uint8_t* out_void,
                                                   ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_void) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  auto result = animation->value.isGroupVoid(group);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_void = static_cast<uint8_t>(*result);
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_is_group_claimed(const ArxAnimation* animation, size_t group, uint8_t* out_claimed,
                                                      ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_claimed) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  auto result = animation->value.isGroupClaimed(group);
  if (!result) return pistoris::c_api::publish(result, error);
  *out_claimed = static_cast<uint8_t>(*result);
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_keyframe_count(const ArxAnimation* animation, size_t* out_count,
                                                    ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = animation->value.keyframeCount();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_sound_count(const ArxAnimation* animation, size_t* out_count,
                                                 ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = animation->value.soundCount();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_copy_sound_views(const ArxAnimation* animation, size_t offset, size_t count,
                                                      ArxSoundView* out_sounds, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (offset > animation->value.soundCount() || count > animation->value.soundCount() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_sounds) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Animation::SoundsView values = animation->value.sounds();
  for (std::size_t index = 0; index < count; ++index) out_sounds[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_copy_keyframes(const ArxAnimation* animation, size_t offset, size_t count,
                                                    ArxAnimationKeyframe* out_keyframes, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (offset > animation->value.keyframeCount() || count > animation->value.keyframeCount() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_keyframes) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  const pistoris::Animation::KeyframesView values = animation->value.keyframes();
  for (std::size_t index = 0; index < count; ++index) out_keyframes[index] = values[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_animation_copy_group_transforms(const ArxAnimation* animation, size_t keyframe,
                                                           size_t offset, size_t count,
                                                           ArxAnimationGroupTransform* out_transforms,
                                                           ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = animation->value.groupTransforms(keyframe);
  if (!result) return pistoris::c_api::publish(result, error);
  if (offset > result->size() || count > result->size() - offset)
    return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  if (count != 0 && !out_transforms) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < count; ++index) out_transforms[index] = (*result)[offset + index];
  return pistoris::c_api::publishCode(ARX_OK, error);
}

// NOLINTEND(readability-identifier-naming)
