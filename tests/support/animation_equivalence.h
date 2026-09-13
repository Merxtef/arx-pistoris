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
    bool is_void = false;
    const ArxReturnCode rc = animation.isGroupVoid(group, is_void);
    CHECK(rc == ARX_OK);
    if (rc != ARX_OK) return false;
    CHECK(is_void);
    all_void = all_void && is_void;
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
    bool lhs_claimed = false;
    bool rhs_claimed = false;
    const ArxReturnCode lhs_rc = lhs.isGroupClaimed(group, lhs_claimed);
    const ArxReturnCode rhs_rc = rhs.isGroupClaimed(group, rhs_claimed);
    CHECK(lhs_rc == ARX_OK);
    CHECK(rhs_rc == ARX_OK);
    if (lhs_rc != ARX_OK || rhs_rc != ARX_OK) return;
    CHECK(lhs_claimed == rhs_claimed);
  }

  std::vector<ArxAnimationKeyframe> lhs_keyframes(lhs.keyframeCount());
  std::vector<ArxAnimationKeyframe> rhs_keyframes(rhs.keyframeCount());
  const ArxReturnCode lhs_keyframes_rc = lhs.copyKeyframes(0, lhs_keyframes.size(), lhs_keyframes.data());
  const ArxReturnCode rhs_keyframes_rc = rhs.copyKeyframes(0, rhs_keyframes.size(), rhs_keyframes.data());
  CHECK(lhs_keyframes_rc == ARX_OK);
  CHECK(rhs_keyframes_rc == ARX_OK);
  if (lhs_keyframes_rc != ARX_OK || rhs_keyframes_rc != ARX_OK) return;
  std::vector<ArxAnimationGroupTransform> lhs_groups(shared_groups);
  std::vector<ArxAnimationGroupTransform> rhs_groups(shared_groups);

  for (std::size_t keyframe = 0; keyframe < lhs_keyframes.size(); ++keyframe) {
    const ArxAnimationKeyframe& lhs_keyframe = lhs_keyframes[keyframe];
    const ArxAnimationKeyframe& rhs_keyframe = rhs_keyframes[keyframe];
    CHECK(lhs_keyframe.frame == rhs_keyframe.frame);
    equivalence::checkVectorEqual(
        lhs_keyframe.root_translation, rhs_keyframe.root_translation, options.comparison_epsilon);
    equivalence::checkQuatEqual(lhs_keyframe.root_rotation, rhs_keyframe.root_rotation, options.comparison_epsilon);
    CHECK(lhs_keyframe.footstep == rhs_keyframe.footstep);
    CHECK(lhs_keyframe.sound == rhs_keyframe.sound);

    const ArxReturnCode lhs_groups_rc = lhs.copyGroupTransforms(keyframe, 0, lhs_groups.size(), lhs_groups.data());
    const ArxReturnCode rhs_groups_rc = rhs.copyGroupTransforms(keyframe, 0, rhs_groups.size(), rhs_groups.data());
    CHECK(lhs_groups_rc == ARX_OK);
    CHECK(rhs_groups_rc == ARX_OK);
    if (lhs_groups_rc != ARX_OK || rhs_groups_rc != ARX_OK) return;
    for (std::size_t group = 0; group < lhs_groups.size(); ++group) {
      equivalence::checkQuatEqual(lhs_groups[group].rotation, rhs_groups[group].rotation, options.comparison_epsilon);
      equivalence::checkVectorEqual(
          lhs_groups[group].translation, rhs_groups[group].translation, options.comparison_epsilon);
      equivalence::checkVectorEqual(lhs_groups[group].scale, rhs_groups[group].scale, options.comparison_epsilon);
    }
  }

  checkSoundsEquivalent(lhs, rhs, options.sounds);
}

}  // namespace test_support
