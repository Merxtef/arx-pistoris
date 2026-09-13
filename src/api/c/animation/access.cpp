// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/sound.h"

#include "api/c/animation/internal.h"  // IWYU pragma: keep
#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_name(const ArxAnimation* animation, ArxStringView* out_name) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_name) return ARX_INVALID_DATA_POINTER;
  *out_name = pistoris::c_api::view(animation->value.name());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_resource_path(const ArxAnimation* animation, ArxStringView* out_path) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = pistoris::c_api::view(animation->value.resourcePath());
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_frame_length(const ArxAnimation* animation, uint32_t* out_length) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_length) return ARX_INVALID_DATA_POINTER;
  *out_length = animation->value.frameLength();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_group_count(const ArxAnimation* animation, size_t* out_count) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = animation->value.groupCount();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_is_group_void(const ArxAnimation* animation, size_t group,
                                                   uint8_t* out_void) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_void) return ARX_INVALID_DATA_POINTER;
  bool value = false;
  const ArxReturnCode rc = animation->value.isGroupVoid(group, value);
  *out_void = static_cast<uint8_t>(value);
  return rc;
}

ArxReturnCode arx_pistoris_animation_is_group_claimed(const ArxAnimation* animation, size_t group,
                                                      uint8_t* out_claimed) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_claimed) return ARX_INVALID_DATA_POINTER;
  bool value = false;
  const ArxReturnCode rc = animation->value.isGroupClaimed(group, value);
  *out_claimed = static_cast<uint8_t>(value);
  return rc;
}

ArxReturnCode arx_pistoris_animation_keyframe_count(const ArxAnimation* animation, size_t* out_count) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = animation->value.keyframeCount();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_sound_count(const ArxAnimation* animation, size_t* out_count) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = animation->value.soundCount();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_animation_copy_sound_views(const ArxAnimation* animation, size_t offset, size_t count,
                                                      ArxSoundView* out_sounds) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.copySoundViews(offset, count, out_sounds); });
}

ArxReturnCode arx_pistoris_animation_copy_keyframes(const ArxAnimation* animation, size_t offset, size_t count,
                                                    ArxAnimationKeyframe* out_keyframes) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.copyKeyframes(offset, count, out_keyframes); });
}

ArxReturnCode arx_pistoris_animation_copy_group_transforms(const ArxAnimation* animation, size_t keyframe,
                                                           size_t offset, size_t count,
                                                           ArxAnimationGroupTransform* out_transforms) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard(
      [&] { return animation->value.copyGroupTransforms(keyframe, offset, count, out_transforms); });
}

// NOLINTEND(readability-identifier-naming)
