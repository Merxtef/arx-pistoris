// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "audio_helpers.h"
#include "image_helpers.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

std::string_view stringView(ArxStringView value) { return {value.data, value.size}; }

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
  TEST_CASE("Native import locates an invalid CIN keyframe") {
    pistoris::Cin native;
    native.bitmaps = {{1, "graph/interface/illustrations/test"}};
    native.end_frame = 10;
    native.keyframes.resize(2);
    native.keyframes[1].frame = 10;
    native.keyframes[1].bitmap = 1;

    const pistoris::CinResult<pistoris::Cinematic> result = pistoris::Cinematic::importNative(native);

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_CIN_BAD_KEY_BITMAP);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::CinElement::kKeyframe);
    CHECK(result.error()->location()->index == 1);
    CHECK(result.error()->location()->subindex == pistoris::kNoElementIndex);
  }

  TEST_CASE("Owns cinematic resources and bakes canonical native data") {
    pistoris::Cinematic cinematic;
    CHECK(cinematic.setResourcePath("introduction").code() == ARX_CINEMATIC_BAD_RESOURCE_PATH);
    REQUIRE(cinematic.setResourcePath("cinematic:introduction"));
    CHECK((cinematic.resourcePath() == "graph/interface/illustrations/introduction.cin"));

    const std::vector<std::uint8_t> image = makeTestBmp();
    const ArxTextureView texture{view("graph/interface/illustrations/introduction"), {image.data(), image.size()}, {}};
    const auto texture_result = cinematic.addTexture(texture);
    REQUIRE(texture_result);
    const pistoris::TextureIndex texture_index = *texture_result;
    CHECK(texture_index == 0);
    REQUIRE(cinematic.setTexturePath(texture_index, "story/introduction"));
    CHECK(cinematic.setTextureExternalImageExtension(texture_index, ".png").code() == ARX_CINEMATIC_BAD_TEXTURE_IMAGE);
    REQUIRE(cinematic.setTexturePath(texture_index, "graph/interface/illustrations/introduction"));
    const auto illustration_result = cinematic.addIllustration({texture_index, 2});
    REQUIRE(illustration_result);
    const pistoris::CinematicIllustrationIndex illustration = *illustration_result;
    CHECK(illustration == 0);
    REQUIRE(cinematic.setTimeline(11, 25.0f));

    const auto language_result = cinematic.addLanguage("English");
    REQUIRE(language_result);
    const pistoris::LanguageId english = *language_result;
    CHECK(english == 1);
    REQUIRE(cinematic.findLanguage("ENGLISH"));
    CHECK(*cinematic.findLanguage("ENGLISH") == english);
    CHECK((stringView(cinematic.languages()[0].name) == "english"));
    const auto unused_result = cinematic.addSound(pistoris::SoundKind::kEffect, "unused");
    REQUIRE(unused_result);
    const auto effect_result = cinematic.addSound(pistoris::SoundKind::kEffect, "effects/hit");
    REQUIRE(effect_result);
    const auto speech_result = cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line");
    REQUIRE(speech_result);
    const pistoris::SoundHandle effect = *effect_result;
    const pistoris::SoundHandle speech = *speech_result;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(cinematic.setSoundData(effect, pistoris::kSoundEffects, {wav.data(), wav.size()}));
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}));

    const auto first_key = cinematic.addKeyframe(keyframe(0, effect));
    REQUIRE(first_key);
    CHECK(*first_key == 0);
    const auto second_key = cinematic.addKeyframe(keyframe(10, speech));
    REQUIRE(second_key);
    CHECK(*second_key == 1);
    CHECK(cinematic.removeIllustration(illustration).code() == ARX_CINEMATIC_ILLUSTRATION_IN_USE);
    CHECK(cinematic.removeSound(effect).code() == ARX_CINEMATIC_SOUND_IN_USE);
    REQUIRE(cinematic.validate());

    const auto compacted = cinematic.compactSounds(pistoris::SoundKind::kEffect);
    REQUIRE(compacted);
    CHECK(*compacted == 1);
    const auto keys = cinematic.keyframes();
    pistoris::SoundIndex remapped = pistoris::kNoSound;
    REQUIRE(pistoris::soundHandleIndex(keys[0].sound, remapped) == ARX_OK);
    CHECK(remapped == 0);

    auto bundle_result = cinematic.bakeNativeBundle({});
    REQUIRE(bundle_result);
    pistoris::NativeCinematicBundle bundle = std::move(*bundle_result);
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
    auto roundtrip_result = pistoris::Cinematic::importNative(bundle.cin);
    REQUIRE(roundtrip_result);
    pistoris::Cinematic roundtrip = std::move(*roundtrip_result);
    CHECK(roundtrip.endFrame() == 11);
    CHECK(roundtrip.keyframeCount() == 2);
    CHECK(roundtrip.soundCount(pistoris::SoundKind::kEffect) == 1);
    CHECK(roundtrip.soundCount(pistoris::SoundKind::kSpeech) == 1);
    CHECK(roundtrip.languageCount() == 0);
    REQUIRE(roundtrip.validate());
    auto rebaked_result = roundtrip.bakeNative();
    REQUIRE(rebaked_result);
    const pistoris::cin::Data& rebaked = *rebaked_result;
    CHECK(rebaked.keyframes[0].bitmap_position.x == 0.0f);
    CHECK(rebaked.keyframes[0].bitmap_position.y == 0.0f);
    CHECK(rebaked.keyframes[0].bitmap_position.z == 0.0f);
    CHECK(rebaked.keyframes[0].bitmap_roll == 0.0f);
  }

  TEST_CASE("Illustration compaction removes unused images and remaps references") {
    pistoris::Cinematic cinematic;
    const auto unused_texture = cinematic.addTexture({view("story/unused"), {}, view(".png")});
    REQUIRE(unused_texture);
    CHECK(*unused_texture == 0);
    const auto retained_texture = cinematic.addTexture({view("story/retained"), {}, view(".png")});
    REQUIRE(retained_texture);
    CHECK(*retained_texture == 1);
    const auto illustration = cinematic.addIllustration({*retained_texture, 1});
    REQUIRE(illustration);

    const auto compacted = cinematic.compactIllustrations();
    REQUIRE(compacted);
    CHECK(*compacted == 1);
    CHECK(cinematic.textureCount() == 1);
    REQUIRE(cinematic.illustrations().size() == 1);
    CHECK(cinematic.illustrations()[0].texture == 0);
    REQUIRE(cinematic.textures().size() == 1);
    CHECK((stringView(cinematic.textures()[0].path) == "story/retained"));
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

    auto cinematic_result = pistoris::Cinematic::importNative(native);
    REQUIRE(cinematic_result);
    pistoris::Cinematic cinematic = std::move(*cinematic_result);
    const ArxCinematicKeyframe key = cinematic.keyframes()[0];
    CHECK(key.light_active == 1);
    CHECK(key.light.intensity == -1.0f);
    CHECK(key.light.position.x == 0.0f);

    auto rebaked_result = cinematic.bakeNative();
    REQUIRE(rebaked_result);
    const pistoris::Cin& rebaked = *rebaked_result;
    CHECK(rebaked.keyframes[0].effects == native.keyframes[0].effects);
    CHECK(rebaked.keyframes[0].light.intensity == -1.0f);
    CHECK(rebaked.keyframes[0].light.position.x == 0.0f);
  }

  TEST_CASE("Normalizes native Cinematic colors and quantizes them only when baking") {
    pistoris::Cin native;
    native.bitmaps = {{1, "graph/interface/illustrations/test"}};
    native.end_frame = 10;
    native.keyframes.resize(2);
    native.keyframes[1].frame = 10;
    auto& first = native.keyframes[0];
    first.effects = 1U | (1U << 16U) | (1U << 24U);
    first.color = 0xff804020U;
    first.secondary_color = 0xff102040U;
    first.flash_color = 0xff4080ffU;
    first.light.color = {510.0f, 127.5f, 63.75f};
    first.light.intensity = 1.0f;

    auto cinematic_result = pistoris::Cinematic::importNative(native);
    REQUIRE(cinematic_result);
    pistoris::Cinematic cinematic = std::move(*cinematic_result);
    const ArxCinematicKeyframe key = cinematic.keyframes()[0];
    CHECK(key.color.r == doctest::Approx(128.0f / 255.0f));
    CHECK(key.color.g == doctest::Approx(64.0f / 255.0f));
    CHECK(key.color.b == doctest::Approx(32.0f / 255.0f));
    CHECK(key.secondary_color.r == doctest::Approx(16.0f / 255.0f));
    CHECK(key.flash_color.b == doctest::Approx(1.0f));
    CHECK(key.light.color.r == doctest::Approx(2.0f));
    CHECK(key.light.color.g == doctest::Approx(0.5f));
    CHECK(key.light.color.b == doctest::Approx(0.25f));

    ArxCinematicKeyframe invalid = key;
    invalid.color.r = 1.01f;
    CHECK(cinematic.setKeyframe(0, invalid).code() == ARX_CINEMATIC_BAD_KEY_EFFECT);

    auto rebaked_result = cinematic.bakeNative();
    REQUIRE(rebaked_result);
    const auto& rebaked = rebaked_result->keyframes[0];
    CHECK(rebaked.color == first.color);
    CHECK(rebaked.secondary_color == first.secondary_color);
    CHECK(rebaked.flash_color == first.flash_color);
    CHECK(rebaked.light.color.r == doctest::Approx(510.0f));
    CHECK(rebaked.light.color.g == doctest::Approx(127.5f));
    CHECK(rebaked.light.color.b == doctest::Approx(63.75f));
  }

  TEST_CASE("Enforces language and in-use sound boundaries") {
    pistoris::Cinematic cinematic;
    const auto language_result = cinematic.addLanguage("English");
    REQUIRE(language_result);
    const pistoris::LanguageId english = *language_result;
    CHECK(cinematic.setLanguage(2, "english").code() == ARX_CINEMATIC_DUPLICATE_LANGUAGE);
    CHECK(cinematic.setLanguage(2, "con").code() == ARX_CINEMATIC_BAD_LANGUAGE);

    const auto effect_result = cinematic.addSound(pistoris::SoundKind::kEffect, "effect");
    REQUIRE(effect_result);
    const auto speech_result = cinematic.addSound(pistoris::SoundKind::kSpeech, "speech");
    REQUIRE(speech_result);
    const pistoris::SoundHandle effect = *effect_result;
    const pistoris::SoundHandle speech = *speech_result;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    const auto bad_effect_encoding = cinematic.setSoundData(effect, english, {wav.data(), wav.size()});
    CHECK(bad_effect_encoding.code() == ARX_CINEMATIC_BAD_LANGUAGE);
    REQUIRE(bad_effect_encoding.error() != nullptr);
    REQUIRE(bad_effect_encoding.error()->location().has_value());
    CHECK(bad_effect_encoding.error()->location()->element == pistoris::CinematicElement::kSoundEncoding);
    CHECK(bad_effect_encoding.error()->location()->sound_handle == effect);
    CHECK(bad_effect_encoding.error()->location()->language_id == english);
    CHECK(bad_effect_encoding.error()->location()->index == pistoris::kNoElementIndex);
    CHECK(cinematic.setSoundData(speech, pistoris::kSoundEffects, {wav.data(), wav.size()}).code() ==
          ARX_CINEMATIC_BAD_LANGUAGE);
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}));
    REQUIRE(cinematic.removeLanguage(english));
    CHECK(cinematic.soundEncodingCount() == 0);
    REQUIRE(cinematic.removeSound(effect));
  }

  TEST_CASE("Keeps sparse language IDs ordered and reuses the first gap") {
    pistoris::Cinematic cinematic;
    REQUIRE(cinematic.setLanguage(4, "German"));
    REQUIRE(cinematic.setLanguage(2, "French"));

    auto languages = cinematic.languages();
    REQUIRE(languages.size() == 2);
    CHECK(languages[0].id == 2);
    CHECK((stringView(languages[0].name) == "french"));
    CHECK(languages[1].id == 4);
    CHECK((stringView(languages[1].name) == "german"));

    auto added = cinematic.addLanguage("English");
    REQUIRE(added);
    CHECK(*added == 1);
    REQUIRE(cinematic.removeLanguage(2));
    added = cinematic.addLanguage("Italian");
    REQUIRE(added);
    CHECK(*added == 2);

    languages = cinematic.languages();
    REQUIRE(languages.size() == 3);
    CHECK(languages[0].id == 1);
    CHECK(languages[1].id == 2);
    CHECK(languages[2].id == 4);
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
    const auto texture_result = cinematic.addTexture({view("story/grid"), {image.data(), image.size()}, {}});
    REQUIRE(texture_result);
    const auto illustration_result = cinematic.addIllustration({*texture_result, 100});
    REQUIRE(illustration_result);
    const pistoris::CinematicIllustrationIndex illustration = *illustration_result;
    REQUIRE(cinematic.setTimeline(10, 25.0f));
    REQUIRE(cinematic.addKeyframe(keyframe(0, pistoris::kNoSoundHandle)));
    REQUIRE(cinematic.addKeyframe(keyframe(10, pistoris::kNoSoundHandle)));

    CHECK(cinematic.bakeNativeBundle({}).code() == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    REQUIRE(cinematic.setIllustration(illustration, {*texture_result, 32}));
    ArxCinematicKeyframe dream = keyframe(0, pistoris::kNoSoundHandle);
    dream.dream = 1;
    REQUIRE(cinematic.setKeyframe(0, dream));
    CHECK(cinematic.bakeNativeBundle({}).code() == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    dream.dream = 0;
    REQUIRE(cinematic.setKeyframe(0, dream));
    REQUIRE(cinematic.bakeNativeBundle({}));
  }

  TEST_CASE("Checks the next illustration during a Dream crossfade") {
    pistoris::Cinematic cinematic;
    const std::vector<std::uint8_t> small_image = makeSolidTestBmp(1, 1);
    const std::vector<std::uint8_t> large_image = makeSolidTestBmp(257, 257);
    const auto small_texture_result =
        cinematic.addTexture({view("story/small"), {small_image.data(), small_image.size()}, {}});
    REQUIRE(small_texture_result);
    const auto large_texture_result =
        cinematic.addTexture({view("story/large"), {large_image.data(), large_image.size()}, {}});
    REQUIRE(large_texture_result);
    const pistoris::TextureIndex small_texture = *small_texture_result;
    const pistoris::TextureIndex large_texture = *large_texture_result;
    const auto small_illustration_result = cinematic.addIllustration({small_texture, 1});
    REQUIRE(small_illustration_result);
    const auto large_illustration_result = cinematic.addIllustration({large_texture, 32});
    REQUIRE(large_illustration_result);
    const pistoris::CinematicIllustrationIndex small_illustration = *small_illustration_result;
    const pistoris::CinematicIllustrationIndex large_illustration = *large_illustration_result;
    REQUIRE(cinematic.setTimeline(10, 25.0f));

    ArxCinematicKeyframe first = keyframe(0, pistoris::kNoSoundHandle);
    first.illustration = small_illustration;
    first.dream = 1;
    first.crossfade = 1;
    ArxCinematicKeyframe last = keyframe(10, pistoris::kNoSoundHandle);
    last.illustration = large_illustration;
    REQUIRE(cinematic.addKeyframe(first));
    REQUIRE(cinematic.addKeyframe(last));

    CHECK(cinematic.bakeNativeBundle({}).code() == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    first.crossfade = 0;
    REQUIRE(cinematic.setKeyframe(0, first));
    REQUIRE(cinematic.bakeNativeBundle({}));

    REQUIRE(cinematic.setIllustration(large_illustration, {large_texture, 1}));
    first.crossfade = 1;
    REQUIRE(cinematic.setKeyframe(0, first));
    DreamWarningCapture warnings;
    REQUIRE(cinematic.bakeNativeBundle({}));
    CHECK(warnings.count == 1);

    REQUIRE(cinematic.setIllustration(large_illustration, {small_texture, 1}));
    REQUIRE(cinematic.bakeNativeBundle({}));
    CHECK(warnings.count == 1);
    first.crossfade = 0;
    REQUIRE(cinematic.setKeyframe(0, first));
    REQUIRE(cinematic.bakeNativeBundle({}));
    CHECK(warnings.count == 1);
  }

  TEST_CASE("Bakes BMP sidecars on request and rejects invalid derived timing") {
    pistoris::Cinematic cinematic;
    const std::vector<std::uint8_t> image = makeTestTga();
    const auto texture_result = cinematic.addTexture({view("story/scene"), {image.data(), image.size()}, {}});
    REQUIRE(texture_result);
    const auto illustration_result = cinematic.addIllustration({*texture_result, 1});
    REQUIRE(illustration_result);
    REQUIRE(cinematic.setTimeline(10, 25.0f));
    REQUIRE(cinematic.addKeyframe(keyframe(0, pistoris::kNoSoundHandle)));
    REQUIRE(cinematic.addKeyframe(keyframe(10, pistoris::kNoSoundHandle)));

    auto bundle_result = cinematic.bakeNativeBundle({.illustration_format = ARX_IMAGE_FORMAT_BMP});
    REQUIRE(bundle_result);
    const pistoris::NativeCinematicBundle& bundle = *bundle_result;
    REQUIRE(bundle.illustration_files.size() == 1);
    CHECK(bundle.illustration_files[0].resource_path == "story/scene.bmp");
    CHECK(bundle.illustration_files[0].encoded_image[0] == 'B');
    CHECK(cinematic.bakeNativeBundle({.illustration_format = ARX_IMAGE_FORMAT_PNG}).code() == ARX_INVALID_OPTIONS);

    REQUIRE(cinematic.setTimeline(10, 1.0e30f));
    ArxCinematicKeyframe fast = keyframe(0, pistoris::kNoSoundHandle);
    fast.outgoing_speed = 1.0e30f;
    REQUIRE(cinematic.setKeyframe(0, fast));
    CHECK(cinematic.validate().code() == ARX_CINEMATIC_BAD_KEY_TIMING);
    CHECK(cinematic.bakeNativeBundle({}).code() == ARX_CINEMATIC_BAD_KEY_TIMING);

    CHECK(cinematic.addKeyframe(fast).code() == ARX_CINEMATIC_BAD_KEY_FRAME);
  }
}
