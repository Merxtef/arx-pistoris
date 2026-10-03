// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.h"

#include "animation_helpers.h"
#include "support/animation_equivalence.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <utility>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

std::array<ArxAnimationGroupTransform, 2> transforms(float offset) {
  std::array<ArxAnimationGroupTransform, 2> result{};
  result[0].translation.x = offset;
  result[1].translation.y = offset;
  return result;
}

ArxSoundView soundView(std::string_view path) { return {view(path), {nullptr, 0}}; }

pistoris::Animation importAnimation(const pistoris::tea::Data& native) {
  pistoris::TeaResult<pistoris::Animation> result = pistoris::Animation::importNative(native);
  REQUIRE(result);
  return std::move(*result);
}

pistoris::tea::Data bakeAnimation(const pistoris::Animation& animation) {
  pistoris::AnimationResult<pistoris::tea::Data> result = animation.bakeNative();
  REQUIRE(result);
  return std::move(*result);
}

}  // namespace

TEST_SUITE("C++ Animation API") {
  TEST_CASE("Native import locates an invalid TEA group transform") {
    pistoris::tea::Data native = makeAnimationTea();
    native.keyframes[1].groups[1].translate.x = std::numeric_limits<float>::infinity();

    const pistoris::TeaResult<pistoris::Animation> result = pistoris::Animation::importNative(native);

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_TEA_BAD_GROUP_TRANSFORM);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::TeaElement::kGroupTransform);
    CHECK(result.error()->location()->index == 1);
    CHECK(result.error()->location()->subindex == 1);
  }

  TEST_CASE("Converts native animation data through the editing representation") {
    pistoris::tea::Data native = makeAnimationTea();
    std::strcpy(native.keyframes[1].sample->name, "custom__step");
    pistoris::Animation animation = importAnimation(native);
    CHECK((animation.name() == "walk"));
    CHECK(animation.resourcePath().empty());
    CHECK(animation.frameLength() == 9);
    CHECK(animation.groupCount() == 2);
    CHECK(animation.keyframeCount() == 3);

    const pistoris::Animation::KeyframesView keyframes = animation.keyframes();
    CHECK(keyframes[1].frame == 4);
    CHECK(keyframes[1].footstep == 1);
    CHECK(keyframes[1].sound == 0);
    const ArxSoundView sound = animation.sounds()[0];
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/custom__step.wav"));

    REQUIRE(animation.setResourcePath("ANIM:NPC:WALK__FAST"));
    CHECK((animation.resourcePath() == "graph/obj3d/anims/npc/walk__fast.tea"));
    REQUIRE(animation.setResourcePath(R"(Anims\Custom\MY_WALK.TEA)"));
    CHECK((animation.resourcePath() == "anims/custom/my_walk.tea"));
    pistoris::tea::Data baked = bakeAnimation(animation);
    CHECK((std::string_view(baked.name) == "arx-pistoris/walk"));
    CHECK(baked.num_frames == 9);
    CHECK(baked.num_groups == 2);
    REQUIRE(baked.keyframes[1].sample.has_value());
    CHECK((std::string_view(baked.keyframes[1].sample->name) == "custom__step"));
    pistoris::Animation roundtrip = importAnimation(baked);
    CHECK((roundtrip.name() == animation.name()));
  }

  TEST_CASE("Fills sparse native root transforms with native timing weights") {
    pistoris::tea::Data native = makeAnimationTea();
    native.keyframes[0].quat = ArxQuat{};
    native.keyframes[1].translate.reset();
    native.keyframes[1].quat.reset();
    native.keyframes[2].quat = ArxQuat{0.0f, 0.0f, 0.0f, 1.0f};

    pistoris::Animation animation = importAnimation(native);
    const pistoris::Animation::KeyframesView keyframes = animation.keyframes();
    CHECK(keyframes[1].root_translation.x == doctest::Approx(2.0f / 3.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(2.0f / 3.0f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(1.0f / 3.0f));

    pistoris::tea::Data baked = bakeAnimation(animation);
    REQUIRE(baked.keyframes[1].translate.has_value());
    REQUIRE(baked.keyframes[1].quat.has_value());
    CHECK(baked.keyframes[1].translate->x == doctest::Approx(2.0f / 3.0f));
    CHECK(baked.keyframes[1].quat->w == doctest::Approx(2.0f / 3.0f));
    CHECK(baked.keyframes[1].quat->z == doctest::Approx(1.0f / 3.0f));
  }

  TEST_CASE("Owns replacement data and preserves dense group transforms") {
    pistoris::Animation animation;
    REQUIRE(animation.setName("custom"));
    const pistoris::AnimationResult<pistoris::SoundIndex> added_sound = animation.addSound(soundView("sfx/start.wav"));
    REQUIRE(added_sound);
    const pistoris::SoundIndex sound = *added_sound;
    REQUIRE(animation.setSoundPath(sound, "sfx/renamed.wav"));
    const ArxSoundView renamed_sound = animation.sounds()[sound];
    CHECK((std::string_view(renamed_sound.path.data, renamed_sound.path.size) == "sfx/renamed.wav"));
    auto first_transforms = transforms(1.0f);
    auto second_transforms = transforms(2.0f);
    const std::array<ArxAnimationKeyframeInput, 2> input = {{
        {{0, {}, {}, 0, sound}, first_transforms.data(), first_transforms.size()},
        {{6, {3.0f, 0.0f, 0.0f}, {}, 1, pistoris::kNoSound}, second_transforms.data(), second_transforms.size()},
    }};
    REQUIRE(animation.replaceKeyframes(10, input.data(), input.size()));
    first_transforms[0].translation.x = 99.0f;

    const auto copied = animation.groupTransforms(0);
    REQUIRE(copied);
    CHECK((*copied)[0].translation.x == 1.0f);
    CHECK((*copied)[1].translation.y == 1.0f);
    CHECK(animation.frameLength() == 10);
    CHECK(animation.validate());

    CHECK(animation.replaceKeyframes(0, nullptr, 0).code() == ARX_ANIMATION_NO_KEYFRAMES);
    CHECK(animation.keyframeCount() == 2);
    CHECK(animation.groupCount() == 2);
    CHECK(animation.frameLength() == 10);
    CHECK(animation.setFrameLength(5).code() == ARX_ANIMATION_BAD_FRAME_LENGTH);
    CHECK(animation.groupTransforms(2).code() == ARX_INDEX_OUT_OF_RANGE);
    animation.clearKeyframes();
    CHECK(animation.validate().code() == ARX_ANIMATION_NO_KEYFRAMES);
  }

  TEST_CASE("Rejects an invalid first keyframe without changing Animation state") {
    pistoris::Animation animation;
    const ArxAnimationKeyframeInput input{
        {std::numeric_limits<std::uint32_t>::max(), {}, {}, 0, pistoris::kNoSound}, nullptr, 0};
    CHECK(animation.addKeyframe(input).code() == ARX_ANIMATION_BAD_FRAME);
    CHECK(animation.keyframeCount() == 0);
    CHECK(animation.groupCount() == 0);
    CHECK(animation.frameLength() == 0);

    const ArxAnimationKeyframeInput invalid_payload{{0, {}, {}, 0, pistoris::kNoSound}, nullptr, 1};
    CHECK(animation.setKeyframe(0, invalid_payload).code() == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Rejects an invalid sound index before validating audio") {
    pistoris::Animation animation;
    const std::array<std::uint8_t, 1> malformed_audio{};

    CHECK(animation.setSoundData(0, {malformed_audio.data(), malformed_audio.size()}).code() == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Rejects invalid resource paths and removal of referenced sounds") {
    pistoris::Animation animation;
    CHECK(animation.setResourcePath("walk").code() == ARX_ANIMATION_BAD_RESOURCE_PATH);

    const auto sound_result = animation.addSound(soundView("sfx/used.wav"));
    REQUIRE(sound_result);
    const pistoris::SoundIndex sound = *sound_result;
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, sound}, nullptr, 0};
    REQUIRE(animation.addKeyframe(input));
    CHECK(animation.removeSound(sound).code() == ARX_ANIMATION_SOUND_IN_USE);
  }

  TEST_CASE("Transforms root and group motion without changing group scale") {
    pistoris::Animation animation = importAnimation(makeAnimationTea());

    REQUIRE(animation.scale(2.0f));
    REQUIRE(animation.rotate({2.0f, 0.0f, 0.0f, 2.0f}));

    const pistoris::Animation::KeyframesView keyframes = animation.keyframes();
    CHECK(keyframes[1].root_translation.x == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_translation.y == doctest::Approx(2.0f));
    CHECK(keyframes[1].root_rotation.w == doctest::Approx(1.0f));
    CHECK(keyframes[1].root_rotation.x == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.y == doctest::Approx(0.0f));
    CHECK(keyframes[1].root_rotation.z == doctest::Approx(0.0f));

    const auto groups = animation.groupTransforms(1);
    REQUIRE(groups);
    CHECK((*groups)[0].translation.x == doctest::Approx(0.0f));
    CHECK((*groups)[0].translation.y == doctest::Approx(2.0f));
    CHECK((*groups)[1].translation.x == doctest::Approx(-2.0f));
    CHECK((*groups)[1].translation.y == doctest::Approx(2.0f));
    CHECK((*groups)[1].scale.x == doctest::Approx(1.1f));

    CHECK(animation.scale(0.0f).code() == ARX_INVALID_OPTIONS);
    CHECK(animation.rotate({0.0f, 0.0f, 0.0f, 0.0f}).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Tracks explicit and derived group state") {
    pistoris::Animation animation;
    REQUIRE(animation.setName("guarded"));
    std::array<ArxAnimationGroupTransform, 2> groups{};
    groups[1].translation.x = 1.0f;
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.addKeyframe(input));

    auto state = animation.isGroupVoid(0);
    REQUIRE(state);
    CHECK(*state);
    REQUIRE(animation.claimGroup(0));
    state = animation.isGroupClaimed(0);
    REQUIRE(state);
    CHECK(*state);
    state = animation.isGroupVoid(0);
    REQUIRE(state);
    CHECK_FALSE(*state);
    REQUIRE(animation.voidGroup(1));
    state = animation.isGroupVoid(1);
    REQUIRE(state);
    CHECK(*state);
    state = animation.isGroupClaimed(1);
    REQUIRE(state);
    CHECK_FALSE(*state);
    REQUIRE(animation.unclaimGroup(0));
    groups[0].translation.x = 2.0f;
    groups[1] = {};
    REQUIRE(animation.setKeyframe(0, input));
    state = animation.isGroupVoid(0);
    REQUIRE(state);
    CHECK_FALSE(*state);
    groups[0] = {};
    REQUIRE(animation.setKeyframe(0, input));
    state = animation.isGroupVoid(0);
    REQUIRE(state);
    CHECK(*state);

    groups[1].translation.z = 3.0f;
    const ArxAnimationKeyframeInput appended{{1, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.addKeyframe(appended));
    state = animation.isGroupVoid(1);
    REQUIRE(state);
    CHECK_FALSE(*state);
    REQUIRE(animation.removeKeyframe(1));
    state = animation.isGroupVoid(1);
    REQUIRE(state);
    CHECK(*state);

    groups[1] = {};
    groups[0].translation.y = 4.0f;
    const ArxAnimationKeyframeInput replacement{{0, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.replaceKeyframes(0, &replacement, 1));
    state = animation.isGroupVoid(0);
    REQUIRE(state);
    CHECK_FALSE(*state);
    CHECK(animation.claimGroup(2).code() == ARX_INDEX_OUT_OF_RANGE);
  }

  TEST_CASE("Preserves native identity guards as explicit claims") {
    pistoris::tea::Data native = makeAnimationTea();
    for (pistoris::tea::Keyframe& keyframe : native.keyframes) keyframe.groups[0] = {};
    native.keyframes.front().groups[0].quat = {-1.0f, 0.0f, 0.0f, 0.0f};

    pistoris::Animation animation = importAnimation(native);
    auto state = animation.isGroupClaimed(0);
    REQUIRE(state);
    CHECK(*state);
    state = animation.isGroupVoid(0);
    REQUIRE(state);
    CHECK_FALSE(*state);
    const auto transforms = animation.groupTransforms(0);
    REQUIRE(transforms);
    const ArxAnimationGroupTransform transform = (*transforms)[0];
    CHECK(transform.rotation.w == 1.0f);

    pistoris::tea::Data baked = bakeAnimation(animation);
    CHECK(baked.keyframes.front().groups[0].quat.w == -1.0f);
    REQUIRE(animation.unclaimGroup(0));
    baked = bakeAnimation(animation);
    CHECK(baked.keyframes.front().groups[0].quat.w == 1.0f);
  }

  TEST_CASE("Native bake omits the trailing void group suffix") {
    pistoris::Animation animation;
    REQUIRE(animation.setName("trimmed"));
    std::array<ArxAnimationGroupTransform, 3> groups{};
    groups[0].translation.x = 1.0f;
    const ArxAnimationKeyframeInput input{{0, {}, {}, 0, pistoris::kNoSound}, groups.data(), groups.size()};
    REQUIRE(animation.replaceKeyframes(0, &input, 1));

    pistoris::tea::Data baked = bakeAnimation(animation);
    CHECK(animation.groupCount() == 3);
    CHECK(baked.num_groups == 1);
    REQUIRE(baked.keyframes.front().groups.size() == 1);
    pistoris::Animation roundtrip = importAnimation(baked);
    test_support::checkAnimationsEquivalent(animation, roundtrip);

    REQUIRE(animation.voidGroup(0));
    baked = bakeAnimation(animation);
    CHECK(animation.groupCount() == 3);
    CHECK(baked.num_groups == 0);
    CHECK(baked.keyframes.front().groups.empty());
    roundtrip = importAnimation(baked);
    test_support::checkAnimationsEquivalent(animation, roundtrip);

    REQUIRE(animation.claimGroup(0));
    baked = bakeAnimation(animation);
    CHECK(baked.num_groups == 1);
    REQUIRE(baked.keyframes.front().groups.size() == 1);
    CHECK(baked.keyframes.front().groups[0].quat.w == -1.0f);
    roundtrip = importAnimation(baked);
    test_support::checkAnimationsEquivalent(animation, roundtrip);
  }
}
