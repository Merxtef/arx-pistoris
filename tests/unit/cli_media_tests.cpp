// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"

#include "audio_helpers.h"
#include "image_helpers.h"
#include "media/encoded.h"

#include <array>
#include <cstdint>
#include <ostream>  // IWYU pragma: keep
#include <vector>

TEST_SUITE("CLI prepared media") {
  TEST_CASE("Prepared media validates bytes and retains metadata") {
    const std::vector<std::uint8_t> image = makeTestBmp();
    cli::media::PreparedImage prepared_image;
    REQUIRE(cli::media::prepareImage(image, prepared_image) == ARX_OK);
    CHECK(prepared_image.encoded == image);
    CHECK(prepared_image.info.format == ARX_IMAGE_FORMAT_BMP);
    CHECK(cli::media::imageExtension(prepared_image.info.format) == ".bmp");

    const std::vector<std::uint8_t> audio = makePcm16Wav(2);
    cli::media::PreparedAudio prepared_audio;
    REQUIRE(cli::media::prepareAudio(audio, prepared_audio) == ARX_OK);
    CHECK(prepared_audio.encoded == audio);
    CHECK(prepared_audio.info.format == ARX_AUDIO_FORMAT_WAV);
    CHECK(prepared_audio.info.channels == 2);
    CHECK(cli::media::audioExtension(prepared_audio.info.format) == ".wav");
  }

  TEST_CASE("Prepared media does not overwrite output on failure") {
    cli::media::PreparedImage image{.encoded = {42}};
    cli::media::PreparedAudio audio{.encoded = {42}};
    const std::array<std::uint8_t, 4> malformed{};
    CHECK(cli::media::prepareImage(std::vector<std::uint8_t>(malformed.begin(), malformed.end()), image) ==
          ARX_IMAGE_BAD_DATA);
    CHECK(cli::media::prepareAudio(std::vector<std::uint8_t>(malformed.begin(), malformed.end()), audio) ==
          ARX_AUDIO_BAD_DATA);
    CHECK(image.encoded == std::vector<std::uint8_t>{42});
    CHECK(audio.encoded == std::vector<std::uint8_t>{42});
  }
}
