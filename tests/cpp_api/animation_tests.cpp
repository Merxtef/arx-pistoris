// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.h"

#include "animation_helpers.h"
#include "support/animation_equivalence.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

std::array<ArxAnimationGroupTransform, 2> transforms(float offset) {
  std::array<ArxAnimationGroupTransform, 2> result{};
  result[0].translation.x = offset;
  result[1].translation.y = offset;
  return result;
}

ArxSoundView soundView(std::string_view path) { return {view(path), {nullptr, 0}}; }

}  // namespace

TEST_SUITE("C++ Animation API") {
  TEST_CASE("Converts native animation data through the editing representation") {
    pistoris::tea::Data native = makeAnimationTea();
    std::strcpy(native.keyframes[1].sample->name, "custom__step");
    pistoris::Animation animation;
    REQUIRE(pistoris::Animation::importNative(animation, native) == ARX_OK);
    CHECK((animation.name() == "walk"));
    CHECK(animation.resourcePath().empty());
    CHECK(animation.frameLength() == 9);
    CHECK(animation.groupCount() == 2);
    CHECK(animation.keyframeCount() == 3);

    std::array<ArxAnimationKeyframe, 3> keyframes{};
    REQUIRE(animation.copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].frame == 4);
    CHECK(keyframes[1].footstep == 1);
    CHECK(keyframes[1].sound == 0);
    ArxSoundView sound{};
    REQUIRE(animation.copySoundViews(0, 1, &sound) == ARX_OK);
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/custom__step.wav"));

    REQUIRE(animation.setResourcePath("ANIM:NPC:WALK__FAST") == ARX_OK);
    CHECK((animation.resourcePath() == "graph/obj3d/anims/npc/walk__fast.tea"));
    REQUIRE(animation.setResourcePath(R"(Anims\Custom\MY_WALK.TEA)") == ARX_OK);
    CHECK((animation.resourcePath() == "anims/custom/my_walk.tea"));
    pistoris::tea::Data baked;
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    CHECK((std::string_view(baked.name) == "arx-pistoris/walk"));
    CHECK(baked.num_frames == 9);
    CHECK(baked.num_groups == 2);
    REQUIRE(baked.keyframes[1].sample.has_value());
    CHECK((std::string_view(baked.keyframes[1].sample->name) == "custom__step"));
    pistoris::Animation roundtrip;
    REQUIRE(pistoris::Animation::importNative(roundtrip, baked) == ARX_OK);
    CHECK((roundtrip.name() == animation.name()));
  }

  TEST_CASE("Fills sparse native root transforms with native timing weights") {
    pistoris::tea::Data native = makeAnimationTea();
    native.keyframes[0].quat = ArxQuat{};
    native.keyframes[1].translate.reset();
    native.keyframes[1].quat.reset();
    native.keyframes[2].quat = ArxQuat{0.0f, 0.0f, 0.0f, 1.0f};

    pistoris::Animation animation;
    REQUIRE(pistoris::Animation::importNative(animation, native) == ARX_OK);
    std::array<ArxAnimationKeyframe, 3> keyframes{};
    REQUIRE(animation.copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(2.0f / 3.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(2.0f / 3.0f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(1.0f / 3.0f));

    pistoris::tea::Data baked;
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    REQUIRE(baked.keyframes[1].translate.has_value());
    REQUIRE(baked.keyframes[1].quat.has_value());
    CHECK(baked.keyframes[1].translate->x == doctest::Approx(2.0f / 3.0f));
    CHECK(baked.keyframes[1].quat->w == doctest::Approx(2.0f / 3.0f));
    CHECK(baked.keyframes[1].quat->z == doctest::Approx(1.0f / 3.0f));
  }

  TEST_CASE("Owns replacement data and preserves dense group transforms") {
    pistoris::Animation animation;
    REQUIRE(animation.setName("custom") == ARX_OK);
    pistoris::SoundIndex sound = pistoris::kNoSound;
    REQUIRE(animation.addSound(soundView("sfx/start.wav"), sound) == ARX_OK);
    auto first_transforms = transforms(1.0f);
    auto second_transforms = transforms(2.0f);
    const std::array<ArxAnimationKeyframeInput, 2> input = {{
        {{0, {}, {}, 0, sound}, first_transforms.data(), first_transforms.size()},
        {{6, {3.0f, 0.0f, 0.0f}, {}, 1, pistoris::kNoSound}, second_transforms.data(), second_transforms.size()},
    }};
    REQUIRE(animation.replaceKeyframes(10, input.data(), input.size()) == ARX_OK);
    first_transforms[0].translation.x = 99.0f;

    std::array<ArxAnimationGroupTransform, 2> copied{};
    REQUIRE(animation.copyGroupTransforms(0, 0, copied.size(), copied.data()) == ARX_OK);
    CHECK(copied[0].translation.x == 1.0f);
    CHECK(copied[1].translation.y == 1.0f);
    CHECK(animation.frameLength() == 10);
    CHECK(animation.validate() == ARX_OK);

    CHECK(animation.replaceKeyframes(0, nullptr, 0) == ARX_ANIMATION_NO_KEYFRAMES);
    CHECK(animation.keyframeCount() == 2);
    CHECK(animation.groupCount() == 2);
    CHECK(animation.frameLength() == 10);
    CHECK(animation.setFrameLength(5) == ARX_ANIMATION_BAD_FRAME_LENGTH);
    CHECK(animation.copyGroupTransforms(2, 0, 0, nullptr) == ARX_INDEX_OUT_OF_RANGE);
    animation.clearKeyframes();
    CHECK(animation.validate() == ARX_ANIMATION_NO_KEYFRAMES);
  }

  TEST_CASE("Rejects an invalid first keyframe without changing Animation state") {
    pistoris::Animation animation;
    const ArxAnimationKeyframeInput input{
        {std::numeric_limits<std::uint32_t>::max(), {}, {}, 0, pistoris::kNoSound}, nullptr, 0};
    std::size_t index = 42;
    CHECK(animation.addKeyframe(input, index) == ARX_ANIMATION_BAD_FRAME);
    CHECK(index == std::numeric_limits<std::size_t>::max());
    CHECK(animation.keyframeCount() == 0);
    CHECK(animation.groupCount() == 0);
    CHECK(animation.frameLength() == 0);

    const ArxAnimationKeyframeInput invalid_payload{{0, {}, {}, 0, pistoris::kNoSound}, nullptr, 1};
    CHECK(animation.setKeyframe(0, invalid_payload) == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Rejects an invalid sound index before validating audio") {
    pistoris::Animation animation;
    const std::array<std::uint8_t, 1> malformed_audio{};

    CHECK(animation.setSoundData(0, {malformed_audio.data(), malformed_audio.size()}) == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Transforms root and group motion without changing group scale") {
    pistoris::Animation animation;
    REQUIRE(pistoris::Animation::importNative(animation, makeAnimationTea()) == ARX_OK);

    REQUIRE(animation.scale(2.0f) == ARX_OK);
    REQUIRE(animation.rotate({2.0f, 0.0f, 0.0f, 2.0f}) == ARX_OK);

    std::array<ArxAnimationKeyframe, 3> keyframes{};
    REQUIRE(animation.copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].root_translation.x == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(2.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(1.0f));
    CHECK(keyframes[1].root_rotation.x == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.y == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(0.0f));

    std::array<ArxAnimationGroupTransform, 2> groups{};
    REQUIRE(animation.copyGroupTransforms(1, 0, groups.size(), groups.data()) == ARX_OK);
    CHECK(groups[0].translation.x == doctest::Approx(0.0f));
    CHECK(groups[0].translation.y == doctest::Approx(2.0f));
    CHECK(groups[1].translation.x == doctest::Approx(-2.0f));
    CHECK(groups[1].translation.y == doctest::Approx(2.0f));
    CHECK(groups[1].scale.x == doctest::Approx(1.1f));

    CHECK(animation.scale(0.0f) == ARX_INVALID_OPTIONS);
    CHECK(animation.rotate({0.0f, 0.0f, 0.0f, 0.0f}) == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Tracks explicit and derived group state") {
    pistoris::Animation animation;
    REQUIRE(animation.setName("guarded") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 2> groups{};
    groups[1].translation.x = 1.0f;
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    std::size_t index = std::numeric_limits<std::size_t>::max();
    REQUIRE(animation.addKeyframe(input, index) == ARX_OK);

    bool value = false;
    REQUIRE(animation.isGroupVoid(0, value) == ARX_OK);
    CHECK(value);
    REQUIRE(animation.claimGroup(0) == ARX_OK);
    REQUIRE(animation.isGroupClaimed(0, value) == ARX_OK);
    CHECK(value);
    REQUIRE(animation.isGroupVoid(0, value) == ARX_OK);
    CHECK_FALSE(value);
    REQUIRE(animation.voidGroup(1) == ARX_OK);
    REQUIRE(animation.isGroupVoid(1, value) == ARX_OK);
    CHECK(value);
    REQUIRE(animation.isGroupClaimed(1, value) == ARX_OK);
    CHECK_FALSE(value);
    REQUIRE(animation.unclaimGroup(0) == ARX_OK);
    groups[0].translation.x = 2.0f;
    groups[1] = {};
    REQUIRE(animation.setKeyframe(0, input) == ARX_OK);
    REQUIRE(animation.isGroupVoid(0, value) == ARX_OK);
    CHECK_FALSE(value);
    groups[0] = {};
    REQUIRE(animation.setKeyframe(0, input) == ARX_OK);
    REQUIRE(animation.isGroupVoid(0, value) == ARX_OK);
    CHECK(value);

    groups[1].translation.z = 3.0f;
    const ArxAnimationKeyframeInput appended{{1, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.addKeyframe(appended, index) == ARX_OK);
    REQUIRE(animation.isGroupVoid(1, value) == ARX_OK);
    CHECK_FALSE(value);
    REQUIRE(animation.removeKeyframe(1) == ARX_OK);
    REQUIRE(animation.isGroupVoid(1, value) == ARX_OK);
    CHECK(value);

    groups[1] = {};
    groups[0].translation.y = 4.0f;
    const ArxAnimationKeyframeInput replacement{{0, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.replaceKeyframes(0, &replacement, 1) == ARX_OK);
    REQUIRE(animation.isGroupVoid(0, value) == ARX_OK);
    CHECK_FALSE(value);
    CHECK(animation.claimGroup(2) == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Preserves native identity guards as explicit claims") {
    pistoris::tea::Data native = makeAnimationTea();
    for (pistoris::tea::Keyframe& keyframe : native.keyframes) keyframe.groups[0] = {};
    native.keyframes.front().groups[0].quat = {-1.0f, 0.0f, 0.0f, 0.0f};

    pistoris::Animation animation;
    REQUIRE(pistoris::Animation::importNative(animation, native) == ARX_OK);
    bool value = false;
    REQUIRE(animation.isGroupClaimed(0, value) == ARX_OK);
    CHECK(value);
    REQUIRE(animation.isGroupVoid(0, value) == ARX_OK);
    CHECK_FALSE(value);
    ArxAnimationGroupTransform transform{};
    REQUIRE(animation.copyGroupTransforms(0, 0, 1, &transform) == ARX_OK);
    CHECK(transform.rotation.w == 1.0f);

    pistoris::tea::Data baked;
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    CHECK(baked.keyframes.front().groups[0].quat.w == -1.0f);
    REQUIRE(animation.unclaimGroup(0) == ARX_OK);
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    CHECK(baked.keyframes.front().groups[0].quat.w == 1.0f);
  }

  TEST_CASE("Native bake omits the trailing void group suffix") {
    pistoris::Animation animation;
    REQUIRE(animation.setName("trimmed") == ARX_OK);
    std::array<ArxAnimationGroupTransform, 3> groups{};
    groups[0].translation.x = 1.0f;
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.replaceKeyframes(0, &input, 1) == ARX_OK);

    pistoris::tea::Data baked;
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    CHECK(animation.groupCount() == 3);
    CHECK(baked.num_groups == 1);
    REQUIRE(baked.keyframes.front().groups.size() == 1);
    pistoris::Animation roundtrip;
    REQUIRE(pistoris::Animation::importNative(roundtrip, baked) == ARX_OK);
    test_support::checkAnimationsEquivalent(animation, roundtrip);

    REQUIRE(animation.voidGroup(0) == ARX_OK);
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    CHECK(animation.groupCount() == 3);
    CHECK(baked.num_groups == 0);
    CHECK(baked.keyframes.front().groups.empty());
    REQUIRE(pistoris::Animation::importNative(roundtrip, baked) == ARX_OK);
    test_support::checkAnimationsEquivalent(animation, roundtrip);

    REQUIRE(animation.claimGroup(0) == ARX_OK);
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    CHECK(baked.num_groups == 1);
    REQUIRE(baked.keyframes.front().groups.size() == 1);
    CHECK(baked.keyframes.front().groups[0].quat.w == -1.0f);
    REQUIRE(pistoris::Animation::importNative(roundtrip, baked) == ARX_OK);
    test_support::checkAnimationsEquivalent(animation, roundtrip);
  }
}
