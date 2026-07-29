// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "external/glb/utils/image.h"
#include "image_helpers.h"
#include "modules/geometry.h"
#include "stb/stb_image.h"

#include <array>
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

}  // namespace

TEST_SUITE("texture images") {
  TEST_CASE("Detects encoded format without decoding") {
    CHECK(pistoris::geometry::detectImageFormat({}) == pistoris::geometry::ImageFormat::kUnknown);
    CHECK(pistoris::geometry::detectImageFormat(std::vector<std::uint8_t>{1, 2, 3}) ==
          pistoris::geometry::ImageFormat::kUnknown);
    CHECK(pistoris::geometry::detectImageFormat(makeTestBmp()) == pistoris::geometry::ImageFormat::kBmp);
    CHECK(pistoris::geometry::detectImageFormat(makeTestTga()) == pistoris::geometry::ImageFormat::kTga);
    CHECK(std::string(pistoris::geometry::imageExtension(pistoris::geometry::ImageFormat::kJpeg)) == ".jpg");
    CHECK(std::string(pistoris::geometry::imageExtension(pistoris::geometry::ImageFormat::kPng)) == ".png");
    CHECK(std::string(pistoris::geometry::imageExtension(pistoris::geometry::ImageFormat::kBmp)) == ".bmp");
    CHECK(std::string(pistoris::geometry::imageExtension(pistoris::geometry::ImageFormat::kTga)) == ".tga");
    CHECK(pistoris::geometry::imageExtension(pistoris::geometry::ImageFormat::kUnknown).empty());
  }

  TEST_CASE("Inspects supported lossless source formats") {
    pistoris::geometry::ImageInfo info;
    CHECK(pistoris::geometry::inspectImage(makeTestBmp(), &info) == pistoris::geometry::ImageError::kNone);
    CHECK(info.format == pistoris::geometry::ImageFormat::kBmp);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(info.components == 3);

    CHECK(pistoris::geometry::inspectImage(makeTestTga(), &info) == pistoris::geometry::ImageError::kNone);
    CHECK(info.format == pistoris::geometry::ImageFormat::kTga);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(info.components == 3);
  }

  TEST_CASE("Converts BMP and TGA to valid PNG") {
    for (const std::vector<std::uint8_t>& source : {makeTestBmp(), makeTestTga()}) {
      pistoris::glb::PreparedImage prepared;
      REQUIRE(pistoris::glb::prepareImage(source, prepared) == pistoris::geometry::ImageError::kNone);
      CHECK(prepared.info.format == pistoris::geometry::ImageFormat::kPng);
      CHECK_FALSE(pistoris::geometry::imageHasAlpha(prepared.info));

      pistoris::geometry::ImageInfo info;
      REQUIRE(pistoris::geometry::inspectImage(prepared.encoded, &info) == pistoris::geometry::ImageError::kNone);
      CHECK(info.format == pistoris::geometry::ImageFormat::kPng);
      CHECK(info.width == 1);
      CHECK(info.height == 1);

      pistoris::glb::PreparedImage pass_through;
      REQUIRE(pistoris::glb::prepareImage(prepared.encoded, pass_through) == pistoris::geometry::ImageError::kNone);
      CHECK(pass_through.info.format == pistoris::geometry::ImageFormat::kPng);
      CHECK(pass_through.encoded == prepared.encoded);
    }
  }

  TEST_CASE("Converts BMP black color key to alpha") {
    pistoris::geometry::ImageInfo info;
    std::vector<std::uint8_t> png;
    REQUIRE(pistoris::geometry::transcodeImageToPng(makeColorKeyBmp(), png, &info) ==
            pistoris::geometry::ImageError::kNone);
    CHECK(info.format == pistoris::geometry::ImageFormat::kPng);
    CHECK(info.components == 4);
    CHECK(pistoris::geometry::imageHasAlpha(info));

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

  TEST_CASE("Normalizes only non-power-of-two images to power-of-two PNG") {
    const std::vector<std::uint8_t> npot = makeTestNpotBmp();
    std::vector<std::uint8_t> normalized;
    pistoris::geometry::ImageInfo info;
    bool rescaled = false;
    REQUIRE(pistoris::geometry::normalizeImageToPowerOfTwo(npot, normalized, &info, &rescaled) ==
            pistoris::geometry::ImageError::kNone);
    CHECK(rescaled);
    CHECK(info.format == pistoris::geometry::ImageFormat::kPng);
    CHECK(info.width == 4);
    CHECK(info.height == 2);
    CHECK(info.components == 3);

    pistoris::geometry::ImageInfo inspected;
    REQUIRE(pistoris::geometry::inspectImage(normalized, &inspected) == pistoris::geometry::ImageError::kNone);
    CHECK(inspected.format == pistoris::geometry::ImageFormat::kPng);
    CHECK(inspected.width == 4);
    CHECK(inspected.height == 2);

    const std::vector<std::uint8_t> pot = makeTestBmp();
    normalized = {42};
    rescaled = true;
    REQUIRE(pistoris::geometry::normalizeImageToPowerOfTwo(pot, normalized, &info, &rescaled) ==
            pistoris::geometry::ImageError::kNone);
    CHECK_FALSE(rescaled);
    CHECK(info.format == pistoris::geometry::ImageFormat::kBmp);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    CHECK(normalized == pot);
  }

  TEST_CASE("Power-of-two normalization rejects malformed input transactionally") {
    std::vector<std::uint8_t> normalized = {42};
    pistoris::geometry::ImageInfo info{pistoris::geometry::ImageFormat::kJpeg, 2, 3, 4};
    bool rescaled = true;
    CHECK(pistoris::geometry::normalizeImageToPowerOfTwo(
              std::vector<std::uint8_t>{1, 2, 3}, normalized, &info, &rescaled) ==
          pistoris::geometry::ImageError::kMalformed);
    CHECK(normalized == std::vector<std::uint8_t>{42});
    CHECK(info.format == pistoris::geometry::ImageFormat::kJpeg);
    CHECK(info.width == 2);
    CHECK(info.height == 3);
    CHECK(info.components == 4);
    CHECK(rescaled);
  }

  TEST_CASE("Rejects empty and malformed input transactionally") {
    pistoris::geometry::ImageInfo info;
    CHECK(pistoris::geometry::inspectImage({}, &info) == pistoris::geometry::ImageError::kMalformed);
    CHECK(pistoris::geometry::inspectImage(std::vector<std::uint8_t>{1, 2, 3}, &info) ==
          pistoris::geometry::ImageError::kMalformed);

    pistoris::glb::PreparedImage out{{42}, {pistoris::geometry::ImageFormat::kJpeg, 2, 3, 4}};
    std::vector<std::uint8_t> malformed = {1, 2, 3};
    CHECK(pistoris::glb::prepareImage(malformed, out) == pistoris::geometry::ImageError::kMalformed);
    CHECK(out.encoded == std::vector<std::uint8_t>{42});
    CHECK(out.info.format == pistoris::geometry::ImageFormat::kJpeg);
    CHECK(out.info.width == 2);
    CHECK(out.info.height == 3);
    CHECK(out.info.components == 4);
  }
}
