// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "modules/animation.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

namespace pistoris::animation {
namespace {

Error validateTimelineStorage(std::size_t keyframe_count, std::size_t group_count) noexcept {
  if (keyframe_count == 0) return Error::kNoKeyframes;
  if (keyframe_count > kMaxKeyframes) return Error::kTooManyKeyframes;
  if (group_count > kMaxGroups) return Error::kTooManyGroups;
  if (group_count != 0 && keyframe_count > std::numeric_limits<std::size_t>::max() / group_count)
    return Error::kBadTransformCount;
  return Error::kNone;
}

}  // namespace

Error validateName(std::string_view name) noexcept {
  return isIdentifier(name, {.max_length = kMaxNameLength}) ? Error::kNone : Error::kBadName;
}

Error validateKeyframe(const AnimationKeyframe& keyframe, std::size_t sound_count) noexcept {
  if (keyframe.frame > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) return Error::kBadFrame;
  if (!math::finite(keyframe.root_translation) || !math::finite(keyframe.root_rotation))
    return Error::kBadRootTransform;
  if (keyframe.sound != kNoSoundHandle) {
    SoundKind kind = SoundKind::kEffect;
    SoundIndex index = kNoSound;
    if (soundHandleKind(keyframe.sound, kind) != ARX_OK || soundHandleIndex(keyframe.sound, index) != ARX_OK ||
        kind != SoundKind::kEffect || static_cast<std::size_t>(index) >= sound_count)
      return Error::kBadSound;
  }
  return Error::kNone;
}

Error validateTransform(const AnimationGroupTransform& transform) noexcept {
  return math::finite(transform.rotation) && transform.rotation.w >= 0.0f && math::finite(transform.translation) &&
                 math::finite(transform.scale)
             ? Error::kNone
             : Error::kBadGroupTransform;
}

Error validateFrameLength(const AnimationData& animation, std::uint32_t frame_length) noexcept {
  if ((!animation.keyframes.empty() && frame_length < animation.keyframes.back().frame) ||
      frame_length > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    return Error::kBadFrameLength;
  return Error::kNone;
}

Error validateTimelineShape(std::uint32_t frame_length, std::size_t keyframe_count, std::size_t group_count) noexcept {
  const Error storage_error = validateTimelineStorage(keyframe_count, group_count);
  if (storage_error != Error::kNone) return storage_error;
  if (frame_length > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
    return Error::kBadFrameLength;
  return Error::kNone;
}

Error validateKeyframeAppend(const AnimationData& animation, std::uint32_t frame, std::size_t group_count) noexcept {
  const Error storage_error = validateTimelineStorage(animation.keyframes.size() + 1U, group_count);
  if (storage_error != Error::kNone) return storage_error;
  if (!animation.keyframes.empty()) {
    if (group_count != animation.group_count) return Error::kBadTransformCount;
    if (frame <= animation.keyframes.back().frame) return Error::kBadFrame;
  }
  if (frame > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) return Error::kBadFrame;
  return Error::kNone;
}

Error validateKeyframeSet(const AnimationData& animation, std::size_t index, const AnimationKeyframe& keyframe,
                          std::span<const AnimationGroupTransform> group_transforms, std::size_t sound_count) noexcept {
  if (index >= animation.keyframes.size()) return Error::kBadIndex;
  if (group_transforms.size() != animation.group_count) return Error::kBadTransformCount;
  Error error = validateKeyframe(keyframe, sound_count);
  if (error != Error::kNone) return error;
  for (const AnimationGroupTransform& transform : group_transforms) {
    error = validateTransform(transform);
    if (error != Error::kNone) return error;
  }
  if ((index != 0 && keyframe.frame <= animation.keyframes[index - 1U].frame) ||
      (index + 1U != animation.keyframes.size() && keyframe.frame >= animation.keyframes[index + 1U].frame))
    return Error::kBadFrame;
  return keyframe.frame > animation.frame_length ? Error::kBadFrameLength : Error::kNone;
}

Error validateTimeline(std::uint32_t frame_length, std::size_t group_count,
                       std::span<const AnimationKeyframe> keyframes,
                       std::span<const AnimationGroupTransform> group_transforms, std::size_t sound_count) noexcept {
  const Error storage_error = validateTimelineStorage(keyframes.size(), group_count);
  if (storage_error != Error::kNone) {
    log(ARX_LOG_DEBUG,
        "Animation validation: timeline has {} keyframes and {} groups: error {}",
        keyframes.size(),
        group_count,
        static_cast<int>(storage_error));
    return storage_error;
  }
  const std::size_t expected_transform_count = keyframes.size() * group_count;
  if (group_transforms.size() != expected_transform_count) {
    log(ARX_LOG_DEBUG,
        "Animation validation: transform count {} does not match expected {}",
        group_transforms.size(),
        expected_transform_count);
    return Error::kBadTransformCount;
  }

  std::uint32_t previous = 0;
  bool first = true;
  for (std::size_t index = 0; index < keyframes.size(); ++index) {
    const AnimationKeyframe& keyframe = keyframes[index];
    if (!first && keyframe.frame <= previous) {
      log(ARX_LOG_DEBUG,
          "Animation validation: keyframe {} frame {} does not follow {}",
          index,
          keyframe.frame,
          previous);
      return Error::kBadFrame;
    }
    const Error error = validateKeyframe(keyframe, sound_count);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Animation validation: keyframe {} at frame {} is invalid: sound {}, error {}",
          index,
          keyframe.frame,
          keyframe.sound,
          static_cast<int>(error));
      return error;
    }
    previous = keyframe.frame;
    first = false;
  }
  if (frame_length < previous || frame_length > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())) {
    log(ARX_LOG_DEBUG,
        "Animation validation: frame length {} is outside [{}, {}]",
        frame_length,
        previous,
        std::numeric_limits<std::int32_t>::max());
    return Error::kBadFrameLength;
  }
  for (std::size_t index = 0; index < group_transforms.size(); ++index) {
    const AnimationGroupTransform& transform = group_transforms[index];
    const Error error = validateTransform(transform);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Animation validation: keyframe {} group {} has invalid transform",
          index / group_count,
          index % group_count);
      return error;
    }
  }
  return Error::kNone;
}

Error validate(const AnimationData& data, std::size_t sound_count) noexcept {
  const Error name_error = validateName(data.name);
  if (name_error != Error::kNone) {
    log(ARX_LOG_DEBUG, "Animation validation: invalid name '{}'", data.name);
    return name_error;
  }
  for (std::size_t group = data.group_count; group < kMaxGroups; ++group) {
    if (data.claimed_groups.test(group)) {
      log(ARX_LOG_DEBUG, "Animation validation: claimed group {} is outside group count {}", group, data.group_count);
      return Error::kBadGroupClaim;
    }
  }
  return validateTimeline(data.frame_length, data.group_count, data.keyframes, data.group_transforms, sound_count);
}

}  // namespace pistoris::animation
