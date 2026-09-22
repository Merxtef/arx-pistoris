// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/buffer.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.h"

#include "audio_helpers.h"
#include "image_helpers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE("C binary validation API") {
  TEST_CASE("C API classifies and converts text encodings") {
    ArxTextEncoding encoding = ARX_TEXT_ENCODING_LATIN1;
    CHECK(arx_pistoris_binary_classify_text_encoding({nullptr, 0}, &encoding) == ARX_OK);
    CHECK(encoding == ARX_TEXT_ENCODING_ASCII);

    const std::string utf8 = "caf\xc3\xa9";
    REQUIRE(arx_pistoris_binary_classify_text_encoding({utf8.data(), utf8.size()}, &encoding) == ARX_OK);
    CHECK(encoding == ARX_TEXT_ENCODING_UTF8);

    std::string latin1 = "caf";
    latin1.push_back(static_cast<char>(0xe9));
    REQUIRE(arx_pistoris_binary_classify_text_encoding({latin1.data(), latin1.size()}, &encoding) == ARX_OK);
    CHECK(encoding == ARX_TEXT_ENCODING_LATIN1);
    CHECK(arx_pistoris_binary_classify_text_encoding({nullptr, 1}, &encoding) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_binary_classify_text_encoding({}, nullptr) == ARX_INVALID_DATA_POINTER);

    char* converted = nullptr;
    std::size_t converted_size = 0;
    REQUIRE(arx_pistoris_binary_latin1_to_utf8({latin1.data(), latin1.size()}, &converted, &converted_size) == ARX_OK);
    REQUIRE(converted != nullptr);
    CHECK((std::string_view(converted, converted_size) == utf8));
    CHECK(converted[converted_size] == '\0');
    arx_pistoris_free_string(converted);

    converted = nullptr;
    converted_size = 0;
    REQUIRE(arx_pistoris_binary_utf8_to_latin1({utf8.data(), utf8.size()}, &converted, &converted_size) == ARX_OK);
    REQUIRE(converted != nullptr);
    CHECK((std::string_view(converted, converted_size) == latin1));
    CHECK(converted[converted_size] == '\0');
    arx_pistoris_free_string(converted);

    converted = nullptr;
    converted_size = 42;
    CHECK(arx_pistoris_binary_utf8_to_latin1({"\xc3", 1}, &converted, &converted_size) == ARX_TEXT_INVALID_UTF8);
    CHECK(converted == nullptr);
    CHECK(converted_size == 0);
    CHECK(arx_pistoris_binary_latin1_to_utf8({}, nullptr, &converted_size) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_binary_latin1_to_utf8({}, &converted, nullptr) == ARX_INVALID_DATA_POINTER);
  }

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
