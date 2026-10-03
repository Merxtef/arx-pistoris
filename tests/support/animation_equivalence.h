// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"

#include "support/equivalence.h"
#include "support/sound_equivalence.h"

#include <algorithm>
#include <cstddef>
#include <ostream>  // IWYU pragma: keep
#include <vector>

namespace test_support {
namespace animation_equivalence_detail {

inline bool trailingGroupsVoid(const pistoris::Animation& animation, std::size_t first) {
  bool all_void = true;
  for (std::size_t group = first; group < animation.groupCount(); ++group) {
    const pistoris::AnimationResult<bool> result = animation.isGroupVoid(group);
    CHECK(result);
    if (!result) return false;
    CHECK(*result);
    all_void = all_void && *result;
  }
  return all_void;
}

}  // namespace animation_equivalence_detail

struct AnimationEquivalenceOptions {
  SoundEquivalenceOptions sounds;
  float comparison_epsilon = 1e-5f;
};

inline void checkAnimationsEquivalent(const pistoris::Animation& lhs, const pistoris::Animation& rhs,
                                      AnimationEquivalenceOptions options = {}) {
  CHECK(lhs.name() == rhs.name());
  CHECK(lhs.resourcePath() == rhs.resourcePath());
  CHECK(lhs.frameLength() == rhs.frameLength());
  const std::size_t shared_groups = std::min(lhs.groupCount(), rhs.groupCount());
  const bool compatible_groups = animation_equivalence_detail::trailingGroupsVoid(lhs, shared_groups) &&
                                 animation_equivalence_detail::trailingGroupsVoid(rhs, shared_groups);
  if (!compatible_groups) return;
  if (lhs.keyframeCount() != rhs.keyframeCount()) {
    CHECK(lhs.keyframeCount() == rhs.keyframeCount());
    return;
  }

  for (std::size_t group = 0; group < shared_groups; ++group) {
    const pistoris::AnimationResult<bool> lhs_claimed = lhs.isGroupClaimed(group);
    const pistoris::AnimationResult<bool> rhs_claimed = rhs.isGroupClaimed(group);
    CHECK(lhs_claimed);
    CHECK(rhs_claimed);
    if (!lhs_claimed || !rhs_claimed) return;
    CHECK(*lhs_claimed == *rhs_claimed);
  }

  const pistoris::Animation::KeyframesView lhs_keyframes = lhs.keyframes();
  const pistoris::Animation::KeyframesView rhs_keyframes = rhs.keyframes();

  for (std::size_t keyframe = 0; keyframe < lhs_keyframes.size(); ++keyframe) {
    const ArxAnimationKeyframe& lhs_keyframe = lhs_keyframes[keyframe];
    const ArxAnimationKeyframe& rhs_keyframe = rhs_keyframes[keyframe];
    CHECK(lhs_keyframe.frame == rhs_keyframe.frame);
    equivalence::checkVectorEqual(
        lhs_keyframe.root_translation, rhs_keyframe.root_translation, options.comparison_epsilon);
    equivalence::checkQuatEqual(lhs_keyframe.root_rotation, rhs_keyframe.root_rotation, options.comparison_epsilon);
    CHECK(lhs_keyframe.footstep == rhs_keyframe.footstep);
    CHECK(lhs_keyframe.sound == rhs_keyframe.sound);

    const pistoris::AnimationResult<pistoris::Animation::GroupTransformsView> lhs_groups =
        lhs.groupTransforms(keyframe);
    const pistoris::AnimationResult<pistoris::Animation::GroupTransformsView> rhs_groups =
        rhs.groupTransforms(keyframe);
    CHECK(lhs_groups);
    CHECK(rhs_groups);
    if (!lhs_groups || !rhs_groups) return;
    for (std::size_t group = 0; group < shared_groups; ++group) {
      equivalence::checkQuatEqual(
          (*lhs_groups)[group].rotation, (*rhs_groups)[group].rotation, options.comparison_epsilon);
      equivalence::checkVectorEqual(
          (*lhs_groups)[group].translation, (*rhs_groups)[group].translation, options.comparison_epsilon);
      equivalence::checkVectorEqual((*lhs_groups)[group].scale, (*rhs_groups)[group].scale, options.comparison_epsilon);
    }
  }

  checkSoundsEquivalent(lhs, rhs, options.sounds);
}

}  // namespace test_support
