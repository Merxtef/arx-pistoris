// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.h"

#include "audio_helpers.h"
#include "image_helpers.h"

#include <array>
#include <cstdint>
#include <vector>

TEST_SUITE("C binary validation API") {
  TEST_CASE("Validates encoded audio") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    CHECK(arx_pistoris_binary_validate_encoded_audio({wav.data(), wav.size()}) == ARX_OK);
    ArxAudioInfo info{};
    REQUIRE(arx_pistoris_binary_inspect_encoded_audio({wav.data(), wav.size()}, &info) == ARX_OK);
    CHECK(info.format == ARX_AUDIO_FORMAT_WAV);
    CHECK(info.channels == 2);
    CHECK(info.sample_rate != 0);
    CHECK(info.frame_count != 0);
    CHECK(arx_pistoris_binary_inspect_encoded_audio({wav.data(), wav.size()}, nullptr) == ARX_INVALID_DATA_POINTER);

    const std::array<std::uint8_t, 4> malformed{};
    CHECK(arx_pistoris_binary_validate_encoded_audio({malformed.data(), malformed.size()}) == ARX_AUDIO_BAD_DATA);
    CHECK(arx_pistoris_binary_validate_encoded_audio({nullptr, 0}) == ARX_AUDIO_BAD_DATA);
    CHECK(arx_pistoris_binary_validate_encoded_audio({nullptr, 1}) == ARX_INVALID_DATA_POINTER);

    const std::vector<std::uint8_t> multichannel = makePcm16Wav(3);
    CHECK(arx_pistoris_binary_validate_encoded_audio({multichannel.data(), multichannel.size()}) ==
          ARX_AUDIO_UNSUPPORTED_CHANNELS);
  }

  TEST_CASE("Validates encoded images") {
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    CHECK(arx_pistoris_binary_validate_encoded_image({bmp.data(), bmp.size()}) == ARX_OK);
    ArxImageInfo info{};
    REQUIRE(arx_pistoris_binary_inspect_encoded_image({bmp.data(), bmp.size()}, &info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_BMP);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(info.components == 3);
    CHECK(arx_pistoris_binary_inspect_encoded_image({bmp.data(), bmp.size()}, nullptr) == ARX_INVALID_DATA_POINTER);

    const std::array<std::uint8_t, 4> malformed{};
    CHECK(arx_pistoris_binary_validate_encoded_image({malformed.data(), malformed.size()}) == ARX_IMAGE_BAD_DATA);
    CHECK(arx_pistoris_binary_validate_encoded_image({nullptr, 0}) == ARX_IMAGE_BAD_DATA);
    CHECK(arx_pistoris_binary_validate_encoded_image({nullptr, 1}) == ARX_INVALID_DATA_POINTER);
  }
}
