// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "modules/animation.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

TEST_CASE("Animation validates dense transforms and frame bounds") {
  pistoris::AnimationData data;
  data.name = "walk";
  data.frame_length = 2;
  data.group_count = 1;
  data.keyframes.resize(2);
  data.keyframes[1].frame = 2;
  data.group_transforms.resize(2);
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kNone);

  data.group_transforms.pop_back();
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadTransformCount);
  data.group_transforms.resize(2);
  data.frame_length = 1;
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadFrameLength);
  data.frame_length = 2;
  data.keyframes[1].frame = 0;
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadFrame);
  data.keyframes[1].frame = 2;
  data.group_transforms[0].scale.x = std::numeric_limits<float>::infinity();
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadGroupTransform);
  data.group_transforms[0] = {};
  data.group_transforms[0].rotation.w = -1.0f;
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadGroupTransform);
  data.group_transforms[0] = {};
  data.keyframes[0].root_rotation.w = -1.0f;
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kNone);
}

TEST_CASE("Animation accepts root-only animation and validates sound indices") {
  pistoris::AnimationData data;
  data.name = "walk";
  data.keyframes.resize(1);
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kNone);

  data.keyframes[0].sound = 0;
  CHECK(pistoris::animation::validate(data, 1) == pistoris::animation::Error::kNone);
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadSound);
}

TEST_CASE("Animation rejects frames outside the supported range") {
  pistoris::AnimationData data;
  data.name = "walk";
  data.frame_length = std::numeric_limits<std::uint32_t>::max();
  data.keyframes.resize(1);
  data.keyframes[0].frame = static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) + 1U;
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadFrame);
}

TEST_CASE("Animation operations preserve dense transform alignment transactionally") {
  pistoris::AnimationData data;
  REQUIRE(pistoris::animation::validateName("walk") == pistoris::animation::Error::kNone);
  pistoris::animation::setName(data, "walk");
  const std::vector<pistoris::AnimationGroupTransform> transforms(2);
  pistoris::AnimationKeyframe keyframe;
  keyframe.frame = 1;
  REQUIRE(pistoris::animation::validateKeyframeAppend(data, keyframe.frame, transforms.size()) ==
          pistoris::animation::Error::kNone);
  REQUIRE(pistoris::animation::validateKeyframe(keyframe, 0) == pistoris::animation::Error::kNone);
  std::size_t index = pistoris::animation::addKeyframe(data, keyframe, transforms);
  CHECK(index == 0);
  CHECK(data.group_count == 2);
  CHECK(data.group_transforms.size() == 2);

  CHECK(pistoris::animation::validateKeyframeAppend(data, keyframe.frame, transforms.size()) ==
        pistoris::animation::Error::kBadFrame);
  CHECK(data.keyframes.size() == 1);
  CHECK(data.group_transforms.size() == 2);

  pistoris::animation::removeKeyframe(data, 0);
  CHECK(data.keyframes.empty());
  CHECK(data.group_transforms.empty());
  CHECK(data.group_count == 0);
  CHECK(data.frame_length == 0);
}

TEST_CASE("Animation replacement candidates can be rejected before publishing") {
  pistoris::AnimationData data;
  data.name = "walk";
  data.frame_length = 1;
  data.group_count = 1;
  pistoris::AnimationKeyframe retained;
  retained.frame = 1;
  data.keyframes = {retained};
  data.group_transforms.resize(1);

  pistoris::AnimationKeyframe replacement;
  replacement.frame = 2;
  std::vector<pistoris::AnimationKeyframe> keyframes = {replacement};
  std::vector<pistoris::AnimationGroupTransform> transforms(1);
  CHECK(pistoris::animation::validateTimeline(1, 1, keyframes, transforms, 0) ==
        pistoris::animation::Error::kBadFrameLength);
  REQUIRE(data.keyframes.size() == 1);
  CHECK(data.keyframes.front().frame == 1);
  CHECK(data.frame_length == 1);

  pistoris::AnimationKeyframe invalid = data.keyframes.front();
  invalid.root_translation.x = std::numeric_limits<float>::infinity();
  CHECK(pistoris::animation::validateKeyframeSet(data, 0, invalid, data.group_transforms, 0) ==
        pistoris::animation::Error::kBadRootTransform);
  CHECK(data.keyframes.front().root_translation.x == 0.0f);
}

TEST_CASE("Animation group claims are validated and voiding clears transforms") {
  pistoris::AnimationData data;
  data.name = "walk";
  data.group_count = 2;
  data.keyframes.resize(1);
  data.group_transforms.resize(2);
  data.group_transforms[0].translation.x = 1.0f;

  CHECK_FALSE(pistoris::animation::isIdentityGroup(data, 0));
  CHECK(pistoris::animation::isIdentityGroup(data, 1));
  pistoris::animation::claimGroup(data, 1);
  CHECK(pistoris::animation::isGroupClaimed(data, 1));
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kNone);

  pistoris::animation::voidGroup(data, 0);
  CHECK(pistoris::animation::isIdentityGroup(data, 0));
  CHECK_FALSE(pistoris::animation::isGroupClaimed(data, 0));

  data.claimed_groups.set(2);
  CHECK(pistoris::animation::validate(data, 0) == pistoris::animation::Error::kBadGroupClaim);
}
