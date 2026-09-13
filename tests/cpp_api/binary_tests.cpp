// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"

#include "audio_helpers.h"
#include "image_helpers.h"

#include <array>
#include <cstdint>
#include <vector>

TEST_SUITE("C++ binary validation API") {
  TEST_CASE("Validates encoded audio") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    CHECK(pistoris::binary::validateEncodedAudio(wav) == ARX_OK);
    ArxAudioInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedAudio(wav, info) == ARX_OK);
    CHECK(info.format == ARX_AUDIO_FORMAT_WAV);
    CHECK(info.channels == 2);
    CHECK(info.sample_rate != 0);
    CHECK(info.frame_count != 0);

    const std::array<std::uint8_t, 4> malformed{};
    CHECK(pistoris::binary::validateEncodedAudio(malformed) == ARX_AUDIO_BAD_DATA);
    CHECK(pistoris::binary::validateEncodedAudio({}) == ARX_AUDIO_BAD_DATA);

    const std::vector<std::uint8_t> multichannel = makePcm16Wav(3);
    CHECK(pistoris::binary::validateEncodedAudio(multichannel) == ARX_AUDIO_UNSUPPORTED_CHANNELS);
  }

  TEST_CASE("Validates encoded images") {
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    CHECK(pistoris::binary::validateEncodedImage(bmp) == ARX_OK);
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(bmp, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_BMP);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(info.components == 3);

    const std::array<std::uint8_t, 4> malformed{};
    CHECK(pistoris::binary::validateEncodedImage(malformed) == ARX_IMAGE_BAD_DATA);
    CHECK(pistoris::binary::validateEncodedImage({}) == ARX_IMAGE_BAD_DATA);
  }
}
