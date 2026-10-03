// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/glb.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "audio_helpers.h"
#include "image_helpers.h"

#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

void addIllustration(pistoris::Cinematic& cinematic) {
  const std::vector<std::uint8_t> image = makeSolidTestBmp(64, 32, 20, 40, 60);
  const ArxTextureView texture{view("story/scene"), {image.data(), image.size()}, {}};
  const auto texture_result = cinematic.addTexture(texture);
  REQUIRE(texture_result);
  const auto illustration_result = cinematic.addIllustration({*texture_result, 2});
  REQUIRE(illustration_result);
  REQUIRE(*illustration_result == 0);
}

ArxCinematicKeyframe firstKey(pistoris::SoundHandle sound) {
  ArxCinematicKeyframe key;
  key.frame = 0;
  key.illustration = 0;
  key.camera_position = {-12.0f, 7.0f, 50.0f};
  key.camera_roll = 0.25f;
  key.color = {1.0f / 255.0f, 2.0f / 255.0f, 3.0f / 255.0f};
  key.secondary_color = {4.0f / 255.0f, 5.0f / 255.0f, 6.0f / 255.0f};
  key.flash_color = {7.0f / 255.0f, 8.0f / 255.0f, 9.0f / 255.0f};
  key.flash_decay = 0.75f;
  key.light.position = {10.0f, 4.0f, 0.0f};
  key.light.fall_in = 2.0f;
  key.light.fall_out = 8.0f;
  key.light.color = {0.1f, 0.2f, 0.3f};
  key.light.intensity = 0.8f;
  key.light.random_intensity = 0.1f;
  key.outgoing_speed = 1.25f;
  key.sound = sound;
  key.interpolation = ARX_CINEMATIC_INTERPOLATION_BEZIER;
  key.base_effect = ARX_CINEMATIC_BASE_EFFECT_FADE_IN;
  key.post_effect = ARX_CINEMATIC_POST_EFFECT_FLASH;
  key.crossfade = 1;
  key.dream = 1;
  key.light_active = 1;
  return key;
}

ArxCinematicKeyframe lastKey(pistoris::SoundHandle sound) {
  ArxCinematicKeyframe key;
  key.frame = 12;
  key.illustration = 0;
  key.camera_position = {16.0f, -8.0f, 75.0f};
  key.sound = sound;
  return key;
}

void checkNear(float actual, float expected) { CHECK(std::abs(actual - expected) <= 1.0e-4f); }

}  // namespace

