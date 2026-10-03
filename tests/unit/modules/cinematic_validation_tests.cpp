// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"

#include "cinematic/internal.h"
#include "modules/cinematic.h"
#include "modules/sounds.h"
#include "modules/textures.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

pistoris::CinematicData validCinematic() {
  pistoris::CinematicData result;
  result.illustrations.push_back({0, 1});
  result.end_frame = 10;
  result.fps = 25.0f;
  result.keyframes.resize(2);
  result.keyframes[0].illustration = 0;
  result.keyframes[0].outgoing_speed = 1.0f;
  result.keyframes[1].frame = 10;
  result.keyframes[1].illustration = 0;
  return result;
}

}  // namespace

TEST_SUITE("cinematic module validation") {
  TEST_CASE("Module errors map to focused return codes") {
    using pistoris::cinematic_detail::errorCode;
    using pistoris::cinematic_detail::soundError;
    using pistoris::cinematic_detail::textureError;

    CHECK(errorCode(pistoris::cinematic::Error::kNone) == ARX_OK);
    CHECK(errorCode(pistoris::cinematic::Error::kNoIllustrations) == ARX_CINEMATIC_NO_ILLUSTRATIONS);
    CHECK(errorCode(pistoris::cinematic::Error::kTooManyIllustrations) == ARX_CINEMATIC_TOO_MANY_ILLUSTRATIONS);
    CHECK(errorCode(pistoris::cinematic::Error::kBadIllustration) == ARX_CINEMATIC_BAD_ILLUSTRATION);
    CHECK(errorCode(pistoris::cinematic::Error::kBadIllustrationScale) == ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE);
    CHECK(errorCode(pistoris::cinematic::Error::kBadTimeline) == ARX_CINEMATIC_BAD_TIMELINE);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyCount) == ARX_CINEMATIC_BAD_KEY_COUNT);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyFrame) == ARX_CINEMATIC_BAD_KEY_FRAME);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyIllustration) == ARX_CINEMATIC_BAD_KEY_ILLUSTRATION);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeySound) == ARX_CINEMATIC_BAD_KEY_SOUND);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyTransform) == ARX_CINEMATIC_BAD_KEY_TRANSFORM);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyTiming) == ARX_CINEMATIC_BAD_KEY_TIMING);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyInterpolation) == ARX_CINEMATIC_BAD_KEY_INTERPOLATION);
    CHECK(errorCode(pistoris::cinematic::Error::kBadKeyEffect) == ARX_CINEMATIC_BAD_KEY_EFFECT);
    CHECK(errorCode(pistoris::cinematic::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);

    CHECK(textureError(pistoris::textures::Error::kNone) == ARX_OK);
    CHECK(textureError(pistoris::textures::Error::kInvalidOptions) == ARX_INVALID_OPTIONS);
    CHECK(textureError(pistoris::textures::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(textureError(pistoris::textures::Error::kTooManyTextures) == ARX_CINEMATIC_TOO_MANY_TEXTURES);
    CHECK(textureError(pistoris::textures::Error::kBadTexture) == ARX_CINEMATIC_BAD_TEXTURE_PATH);
    CHECK(textureError(pistoris::textures::Error::kDuplicateTexture) == ARX_CINEMATIC_DUPLICATE_TEXTURE_PATH);
    CHECK(textureError(pistoris::textures::Error::kBadImage) == ARX_CINEMATIC_BAD_TEXTURE_IMAGE);
    CHECK(textureError(pistoris::textures::Error::kOutOfMemory) == ARX_BAD_ALLOC);

    CHECK(soundError(pistoris::sounds::Error::kNone) == ARX_OK);
    CHECK(soundError(pistoris::sounds::Error::kInvalidOptions) == ARX_INVALID_OPTIONS);
    CHECK(soundError(pistoris::sounds::Error::kTooManySounds) == ARX_CINEMATIC_TOO_MANY_SOUNDS);
    CHECK(soundError(pistoris::sounds::Error::kBadPath) == ARX_CINEMATIC_BAD_SOUND_PATH);
    CHECK(soundError(pistoris::sounds::Error::kBadAudio) == ARX_CINEMATIC_BAD_SOUND_DATA);
    CHECK(soundError(pistoris::sounds::Error::kUnsupportedChannels) == ARX_CINEMATIC_UNSUPPORTED_SOUND_CHANNELS);
    CHECK(soundError(pistoris::sounds::Error::kAudioTooLarge) == ARX_CINEMATIC_SOUND_TOO_LARGE);
    CHECK(soundError(pistoris::sounds::Error::kDuplicatePath) == ARX_CINEMATIC_DUPLICATE_SOUND_PATH);
    CHECK(soundError(pistoris::sounds::Error::kBadKind) == ARX_INVALID_OPTIONS);
    CHECK(soundError(pistoris::sounds::Error::kBadLanguage) == ARX_CINEMATIC_BAD_LANGUAGE);
    CHECK(soundError(pistoris::sounds::Error::kDuplicateEncoding) == ARX_CINEMATIC_BAD_SOUND_ENCODING);
    CHECK(soundError(pistoris::sounds::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(soundError(pistoris::sounds::Error::kOutOfMemory) == ARX_BAD_ALLOC);
  }

  TEST_CASE("Validates each structural Cinematic boundary") {
    pistoris::SoundsData sounds;
    pistoris::CinematicData data = validCinematic();
    CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kNone);
    CHECK(pistoris::cinematic::validateIllustrationCount(
              static_cast<std::size_t>(pistoris::kInvalidCinematicIllustrationIndex) + 1U) ==
          pistoris::cinematic::Error::kTooManyIllustrations);

    SUBCASE("illustrations") {
      data.illustrations.clear();
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kNoIllustrations);
    }
    SUBCASE("illustration reference") {
      data.illustrations[0].texture = 1;
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadIllustration);
    }
    SUBCASE("illustration scale") {
      data.illustrations[0].subdivision_scale = 0;
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadIllustrationScale);
    }
    SUBCASE("timeline") {
      data.end_frame = 0;
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadTimeline);
    }
    SUBCASE("key count") {
      data.keyframes.pop_back();
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyCount);
    }
    SUBCASE("key frame") {
      data.keyframes[0].frame = 1;
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyFrame);
    }
    SUBCASE("key illustration") {
      data.keyframes[0].illustration = 1;
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyIllustration);
    }
    SUBCASE("key sound") {
      data.keyframes[0].sound = pistoris::sounds::effectHandle(0);
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeySound);
    }
    SUBCASE("key transform") {
      data.keyframes[0].camera_roll = std::numeric_limits<float>::quiet_NaN();
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyTransform);
    }
    SUBCASE("key timing") {
      data.keyframes[0].outgoing_speed = 0.0f;
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyTiming);
    }
    SUBCASE("key interpolation") {
      data.keyframes[0].interpolation = static_cast<pistoris::CinematicInterpolation>(INT8_MAX);
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyInterpolation);
    }
    SUBCASE("key effect") {
      data.keyframes[0].base_effect = static_cast<pistoris::CinematicBaseEffect>(UINT8_MAX);
      CHECK(pistoris::cinematic::validate(data, 1, sounds) == pistoris::cinematic::Error::kBadKeyEffect);
    }
  }
}
