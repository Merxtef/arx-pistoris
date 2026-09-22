// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "audio_helpers.h"
#include "image_helpers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxCinematicKeyframe keyframe(std::int32_t frame, pistoris::SoundHandle sound) {
  ArxCinematicKeyframe key;
  key.frame = frame;
  key.illustration = 0;
  key.camera_position = {10.0f, 20.0f, 30.0f};
  key.outgoing_speed = 1.0f;
  key.sound = sound;
  return key;
}

struct DreamWarningCapture {
  std::size_t count = 0;

  DreamWarningCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (level == ARX_LOG_WARN && message &&
              std::string_view(message).find("Dream crossfade") != std::string_view::npos)
            ++static_cast<DreamWarningCapture*>(userdata)->count;
        },
        this);
  }

  ~DreamWarningCapture() { pistoris::setLogCallback(nullptr, nullptr); }
};

}  // namespace

TEST_SUITE("C++ Cinematic API") {
  TEST_CASE("Owns cinematic resources and bakes canonical native data") {
    pistoris::Cinematic cinematic;
    REQUIRE(cinematic.setResourcePath("cinematic:introduction") == ARX_OK);
    CHECK((cinematic.resourcePath() == "graph/interface/illustrations/introduction.cin"));

    const std::vector<std::uint8_t> image = makeTestBmp();
    const ArxTextureView texture{view("graph/interface/illustrations/introduction"), {image.data(), image.size()}, {}};
    pistoris::TextureIndex texture_index = pistoris::kNoTexture;
    REQUIRE(cinematic.addTexture(texture, texture_index) == ARX_OK);
    CHECK(texture_index == 0);
    pistoris::CinematicIllustrationIndex illustration = pistoris::kInvalidCinematicIllustrationIndex;
    REQUIRE(cinematic.addIllustration({texture_index, 2}, illustration) == ARX_OK);
    CHECK(illustration == 0);
    REQUIRE(cinematic.setTimeline(11, 25.0f) == ARX_OK);

    pistoris::LanguageId english = pistoris::kInvalidLanguageId;
    REQUIRE(cinematic.addLanguage("English", english) == ARX_OK);
    CHECK(english == 1);
    pistoris::SoundHandle unused = pistoris::kNoSoundHandle;
    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "unused", unused) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effects/hit", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line", speech) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(cinematic.setSoundData(effect, pistoris::kSoundEffects, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}) == ARX_OK);

    std::size_t key_index = 99;
    REQUIRE(cinematic.addKeyframe(keyframe(0, effect), key_index) == ARX_OK);
    CHECK(key_index == 0);
    REQUIRE(cinematic.addKeyframe(keyframe(10, speech), key_index) == ARX_OK);
    CHECK(key_index == 1);
    CHECK(cinematic.removeIllustration(illustration) == ARX_CINEMATIC_ILLUSTRATION_IN_USE);
    REQUIRE(cinematic.validate() == ARX_OK);

    std::size_t removed = 0;
    REQUIRE(cinematic.compactSounds(pistoris::SoundKind::kEffect, &removed) == ARX_OK);
    CHECK(removed == 1);
    std::array<ArxCinematicKeyframe, 2> keys{};
    REQUIRE(cinematic.copyKeyframes(0, keys.size(), keys.data()) == ARX_OK);
    pistoris::SoundIndex remapped = pistoris::kNoSound;
    REQUIRE(pistoris::soundHandleIndex(keys[0].sound, remapped) == ARX_OK);
    CHECK(remapped == 0);

    pistoris::NativeCinematicBundle bundle;
    REQUIRE(cinematic.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.cin.bitmaps.size() == 1);
    CHECK(bundle.cin.bitmaps[0].path == "graph/interface/illustrations/introduction");
    REQUIRE(bundle.cin.sounds.size() == 2);
    CHECK(bundle.cin.sounds[0].path == "effects/hit");
    CHECK(bundle.cin.sounds[1].path == "hero/line");
    REQUIRE(bundle.illustration_files.size() == 1);
    CHECK(bundle.illustration_files[0].resource_path == "graph/interface/illustrations/introduction.bmp");
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].path == "sfx/effects/hit.wav");
    CHECK(bundle.sound_files[1].path == "speech/english/hero/line.wav");

    bundle.cin.keyframes[0].bitmap_position = {10.0f, 20.0f, 30.0f};
    bundle.cin.keyframes[0].bitmap_roll = 0.25f;
    pistoris::Cinematic roundtrip;
    REQUIRE(pistoris::Cinematic::importNative(roundtrip, bundle.cin) == ARX_OK);
    CHECK(roundtrip.endFrame() == 11);
    CHECK(roundtrip.keyframeCount() == 2);
    CHECK(roundtrip.soundCount(pistoris::SoundKind::kEffect) == 1);
    CHECK(roundtrip.soundCount(pistoris::SoundKind::kSpeech) == 1);
    CHECK(roundtrip.languageCount() == 0);
    CHECK(roundtrip.validate() == ARX_OK);
    pistoris::cin::Data rebaked;
    REQUIRE(roundtrip.bakeNative(rebaked) == ARX_OK);
    CHECK(rebaked.keyframes[0].bitmap_position.x == 0.0f);
    CHECK(rebaked.keyframes[0].bitmap_position.y == 0.0f);
    CHECK(rebaked.keyframes[0].bitmap_position.z == 0.0f);
    CHECK(rebaked.keyframes[0].bitmap_roll == 0.0f);
  }

  TEST_CASE("Imports an explicit light-off cue with unused native light data") {
    pistoris::Cin native;
    native.bitmaps = {{1, "graph/interface/illustrations/test"}};
    native.end_frame = 10;
    native.keyframes.resize(2);
    native.keyframes[1].frame = 10;
    native.keyframes[0].effects = 1U << 24U;
    native.keyframes[0].light.intensity = -1.0f;
    native.keyframes[0].light.position.x = std::numeric_limits<float>::quiet_NaN();

    pistoris::Cinematic cinematic;
    REQUIRE(pistoris::Cinematic::importNative(cinematic, native) == ARX_OK);
    ArxCinematicKeyframe key;
    REQUIRE(cinematic.copyKeyframes(0, 1, &key) == ARX_OK);
    CHECK(key.light_active == 1);
    CHECK(key.light.intensity == -1.0f);
    CHECK(key.light.position.x == 0.0f);

    pistoris::Cin rebaked;
    REQUIRE(cinematic.bakeNative(rebaked) == ARX_OK);
    CHECK(rebaked.keyframes[0].effects == native.keyframes[0].effects);
    CHECK(rebaked.keyframes[0].light.intensity == -1.0f);
    CHECK(rebaked.keyframes[0].light.position.x == 0.0f);
  }

  TEST_CASE("Enforces language and in-use sound boundaries") {
    pistoris::Cinematic cinematic;
    pistoris::LanguageId english = pistoris::kInvalidLanguageId;
    REQUIRE(cinematic.addLanguage("English", english) == ARX_OK);
    CHECK(cinematic.setLanguage(2, "english") == ARX_CINEMATIC_DUPLICATE_LANGUAGE);
    CHECK(cinematic.setLanguage(2, "con") == ARX_CINEMATIC_BAD_LANGUAGE);

    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effect", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "speech", speech) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    CHECK(cinematic.setSoundData(effect, english, {wav.data(), wav.size()}) == ARX_CINEMATIC_BAD_LANGUAGE);
    CHECK(cinematic.setSoundData(speech, pistoris::kSoundEffects, {wav.data(), wav.size()}) ==
          ARX_CINEMATIC_BAD_LANGUAGE);
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(cinematic.removeLanguage(english) == ARX_OK);
    CHECK(cinematic.soundEncodingCount() == 0);
    CHECK(cinematic.removeSound(effect) == ARX_OK);
  }

  TEST_CASE("Rejects invalid sound handles without retaining stale indices") {
    pistoris::SoundHandle handle = 0;
    CHECK(pistoris::soundHandle(pistoris::SoundKind::kEffect, pistoris::kNoSound, handle) == ARX_INVALID_OPTIONS);
    CHECK(handle == pistoris::kNoSoundHandle);

    pistoris::SoundIndex index = 3;
    CHECK(pistoris::soundHandleIndex(pistoris::kNoSoundHandle, index) == ARX_INVALID_OPTIONS);
    CHECK(index == pistoris::kNoSound);
  }

  TEST_CASE("Checks embedded illustration grids before native baking") {
    pistoris::Cinematic cinematic;
    const std::vector<std::uint8_t> image = makeSolidTestBmp(257, 257);
    pistoris::TextureIndex texture = pistoris::kNoTexture;
    REQUIRE(cinematic.addTexture({view("story/grid"), {image.data(), image.size()}, {}}, texture) == ARX_OK);
    pistoris::CinematicIllustrationIndex illustration = pistoris::kInvalidCinematicIllustrationIndex;
    REQUIRE(cinematic.addIllustration({texture, 100}, illustration) == ARX_OK);
    REQUIRE(cinematic.setTimeline(10, 25.0f) == ARX_OK);
    std::size_t index = 0;
    REQUIRE(cinematic.addKeyframe(keyframe(0, pistoris::kNoSoundHandle), index) == ARX_OK);
    REQUIRE(cinematic.addKeyframe(keyframe(10, pistoris::kNoSoundHandle), index) == ARX_OK);

    pistoris::NativeCinematicBundle bundle;
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    REQUIRE(cinematic.setIllustration(illustration, {texture, 32}) == ARX_OK);
    ArxCinematicKeyframe dream = keyframe(0, pistoris::kNoSoundHandle);
    dream.dream = 1;
    REQUIRE(cinematic.setKeyframe(0, dream) == ARX_OK);
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    dream.dream = 0;
    REQUIRE(cinematic.setKeyframe(0, dream) == ARX_OK);
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_OK);
  }

  TEST_CASE("Checks the next illustration during a Dream crossfade") {
    pistoris::Cinematic cinematic;
    const std::vector<std::uint8_t> small_image = makeSolidTestBmp(1, 1);
    const std::vector<std::uint8_t> large_image = makeSolidTestBmp(257, 257);
    pistoris::TextureIndex small_texture = pistoris::kNoTexture;
    pistoris::TextureIndex large_texture = pistoris::kNoTexture;
    REQUIRE(cinematic.addTexture({view("story/small"), {small_image.data(), small_image.size()}, {}}, small_texture) ==
            ARX_OK);
    REQUIRE(cinematic.addTexture({view("story/large"), {large_image.data(), large_image.size()}, {}}, large_texture) ==
            ARX_OK);
    pistoris::CinematicIllustrationIndex small_illustration = pistoris::kInvalidCinematicIllustrationIndex;
    pistoris::CinematicIllustrationIndex large_illustration = pistoris::kInvalidCinematicIllustrationIndex;
    REQUIRE(cinematic.addIllustration({small_texture, 1}, small_illustration) == ARX_OK);
    REQUIRE(cinematic.addIllustration({large_texture, 32}, large_illustration) == ARX_OK);
    REQUIRE(cinematic.setTimeline(10, 25.0f) == ARX_OK);

    ArxCinematicKeyframe first = keyframe(0, pistoris::kNoSoundHandle);
    first.illustration = small_illustration;
    first.dream = 1;
    first.crossfade = 1;
    ArxCinematicKeyframe last = keyframe(10, pistoris::kNoSoundHandle);
    last.illustration = large_illustration;
    std::size_t index = 0;
    REQUIRE(cinematic.addKeyframe(first, index) == ARX_OK);
    REQUIRE(cinematic.addKeyframe(last, index) == ARX_OK);

    pistoris::NativeCinematicBundle bundle;
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    first.crossfade = 0;
    REQUIRE(cinematic.setKeyframe(0, first) == ARX_OK);
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_OK);

    REQUIRE(cinematic.setIllustration(large_illustration, {large_texture, 1}) == ARX_OK);
    first.crossfade = 1;
    REQUIRE(cinematic.setKeyframe(0, first) == ARX_OK);
    DreamWarningCapture warnings;
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_OK);
    CHECK(warnings.count == 1);

    REQUIRE(cinematic.setIllustration(large_illustration, {small_texture, 1}) == ARX_OK);
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_OK);
    CHECK(warnings.count == 1);
    first.crossfade = 0;
    REQUIRE(cinematic.setKeyframe(0, first) == ARX_OK);
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_OK);
    CHECK(warnings.count == 1);
  }

  TEST_CASE("Bakes BMP sidecars on request and rejects invalid derived timing") {
    pistoris::Cinematic cinematic;
    const std::vector<std::uint8_t> image = makeTestTga();
    pistoris::TextureIndex texture = pistoris::kNoTexture;
    REQUIRE(cinematic.addTexture({view("story/scene"), {image.data(), image.size()}, {}}, texture) == ARX_OK);
    pistoris::CinematicIllustrationIndex illustration = pistoris::kInvalidCinematicIllustrationIndex;
    REQUIRE(cinematic.addIllustration({texture, 1}, illustration) == ARX_OK);
    REQUIRE(cinematic.setTimeline(10, 25.0f) == ARX_OK);
    std::size_t index = 99;
    REQUIRE(cinematic.addKeyframe(keyframe(0, pistoris::kNoSoundHandle), index) == ARX_OK);
    REQUIRE(cinematic.addKeyframe(keyframe(10, pistoris::kNoSoundHandle), index) == ARX_OK);

    pistoris::NativeCinematicBundle bundle;
    REQUIRE(cinematic.bakeNativeBundle({.illustration_format = ARX_IMAGE_FORMAT_BMP}, bundle) == ARX_OK);
    REQUIRE(bundle.illustration_files.size() == 1);
    CHECK(bundle.illustration_files[0].resource_path == "story/scene.bmp");
    CHECK(bundle.illustration_files[0].encoded_image[0] == 'B');
    CHECK(cinematic.bakeNativeBundle({.illustration_format = ARX_IMAGE_FORMAT_PNG}, bundle) == ARX_INVALID_OPTIONS);

    REQUIRE(cinematic.setTimeline(10, 1.0e30f) == ARX_OK);
    ArxCinematicKeyframe fast = keyframe(0, pistoris::kNoSoundHandle);
    fast.outgoing_speed = 1.0e30f;
    REQUIRE(cinematic.setKeyframe(0, fast) == ARX_OK);
    CHECK(cinematic.validate() == ARX_CINEMATIC_BAD_KEY_TIMING);
    CHECK(cinematic.bakeNativeBundle({}, bundle) == ARX_CINEMATIC_BAD_KEY_TIMING);

    std::size_t rejected_index = 123;
    CHECK(cinematic.addKeyframe(fast, rejected_index) == ARX_CINEMATIC_BAD_KEY_FRAME);
    CHECK(rejected_index == std::numeric_limits<std::size_t>::max());
  }
}
