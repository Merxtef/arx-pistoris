// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "image_helpers.h"
#include "modules/textures.h"
#include "stb/stb_image.h"
#include "utils/encoded_image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> makeColorKeyBmp() {
  std::vector<std::uint8_t> bmp = makeTestBmp(0, 0, 0);
  bmp.resize(62, 0);
  bmp[2] = 62;
  bmp[18] = 2;
  bmp[34] = 8;
  bmp[59] = 255;
  return bmp;
}

std::vector<std::uint8_t> makeAntialiasedColorKeyBmp() {
  constexpr std::size_t kRowBytes = 12;
  std::vector<std::uint8_t> bmp = makeSolidTestBmp(3, 3, 0, 0, 0);
  const auto set_red = [&](std::size_t x, std::size_t y) {
    const std::size_t offset = 54U + y * kRowBytes + x * 3U;
    bmp[offset + 2] = 255;
  };
  set_red(2, 0);
  set_red(1, 1);
  set_red(2, 1);
  set_red(1, 2);
  set_red(2, 2);
  return bmp;
}

}  // namespace

TEST_SUITE("encoded images") {
  TEST_CASE("Detects encoded format without decoding") {
    CHECK(pistoris::image::detectFormat({}) == pistoris::image::Format::kUnknown);
    CHECK(pistoris::image::detectFormat(std::vector<std::uint8_t>{1, 2, 3}) == pistoris::image::Format::kUnknown);
    CHECK(pistoris::image::detectFormat(makeTestBmp()) == pistoris::image::Format::kBmp);
    CHECK(pistoris::image::detectFormat(makeTestTga()) == pistoris::image::Format::kTga);
    CHECK(std::string(pistoris::image::extension(pistoris::image::Format::kJpeg)) == ".jpg");
    CHECK(std::string(pistoris::image::extension(pistoris::image::Format::kPng)) == ".png");
    CHECK(std::string(pistoris::image::extension(pistoris::image::Format::kBmp)) == ".bmp");
    CHECK(std::string(pistoris::image::extension(pistoris::image::Format::kTga)) == ".tga");
    CHECK(pistoris::image::extension(pistoris::image::Format::kUnknown).empty());
  }

  TEST_CASE("Inspects supported lossless source formats") {
    pistoris::image::Info info;
    CHECK(pistoris::image::inspect(makeTestBmp(), &info) == pistoris::image::Error::kNone);
    CHECK(info.format == pistoris::image::Format::kBmp);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(info.components == 3);

    CHECK(pistoris::image::inspect(makeTestTga(), &info) == pistoris::image::Error::kNone);
    CHECK(info.format == pistoris::image::Format::kTga);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(info.components == 3);
  }

  TEST_CASE("Converts BMP and TGA to valid PNG") {
    for (const std::vector<std::uint8_t>& source : {makeTestBmp(), makeTestTga()}) {
      pistoris::TexturesData textures{{pistoris::Texture{"texture"}}};
      textures.textures[0].encoded_image = source;
      const pistoris::textures::ImagePreparationRequest request{
          0, {pistoris::image::formatFlag(pistoris::image::Format::kPng)}};
      std::vector<pistoris::textures::PreparedImage> output;
      REQUIRE(pistoris::textures::prepareImages(textures, {&request, 1}, output) == pistoris::textures::Error::kNone);
      REQUIRE(output.size() == 1);
      const pistoris::textures::PreparedImage& prepared = output[0];
      CHECK(prepared.info.format == pistoris::image::Format::kPng);
      CHECK_FALSE(pistoris::image::hasAlpha(prepared.info));

      pistoris::image::Info info;
      REQUIRE(pistoris::image::inspect(prepared.bytes.data(), &info) == pistoris::image::Error::kNone);
      CHECK(info.format == pistoris::image::Format::kPng);
      CHECK(info.width == 1);
      CHECK(info.height == 1);

      pistoris::TexturesData prepared_textures{{pistoris::Texture{"texture"}}};
      prepared_textures.textures[0].encoded_image.assign(prepared.bytes.data().begin(), prepared.bytes.data().end());
      std::vector<pistoris::textures::PreparedImage> pass_through;
      REQUIRE(pistoris::textures::prepareImages(prepared_textures, {&request, 1}, pass_through) ==
              pistoris::textures::Error::kNone);
      REQUIRE(pass_through.size() == 1);
      CHECK(pass_through[0].info.format == pistoris::image::Format::kPng);
      CHECK(std::ranges::equal(pass_through[0].bytes.data(), prepared.bytes.data()));
      CHECK(pass_through[0].bytes.converted.empty());
    }
  }

  TEST_CASE("Prepares repeated texture requests in request order") {
    pistoris::TexturesData textures{{pistoris::Texture{"texture"}}};
    textures.textures[0].encoded_image = makeTestBmp();
    const std::vector<pistoris::textures::ImagePreparationRequest> requests = {
        {0,
         {.accepted_formats = pistoris::image::formatFlag(pistoris::image::Format::kPng),
          .fallback_format = pistoris::image::Format::kPng}},
        {0, {.accepted_formats = pistoris::image::kFormatsAll, .fallback_format = pistoris::image::Format::kPng}},
    };

    std::vector<pistoris::textures::PreparedImage> prepared;
    REQUIRE(pistoris::textures::prepareImages(textures, requests, prepared) == pistoris::textures::Error::kNone);
    REQUIRE(prepared.size() == requests.size());
    CHECK(prepared[0].info.format == pistoris::image::Format::kPng);
    CHECK_FALSE(prepared[0].bytes.converted.empty());
    CHECK(prepared[1].info.format == pistoris::image::Format::kBmp);
    CHECK(prepared[1].bytes.converted.empty());
    CHECK(prepared[1].bytes.borrowed.data() == textures.textures[0].encoded_image.data());
  }

  TEST_CASE("Generic BMP transcoding preserves opaque black") {
    pistoris::image::Info info;
    std::vector<std::uint8_t> png;
    REQUIRE(pistoris::image::transcodeToPng(makeColorKeyBmp(), png, &info) == pistoris::image::Error::kNone);
    CHECK(info.components == 3);

    int width = 0;
    int height = 0;
    int components = 0;
    stbi_uc* pixels = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &width, &height, &components, 4);
    REQUIRE(pixels != nullptr);
    CHECK(std::array{pixels[0], pixels[1], pixels[2], pixels[3]} == std::array<std::uint8_t, 4>{0, 0, 0, 255});
    stbi_image_free(pixels);
  }

  TEST_CASE("Converts BMP black color key to alpha on request") {
    pistoris::image::Info info;
    std::vector<std::uint8_t> png;
    REQUIRE(pistoris::image::transcodeToPng(makeColorKeyBmp(), png, &info, pistoris::image::BmpColorKey::kBinary) ==
            pistoris::image::Error::kNone);
    CHECK(info.format == pistoris::image::Format::kPng);
    CHECK(info.components == 4);
    CHECK(pistoris::image::hasAlpha(info));

    int width = 0;
    int height = 0;
    int components = 0;
    stbi_uc* pixels = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &width, &height, &components, 4);
    REQUIRE(pixels != nullptr);
    CHECK(width == 2);
    CHECK(height == 1);
    CHECK(components == 4);
    CHECK(std::array{pixels[0], pixels[1], pixels[2], pixels[3]} == std::array<std::uint8_t, 4>{255, 0, 0, 0});
    CHECK(std::array{pixels[4], pixels[5], pixels[6], pixels[7]} == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    stbi_image_free(pixels);
  }

  TEST_CASE("Antialiases BMP black color-key boundaries on request") {
    std::vector<std::uint8_t> png;
    REQUIRE(pistoris::image::transcodeToPng(
                makeAntialiasedColorKeyBmp(), png, nullptr, pistoris::image::BmpColorKey::kAntialiased) ==
            pistoris::image::Error::kNone);

    int width = 0;
    int height = 0;
    int components = 0;
    stbi_uc* pixels = stbi_load_from_memory(png.data(), static_cast<int>(png.size()), &width, &height, &components, 4);
    REQUIRE(pixels != nullptr);
    bool has_partial_alpha = false;
    for (int index = 0; index < width * height; ++index) {
      const std::uint8_t alpha = pixels[static_cast<std::size_t>(index) * 4U + 3U];
      if (alpha != 0 && alpha != 255) has_partial_alpha = true;
    }
    CHECK(has_partial_alpha);
    stbi_image_free(pixels);
  }

  TEST_CASE("Normalizes only non-power-of-two images to power-of-two PNG") {
    const std::vector<std::uint8_t> npot = makeTestNpotBmp();
    std::vector<std::uint8_t> normalized;
    pistoris::image::Info info;
    bool rescaled = false;
    REQUIRE(pistoris::image::normalizeToPowerOfTwo(npot, normalized, &info, &rescaled) ==
            pistoris::image::Error::kNone);
    CHECK(rescaled);
    CHECK(info.format == pistoris::image::Format::kPng);
    CHECK(info.width == 4);
    CHECK(info.height == 2);
    CHECK(info.components == 3);

    pistoris::image::Info inspected;
    REQUIRE(pistoris::image::inspect(normalized, &inspected) == pistoris::image::Error::kNone);
    CHECK(inspected.format == pistoris::image::Format::kPng);
    CHECK(inspected.width == 4);
    CHECK(inspected.height == 2);

    const std::vector<std::uint8_t> pot = makeTestBmp();
    normalized = {42};
    rescaled = true;
    REQUIRE(pistoris::image::normalizeToPowerOfTwo(pot, normalized, &info, &rescaled) == pistoris::image::Error::kNone);
    CHECK_FALSE(rescaled);
    CHECK(info.format == pistoris::image::Format::kBmp);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(normalized == pot);
  }

  TEST_CASE("Power-of-two normalization rejects malformed input transactionally") {
    std::vector<std::uint8_t> normalized = {42};
    pistoris::image::Info info{pistoris::image::Format::kJpeg, 2, 3, 4};
    bool rescaled = true;
    CHECK(pistoris::image::normalizeToPowerOfTwo(std::vector<std::uint8_t>{1, 2, 3}, normalized, &info, &rescaled) ==
          pistoris::image::Error::kMalformed);
    CHECK(normalized == std::vector<std::uint8_t>{42});
    CHECK(info.format == pistoris::image::Format::kJpeg);
    CHECK(info.width == 2);
    CHECK(info.height == 3);
    CHECK(info.components == 4);
    CHECK(rescaled);
  }

  TEST_CASE("Fits inventory images to a transparent slot canvas") {
    std::vector<std::uint8_t> fitted;
    REQUIRE(pistoris::image::fitToPng(makeTestBmp(255, 0, 0), 96, 64, pistoris::image::FitMode::kCenter, fitted) ==
            pistoris::image::Error::kNone);

    pistoris::image::Info info;
    REQUIRE(pistoris::image::inspect(fitted, &info) == pistoris::image::Error::kNone);
    CHECK(info.format == pistoris::image::Format::kPng);
    CHECK(info.width == 96);
    CHECK(info.height == 64);
    CHECK(info.components == 4);

    int width = 0;
    int height = 0;
    int components = 0;
    stbi_uc* pixels =
        stbi_load_from_memory(fitted.data(), static_cast<int>(fitted.size()), &width, &height, &components, 4);
    REQUIRE(pixels != nullptr);
    CHECK(pixels[(32U * 96U + 15U) * 4U + 3U] == 0);
    CHECK(pixels[(32U * 96U + 16U) * 4U + 3U] == 255);
    CHECK(pixels[(32U * 96U + 79U) * 4U + 3U] == 255);
    CHECK(pixels[(32U * 96U + 80U) * 4U + 3U] == 0);
    stbi_image_free(pixels);

    REQUIRE(pistoris::image::fitToPng(makeTestBmp(255, 0, 0), 96, 64, pistoris::image::FitMode::kTopLeft, fitted) ==
            pistoris::image::Error::kNone);
    pixels = stbi_load_from_memory(fitted.data(), static_cast<int>(fitted.size()), &width, &height, &components, 4);
    REQUIRE(pixels != nullptr);
    CHECK(pixels[3] == 255);
    CHECK(pixels[(63U * 4U) + 3U] == 255);
    CHECK(pixels[(64U * 4U) + 3U] == 0);
    stbi_image_free(pixels);
  }

  TEST_CASE("Image fitting rejects invalid targets transactionally") {
    std::vector<std::uint8_t> fitted = {42};
    CHECK(pistoris::image::fitToPng(makeTestBmp(), 0, 32, pistoris::image::FitMode::kCenter, fitted) ==
          pistoris::image::Error::kMalformed);
    CHECK(fitted == std::vector<std::uint8_t>{42});
  }

  TEST_CASE("Rejects empty and malformed input transactionally") {
    pistoris::image::Info info;
    CHECK(pistoris::image::inspect({}, &info) == pistoris::image::Error::kMalformed);
    CHECK(pistoris::image::inspect(std::vector<std::uint8_t>{1, 2, 3}, &info) == pistoris::image::Error::kMalformed);

    pistoris::TexturesData textures{{pistoris::Texture{"texture"}}};
    textures.textures[0].encoded_image = {1, 2, 3};
    const pistoris::textures::ImagePreparationRequest request{
        0, {pistoris::image::formatFlag(pistoris::image::Format::kPng)}};
    std::vector<pistoris::textures::PreparedImage> out(1);
    out[0].bytes.converted = {42};
    out[0].info = {pistoris::image::Format::kJpeg, 2, 3, 4};
    CHECK(pistoris::textures::prepareImages(textures, {&request, 1}, out) == pistoris::textures::Error::kBadImage);
    REQUIRE(out.size() == 1);
    CHECK(out[0].bytes.converted == std::vector<std::uint8_t>{42});
    CHECK(out[0].info.format == pistoris::image::Format::kJpeg);
    CHECK(out[0].info.width == 2);
    CHECK(out[0].info.height == 3);
    CHECK(out[0].info.components == 4);
  }
}
