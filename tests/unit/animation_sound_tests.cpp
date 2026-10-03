// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "animation_helpers.h"
#include "audio_helpers.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <ostream>  // IWYU pragma: keep
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

void configureAnimationWithSound(pistoris::Animation& animation, std::string_view path,
                                 std::span<const std::uint8_t> bytes) {
  REQUIRE(animation.setName("sound"));
  const auto sound_result = animation.addSound({view(path), {bytes.data(), bytes.size()}});
  REQUIRE(sound_result);
  const pistoris::SoundIndex sound = *sound_result;
  const ArxAnimationKeyframeInput keyframe{{0, {}, {}, 0, sound}, nullptr, 0};
  REQUIRE(animation.replaceKeyframes(1, &keyframe, 1));
}

}  // namespace

TEST_SUITE("Animation sounds") {
  TEST_CASE("Native conversion maps TEA samples through game sound paths") {
    pistoris::tea::Data native = makeAnimationTea();
    std::strcpy(native.keyframes[1].sample->name, "custom/step");
    std::vector<pistoris::SoundSourceReference> sources;
    auto import = pistoris::Animation::importNative(native, &sources);
    REQUIRE(import);
    pistoris::Animation animation = std::move(*import);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0].sound == 0);
    CHECK(sources[0].path == "custom/step");

    REQUIRE(animation.sounds().size() == 1);
    const ArxSoundView sound = animation.sounds()[0];
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/custom/step.wav"));
    REQUIRE(animation.keyframes().size() == 3);
    CHECK(animation.keyframes()[1].sound == 0);

    auto baked_result = animation.bakeNative();
    REQUIRE(baked_result);
    const pistoris::tea::Data& baked = *baked_result;
    REQUIRE(baked.keyframes[1].sample.has_value());
    CHECK((std::string_view(baked.keyframes[1].sample->name) == "custom/step"));
  }

  TEST_CASE("Native baking converts used audio to sfx-relative WAV sidecars") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Animation animation;
    configureAnimationWithSound(animation, "custom/step.mp3", wav);
    auto bundle_result = animation.bakeNativeBundle({});
    REQUIRE(bundle_result);
    const pistoris::NativeAnimationBundle& bundle = *bundle_result;
    REQUIRE(bundle.sound_files.size() == 1);
    CHECK(bundle.sound_files[0].path == "sfx/custom/step.wav");
    CHECK(bundle.sound_files[0].encoded_audio == wav);
    REQUIRE(bundle.tea.keyframes[0].sample.has_value());
    CHECK((std::string_view(bundle.tea.keyframes[0].sample->name) == "custom/step"));
  }

  TEST_CASE("Native projection remains independent between Animations") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Animation first;
    pistoris::Animation second;
    configureAnimationWithSound(first, "custom/step.mp3", wav);
    configureAnimationWithSound(second, "custom/step.ogg", wav);
    auto first_bundle = first.bakeNativeBundle({});
    auto second_bundle = second.bakeNativeBundle({});
    REQUIRE(first_bundle);
    REQUIRE(second_bundle);
    const std::array<pistoris::NativeAnimationBundle, 2> bundles = {std::move(*first_bundle),
                                                                    std::move(*second_bundle)};
    CHECK(bundles[0].sound_files[0].path == "sfx/custom/step.wav");
    CHECK(bundles[1].sound_files[0].path == "sfx/custom/step.wav");
    CHECK((std::string_view(bundles[0].tea.keyframes[0].sample->name) == "custom/step"));
    CHECK((std::string_view(bundles[1].tea.keyframes[0].sample->name) == "custom/step"));
  }
}
