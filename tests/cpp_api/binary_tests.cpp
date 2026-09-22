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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE("C++ binary validation API") {
  TEST_CASE("Classifies and converts text encodings") {
    using pistoris::binary::TextEncoding;

    CHECK(pistoris::binary::classifyTextEncoding({}) == TextEncoding::kAscii);
    CHECK(pistoris::binary::classifyTextEncoding(std::string_view("a\0b", 3)) == TextEncoding::kAscii);
    CHECK(pistoris::binary::classifyTextEncoding("caf\xc3\xa9") == TextEncoding::kUtf8);

    std::string latin1 = "caf";
    latin1.push_back(static_cast<char>(0xe9));
    CHECK(pistoris::binary::classifyTextEncoding(latin1) == TextEncoding::kLatin1);
    CHECK(pistoris::binary::classifyTextEncoding("\xc3\xa9") == TextEncoding::kUtf8);

    std::string converted;
    REQUIRE(pistoris::binary::latin1ToUtf8(latin1, converted) == ARX_OK);
    CHECK(converted == "caf\xc3\xa9");
    REQUIRE(pistoris::binary::utf8ToLatin1(converted, converted) == ARX_OK);
    CHECK(converted == latin1);

    std::string all_latin1(256, '\0');
    for (std::size_t index = 0; index < all_latin1.size(); ++index) all_latin1[index] = static_cast<char>(index);
    REQUIRE(pistoris::binary::latin1ToUtf8(all_latin1, converted) == ARX_OK);
    std::string roundtrip;
    REQUIRE(pistoris::binary::utf8ToLatin1(converted, roundtrip) == ARX_OK);
    CHECK(roundtrip == all_latin1);

    converted = "unchanged";
    CHECK(pistoris::binary::utf8ToLatin1("\xc3", converted) == ARX_TEXT_INVALID_UTF8);
    CHECK(converted == "unchanged");
    CHECK(pistoris::binary::utf8ToLatin1("\xe2\x82\xac", converted) == ARX_TEXT_NOT_LATIN1);
    CHECK(converted == "unchanged");
  }

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
