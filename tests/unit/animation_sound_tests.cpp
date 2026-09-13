// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
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
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

void configureAnimationWithSound(pistoris::Animation& animation, std::string_view path,
                                 std::span<const std::uint8_t> bytes) {
  REQUIRE(animation.setName("sound") == ARX_OK);
  pistoris::SoundIndex sound = pistoris::kNoSound;
  REQUIRE(animation.addSound({view(path), {bytes.data(), bytes.size()}}, sound) == ARX_OK);
  const ArxAnimationKeyframeInput keyframe{{0, {}, {}, 0, sound}, nullptr, 0};
  REQUIRE(animation.replaceKeyframes(1, &keyframe, 1) == ARX_OK);
}

}  // namespace

TEST_SUITE("Animation sounds") {
  TEST_CASE("Native conversion maps TEA samples through game sound paths") {
    pistoris::tea::Data native = makeAnimationTea();
    std::strcpy(native.keyframes[1].sample->name, "custom/step");
    pistoris::Animation animation;
    std::vector<pistoris::SoundSourceReference> sources;
    REQUIRE(pistoris::Animation::importNative(animation, native, &sources) == ARX_OK);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0].sound == 0);
    CHECK(sources[0].path == "custom/step");

    ArxSoundView sound{};
    REQUIRE(animation.copySoundViews(0, 1, &sound) == ARX_OK);
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/custom/step.wav"));
    std::array<ArxAnimationKeyframe, 3> keyframes{};
    REQUIRE(animation.copyKeyframes(0, keyframes.size(), keyframes.data()) == ARX_OK);
    CHECK(keyframes[1].sound == 0);

    pistoris::tea::Data baked;
    REQUIRE(animation.bakeNative(baked) == ARX_OK);
    REQUIRE(baked.keyframes[1].sample.has_value());
    CHECK((std::string_view(baked.keyframes[1].sample->name) == "custom/step"));
  }

  TEST_CASE("Native baking converts used audio to sfx-relative WAV sidecars") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Animation animation;
    configureAnimationWithSound(animation, "custom/step.mp3", wav);
    pistoris::NativeAnimationBundle bundle;
    REQUIRE(animation.bakeNativeBundle({}, bundle) == ARX_OK);
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
    std::array<pistoris::NativeAnimationBundle, 2> bundles;
    REQUIRE(first.bakeNativeBundle({}, bundles[0]) == ARX_OK);
    REQUIRE(second.bakeNativeBundle({}, bundles[1]) == ARX_OK);
    CHECK(bundles[0].sound_files[0].path == "sfx/custom/step.wav");
    CHECK(bundles[1].sound_files[0].path == "sfx/custom/step.wav");
    CHECK((std::string_view(bundles[0].tea.keyframes[0].sample->name) == "custom/step"));
    CHECK((std::string_view(bundles[1].tea.keyframes[0].sample->name) == "custom/step"));
  }
}
