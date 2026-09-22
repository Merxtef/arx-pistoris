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
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

void addIllustration(pistoris::Cinematic& cinematic) {
  const std::vector<std::uint8_t> image = makeSolidTestBmp(64, 32, 20, 40, 60);
  const ArxTextureView texture{view("story/scene"), {image.data(), image.size()}, {}};
  pistoris::TextureIndex texture_index = pistoris::kNoTexture;
  REQUIRE(cinematic.addTexture(texture, texture_index) == ARX_OK);
  pistoris::CinematicIllustrationIndex illustration = pistoris::kInvalidCinematicIllustrationIndex;
  REQUIRE(cinematic.addIllustration({texture_index, 2}, illustration) == ARX_OK);
  REQUIRE(illustration == 0);
}

ArxCinematicKeyframe firstKey(pistoris::SoundHandle sound) {
  ArxCinematicKeyframe key;
  key.frame = 0;
  key.illustration = 0;
  key.camera_position = {-12.0f, 7.0f, 50.0f};
  key.camera_roll = 0.25f;
  key.color = {1, 2, 3};
  key.secondary_color = {4, 5, 6};
  key.flash_color = {7, 8, 9};
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
    pistoris::TextureIndex texture_index = pistoris::kNoTexture;
    REQUIRE(cinematic.addTexture({view("story/black"), {image.data(), image.size()}, {}}, texture_index) == ARX_OK);
    pistoris::CinematicIllustrationIndex illustration = pistoris::kInvalidCinematicIllustrationIndex;
    REQUIRE(cinematic.addIllustration({texture_index, 1}, illustration) == ARX_OK);
    REQUIRE(cinematic.setTimeline(12, 25.0f) == ARX_OK);
    std::size_t index = 0;
    REQUIRE(cinematic.addKeyframe(firstKey(pistoris::kNoSoundHandle), index) == ARX_OK);
    REQUIRE(cinematic.addKeyframe(lastKey(pistoris::kNoSoundHandle), index) == ARX_OK);

    std::vector<std::uint8_t> glb;
    REQUIRE(cinematic.exportGlb(glb) == ARX_OK);
    pistoris::Cinematic imported;
    REQUIRE(pistoris::Cinematic::importGlb(imported, glb) == ARX_OK);
    ArxTextureView texture{};
    REQUIRE(imported.copyTextureViews(0, 1, &texture) == ARX_OK);
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage({texture.encoded_image.data, texture.encoded_image.size}, info) ==
            ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.components == 3);
  }

  TEST_CASE("Roundtrips the timeline and emits referenced audio sidecars") {
    pistoris::Cinematic cinematic;
    addIllustration(cinematic);
    pistoris::CinematicIllustrationIndex second_illustration = pistoris::kInvalidCinematicIllustrationIndex;
    REQUIRE(cinematic.addIllustration({0, 3}, second_illustration) == ARX_OK);
    REQUIRE(second_illustration == 1);
    REQUIRE(cinematic.setTimeline(15, 30.0f) == ARX_OK);

    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effects/my__hit", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line.v2", speech) == ARX_OK);
    pistoris::LanguageId english = pistoris::kInvalidLanguageId;
    REQUIRE(cinematic.addLanguage("English", english) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(cinematic.setSoundData(effect, pistoris::kSoundEffects, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}) == ARX_OK);

    std::size_t index = 0;
    REQUIRE(cinematic.addKeyframe(firstKey(effect), index) == ARX_OK);
    REQUIRE(cinematic.addKeyframe(lastKey(speech), index) == ARX_OK);
    REQUIRE(cinematic.validate() == ARX_OK);

    pistoris::CinematicGlbBundle bundle;
    REQUIRE(cinematic.exportGlbBundle(bundle) == ARX_OK);
    CHECK_FALSE(bundle.glb.empty());
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].path == "effects/my__hit.wav");
    CHECK(bundle.sound_files[1].path == "hero/line.v2[English].wav");

    pistoris::Cinematic imported;
    std::vector<pistoris::CinematicSoundSourceReference> sources;
    REQUIRE(pistoris::Cinematic::importGlb(imported, bundle.glb, &sources) == ARX_OK);
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

    ArxCinematicIllustration illustrations[2]{};
    REQUIRE(imported.copyIllustrations(0, 2, illustrations) == ARX_OK);
    CHECK(illustrations[0].subdivision_scale == 2);
    CHECK(illustrations[1].subdivision_scale == 3);
    CHECK(illustrations[0].texture == illustrations[1].texture);
    ArxTextureView texture{};
    REQUIRE(imported.copyTextureViews(0, 1, &texture) == ARX_OK);
    CHECK((std::string_view(texture.path.data, texture.path.size) == "story/scene"));
    CHECK(texture.encoded_image.size != 0);

    ArxCinematicKeyframe keys[2]{};
    REQUIRE(imported.copyKeyframes(0, 2, keys) == ARX_OK);
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
    CHECK(imported.validate() == ARX_OK);
  }

  TEST_CASE("Rejects colliding effect and localized speech sidecars transactionally") {
    pistoris::Cinematic cinematic;
    addIllustration(cinematic);
    REQUIRE(cinematic.setTimeline(12, 25.0f) == ARX_OK);

    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "hero/line[English]", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line", speech) == ARX_OK);
    pistoris::LanguageId english = pistoris::kInvalidLanguageId;
    REQUIRE(cinematic.addLanguage("English", english) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(cinematic.setSoundData(effect, pistoris::kSoundEffects, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(cinematic.setSoundData(speech, english, {wav.data(), wav.size()}) == ARX_OK);

    std::size_t index = 0;
    REQUIRE(cinematic.addKeyframe(firstKey(effect), index) == ARX_OK);
    REQUIRE(cinematic.addKeyframe(lastKey(speech), index) == ARX_OK);

    pistoris::CinematicGlbBundle bundle;
    bundle.glb = {0x7f};
    CHECK(cinematic.exportGlbBundle(bundle) == ARX_CINEMATIC_BAD_SOUND_ENCODING);
    CHECK(bundle.glb == std::vector<std::uint8_t>{0x7f});
    CHECK(bundle.sound_files.empty());
  }
}
