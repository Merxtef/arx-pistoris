// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/animation.h"
#include "utils/identifier.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::animation {

void setName(AnimationData& animation, std::string_view name) {
  assert(validateName(name) == Error::kNone);
  animation.name = name;
}

void setFrameLength(AnimationData& animation, std::uint32_t frame_length) noexcept {
  assert(validateFrameLength(animation, frame_length) == Error::kNone);
  animation.frame_length = frame_length;
}

void setKeyframe(AnimationData& animation, std::size_t index, AnimationKeyframe keyframe,
                 std::span<const AnimationGroupTransform> group_transforms) noexcept {
  assert(index < animation.keyframes.size());
  assert(group_transforms.size() == animation.group_count);
  assert(index == 0 || keyframe.frame > animation.keyframes[index - 1U].frame);
  assert(index + 1U == animation.keyframes.size() || keyframe.frame < animation.keyframes[index + 1U].frame);
  assert(keyframe.frame <= animation.frame_length);
  animation.keyframes[index] = keyframe;
  const std::size_t first = index * animation.group_count;
  std::copy(group_transforms.begin(),
            group_transforms.end(),
            animation.group_transforms.begin() + static_cast<std::ptrdiff_t>(first));
}

std::size_t addKeyframe(AnimationData& animation, AnimationKeyframe keyframe,
                        std::span<const AnimationGroupTransform> group_transforms) {
  assert(validateKeyframeAppend(animation, keyframe.frame, group_transforms.size()) == Error::kNone);
  const std::size_t index = animation.keyframes.size();
  const std::size_t transform_count = animation.group_transforms.size();
  try {
    animation.keyframes.push_back(keyframe);
    animation.group_transforms.insert(
        animation.group_transforms.end(), group_transforms.begin(), group_transforms.end());
  } catch (...) {
    while (animation.group_transforms.size() > transform_count) animation.group_transforms.pop_back();
    while (animation.keyframes.size() > index) animation.keyframes.pop_back();
    throw;
  }
  if (index == 0) animation.group_count = group_transforms.size();
  animation.frame_length = std::max(animation.frame_length, animation.keyframes.back().frame);
  return index;
}

void removeKeyframe(AnimationData& animation, std::size_t index) {
  assert(index < animation.keyframes.size());
  const std::size_t first = index * animation.group_count;
  animation.group_transforms.erase(
      animation.group_transforms.begin() + static_cast<std::ptrdiff_t>(first),
      animation.group_transforms.begin() + static_cast<std::ptrdiff_t>(first + animation.group_count));
  animation.keyframes.erase(animation.keyframes.begin() + static_cast<std::ptrdiff_t>(index));
  if (animation.keyframes.empty()) {
    animation.group_count = 0;
    animation.frame_length = 0;
    animation.claimed_groups.reset();
  }
}

void replaceKeyframes(AnimationData& animation, std::uint32_t frame_length, std::size_t group_count,
                      std::vector<AnimationKeyframe>&& keyframes,
                      std::vector<AnimationGroupTransform>&& group_transforms) noexcept {
  animation.frame_length = frame_length;
  animation.group_count = group_count;
  animation.keyframes = std::move(keyframes);
  animation.group_transforms = std::move(group_transforms);
  for (std::size_t group = group_count; group < kMaxGroups; ++group) animation.claimed_groups.reset(group);
}

void clearKeyframes(AnimationData& animation) noexcept {
  animation.frame_length = 0;
  animation.group_count = 0;
  animation.keyframes.clear();
  animation.group_transforms.clear();
  animation.claimed_groups.reset();
}

void claimGroup(AnimationData& animation, std::size_t group) noexcept {
  assert(group < animation.group_count);
  animation.claimed_groups.set(group);
}

void unclaimGroup(AnimationData& animation, std::size_t group) noexcept {
  assert(group < animation.group_count);
  animation.claimed_groups.reset(group);
}

void voidGroup(AnimationData& animation, std::size_t group) noexcept {
  assert(group < animation.group_count);
  for (std::size_t frame = 0; frame < animation.keyframes.size(); ++frame)
    animation.group_transforms[frame * animation.group_count + group] = {};
  animation.claimed_groups.reset(group);
}

IdentifierRepair repairName(std::string& name) { return repairIdentifier(name, {.max_length = kMaxNameLength}); }

}  // namespace pistoris::animation