TEST_SUITE("C++ Cinematic GLB API") {
  TEST_CASE("Preserves opaque black in BMP illustrations") {
    pistoris::Cinematic cinematic;
    const std::vector<std::uint8_t> image = makeTestBmp(0, 0, 0);
    const auto texture_result = cinematic.addTexture({view("story/black"), {image.data(), image.size()}, {}});
    REQUIRE(texture_result);
    REQUIRE(cinematic.addIllustration({*texture_result, 1}));
    REQUIRE(cinematic.setTimeline(12, 25.0f));
    REQUIRE(cinematic.addKeyframe(firstKey(pistoris::kNoSoundHandle)));
    REQUIRE(cinematic.addKeyframe(lastKey(pistoris::kNoSoundHandle)));

    auto glb_result = cinematic.exportGlb();
    REQUIRE(glb_result);
    auto imported_result = pistoris::Cinematic::importGlb(*glb_result);
    REQUIRE(imported_result);
    pistoris::Cinematic imported = std::move(*imported_result);
    const ArxTextureView texture = imported.textures()[0];
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage({texture.encoded_image.data, texture.encoded_image.size}, info) ==
            ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.components == 3);
  }

  TEST_CASE("Roundtrips the timeline and emits referenced audio sidecars") {
    pistoris::Cinematic cinematic;
    addIllustration(cinematic);
    const auto second_illustration_result = cinematic.addIllustration({0, 3});
    REQUIRE(second_illustration_result);
    REQUIRE(*second_illustration_result == 1);
    REQUIRE(cinematic.setTimeline(15, 30.0f));

    const auto effect_result = cinematic.addSound(pistoris::SoundKind::kEffect, "effects/my__hit");
    REQUIRE(effect_result);
    const auto speech_result = cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line.v2");
    REQUIRE(speech_result);
    const pistoris::SoundHandle effect = *effect_result;
    const pistoris::SoundHandle speech = *speech_result;
    const auto language_result = cinematic.addLanguage("English");
    REQUIRE(language_result);
    const pistoris::LanguageId english = *language_result;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(cinematic.setSoundData(effect, pistoris::kSoundEffects, {wav.data(), wav.size()}));
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}));

    REQUIRE(cinematic.addKeyframe(firstKey(effect)));
    REQUIRE(cinematic.addKeyframe(lastKey(speech)));
    REQUIRE(cinematic.validate());

    auto bundle_result = cinematic.exportGlbBundle();
    REQUIRE(bundle_result);
    pistoris::CinematicGlbBundle bundle = std::move(*bundle_result);
    CHECK_FALSE(bundle.glb.empty());
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].path == "effects/my__hit.wav");
    CHECK(bundle.sound_files[1].path == "hero/line.v2[english].wav");

    std::vector<pistoris::CinematicSoundSourceReference> sources;
    auto imported_result = pistoris::Cinematic::importGlb(bundle.glb, &sources);
    REQUIRE(imported_result);
    pistoris::Cinematic imported = std::move(*imported_result);
    CHECK(imported.endFrame() == 15);
    checkNear(imported.fps(), 30.0f);
    CHECK(imported.illustrationCount() == 2);
    CHECK(imported.textureCount() == 1);
    CHECK(imported.keyframeCount() == 2);
    CHECK(imported.soundCount(pistoris::SoundKind::kEffect) == 1);
    CHECK(imported.soundCount(pistoris::SoundKind::kSpeech) == 1);
    CHECK(imported.soundEncodingCount() == 0);
    REQUIRE(sources.size() == 2);
    CHECK(sources[0].path == "effects/my__hit");
    CHECK(sources[1].path == "hero/line.v2");

    const auto illustrations = imported.illustrations();
    CHECK(illustrations[0].subdivision_scale == 2);
    CHECK(illustrations[1].subdivision_scale == 3);
    CHECK(illustrations[0].texture == illustrations[1].texture);
    const ArxTextureView texture = imported.textures()[0];
    CHECK((std::string_view(texture.path.data, texture.path.size) == "story/scene"));
    CHECK(texture.encoded_image.size != 0);

    const auto keys = imported.keyframes();
    CHECK(keys[0].frame == 0);
    checkNear(keys[0].camera_position.x, -12.0f);
    checkNear(keys[0].camera_position.y, 7.0f);
    checkNear(keys[0].camera_position.z, 50.0f);
    checkNear(keys[0].camera_roll, 0.25f);
    CHECK(keys[0].base_effect == ARX_CINEMATIC_BASE_EFFECT_FADE_IN);
    CHECK(keys[0].post_effect == ARX_CINEMATIC_POST_EFFECT_FLASH);
    CHECK(keys[0].light_active == 1);
    checkNear(keys[0].light.position.x, 10.0f);
    checkNear(keys[0].light.position.y, 4.0f);
    checkNear(keys[0].light.position.z, 0.0f);
    CHECK(keys[1].frame == 12);
    REQUIRE(imported.validate());
  }

  TEST_CASE("Rejects colliding effect and localized speech sidecars transactionally") {
    pistoris::Cinematic cinematic;
    addIllustration(cinematic);
    REQUIRE(cinematic.setTimeline(12, 25.0f));

    const auto effect_result = cinematic.addSound(pistoris::SoundKind::kEffect, "hero/line[English]");
    REQUIRE(effect_result);
    const auto speech_result = cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line");
    REQUIRE(speech_result);
    const pistoris::SoundHandle effect = *effect_result;
    const pistoris::SoundHandle speech = *speech_result;
    const auto language_result = cinematic.addLanguage("English");
    REQUIRE(language_result);
    const pistoris::LanguageId english = *language_result;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(cinematic.setSoundData(effect, pistoris::kSoundEffects, {wav.data(), wav.size()}));
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}));

    REQUIRE(cinematic.addKeyframe(firstKey(effect)));
    REQUIRE(cinematic.addKeyframe(lastKey(speech)));

    CHECK(cinematic.exportGlbBundle().code() == ARX_CINEMATIC_BAD_SOUND_ENCODING);
  }
}
