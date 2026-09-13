// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "image_helpers.h"
#include "stb/stb_image.h"
#include "utils/encoded_image.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

std::array<std::uint8_t, 4> pixel(const std::vector<std::uint8_t>& encoded, int x, int y) {
  int width = 0;
  int height = 0;
  int components = 0;
  stbi_uc* decoded =
      stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &components, 4);
  REQUIRE(decoded != nullptr);
  REQUIRE(x >= 0);
  REQUIRE(y >= 0);
  REQUIRE(x < width);
  REQUIRE(y < height);
  const std::size_t offset =
      (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4U;
  const std::array<std::uint8_t, 4> result = {
      decoded[offset], decoded[offset + 1], decoded[offset + 2], decoded[offset + 3]};
  stbi_image_free(decoded);
  return result;
}

}  // namespace

TEST_SUITE("encoded image placement") {
  TEST_CASE("Places and crops encoded images on a filled PNG canvas") {
    std::vector<std::uint8_t> source = makeSolidTestBmp(2, 1, 255, 0, 0);
    source[57] = 0;
    source[58] = 255;
    source[59] = 0;

    std::vector<std::uint8_t> placed;
    REQUIRE(pistoris::image::placeToPng(
                source, 4, 3, {.x = 1, .y = 1, .width = 2, .height = 1}, {0, 0, 255, 255}, placed) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(placed, 0, 0) == std::array<std::uint8_t, 4>{0, 0, 255, 255});
    CHECK(pixel(placed, 1, 1) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    CHECK(pixel(placed, 2, 1) == std::array<std::uint8_t, 4>{0, 255, 0, 255});

    REQUIRE(pistoris::image::placeToPng(source, 1, 1, {.x = -1, .y = 0, .width = 2, .height = 1}, {}, placed) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(placed, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
  }

  TEST_CASE("Optional border overwrites the final canvas perimeter") {
    const std::vector<std::uint8_t> source = makeSolidTestBmp(3, 3, 255, 0, 0);
    std::vector<std::uint8_t> placed;
    REQUIRE(pistoris::image::placeToPng(source,
                                        3,
                                        3,
                                        {.x = 0, .y = 0, .width = 3, .height = 3},
                                        {},
                                        placed,
                                        std::array<std::uint8_t, 4>{0, 255, 0, 255}) == pistoris::image::Error::kNone);
    CHECK(pixel(placed, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(placed, 2, 2) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(placed, 1, 1) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
  }

  TEST_CASE("Rejects invalid placement without replacing the output") {
    std::vector<std::uint8_t> output = {42};
    CHECK(pistoris::image::placeToPng(makeTestBmp(), 1, 1, {.x = 0, .y = 0, .width = 0, .height = 1}, {}, output) ==
          pistoris::image::Error::kMalformed);
    CHECK(output == std::vector<std::uint8_t>{42});
  }

  TEST_CASE("Rejects image dimensions above the shared limit") {
    pistoris::image::Info info;
    CHECK(pistoris::image::inspectMetadata(makeSolidTestBmp(pistoris::image::kMaxDimension + 1U, 1), info) ==
          pistoris::image::Error::kMalformed);

    std::vector<std::uint8_t> output = {42};
    CHECK(pistoris::image::fitToPng(
              makeTestBmp(), pistoris::image::kMaxDimension + 1U, 1, pistoris::image::FitMode::kStretch, output) ==
          pistoris::image::Error::kMalformed);
    CHECK(output == std::vector<std::uint8_t>{42});
    CHECK(pistoris::image::placeToPng(makeTestBmp(),
                                      pistoris::image::kMaxDimension + 1U,
                                      1,
                                      {.x = 0, .y = 0, .width = 1, .height = 1},
                                      {},
                                      output) == pistoris::image::Error::kMalformed);
    CHECK(output == std::vector<std::uint8_t>{42});
  }

  TEST_CASE("Fits images at every supported canvas layout") {
    constexpr std::array<std::uint8_t, 4> kOpaqueRed = {255, 0, 0, 255};
    constexpr std::array<std::uint8_t, 4> kTransparent = {};
    std::vector<std::uint8_t> fitted;

    const std::vector<std::uint8_t> wide = makeSolidTestBmp(2, 1, 255, 0, 0);
    REQUIRE(pistoris::image::fitToPng(wide, 3, 3, pistoris::image::FitMode::kCenter, fitted) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(fitted, 1, 0) == kTransparent);
    CHECK(pixel(fitted, 1, 1) == kOpaqueRed);
    CHECK(pixel(fitted, 1, 2) == kTransparent);

    REQUIRE(pistoris::image::fitToPng(wide, 4, 3, pistoris::image::FitMode::kCenter, fitted) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(fitted, 2, 0) == kOpaqueRed);
    CHECK(pixel(fitted, 2, 1) == kOpaqueRed);
    CHECK(pixel(fitted, 2, 2) == kTransparent);

    for (pistoris::image::FitMode mode : {pistoris::image::FitMode::kTopLeft, pistoris::image::FitMode::kTopRight}) {
      REQUIRE(pistoris::image::fitToPng(wide, 3, 3, mode, fitted) == pistoris::image::Error::kNone);
      CHECK(pixel(fitted, 1, 0) == kOpaqueRed);
      CHECK(pixel(fitted, 1, 2) == kTransparent);
    }
    for (pistoris::image::FitMode mode :
         {pistoris::image::FitMode::kBottomLeft, pistoris::image::FitMode::kBottomRight}) {
      REQUIRE(pistoris::image::fitToPng(wide, 3, 3, mode, fitted) == pistoris::image::Error::kNone);
      CHECK(pixel(fitted, 1, 0) == kTransparent);
      CHECK(pixel(fitted, 1, 2) == kOpaqueRed);
    }

    const std::vector<std::uint8_t> tall = makeSolidTestBmp(1, 2, 255, 0, 0);
    for (pistoris::image::FitMode mode : {pistoris::image::FitMode::kTopLeft, pistoris::image::FitMode::kBottomLeft}) {
      REQUIRE(pistoris::image::fitToPng(tall, 3, 3, mode, fitted) == pistoris::image::Error::kNone);
      CHECK(pixel(fitted, 0, 1) == kOpaqueRed);
      CHECK(pixel(fitted, 2, 1) == kTransparent);
    }
    for (pistoris::image::FitMode mode :
         {pistoris::image::FitMode::kTopRight, pistoris::image::FitMode::kBottomRight}) {
      REQUIRE(pistoris::image::fitToPng(tall, 3, 3, mode, fitted) == pistoris::image::Error::kNone);
      CHECK(pixel(fitted, 0, 1) == kTransparent);
      CHECK(pixel(fitted, 2, 1) == kOpaqueRed);
    }

    REQUIRE(pistoris::image::fitToPng(wide, 3, 3, pistoris::image::FitMode::kStretch, fitted) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(fitted, 0, 0) == kOpaqueRed);
    CHECK(pixel(fitted, 2, 2) == kOpaqueRed);
  }

  TEST_CASE("Rejects invalid fit modes transactionally") {
    std::vector<std::uint8_t> fitted = {42};
    CHECK(pistoris::image::fitToPng(makeTestBmp(), 3, 3, static_cast<pistoris::image::FitMode>(255), fitted) ==
          pistoris::image::Error::kMalformed);
    CHECK(fitted == std::vector<std::uint8_t>{42});
  }

  TEST_CASE("Rotates encoded images by exact quarter turns") {
    std::vector<std::uint8_t> source = makeSolidTestBmp(2, 1, 255, 0, 0);
    source[57] = 0;
    source[58] = 255;
    source[59] = 0;

    std::vector<std::uint8_t> rotated;
    REQUIRE(pistoris::image::rotateQuarterTurnToPng(source, pistoris::image::QuarterTurn::kClockwise90, rotated) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(rotated, 0, 0) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    CHECK(pixel(rotated, 0, 1) == std::array<std::uint8_t, 4>{0, 255, 0, 255});

    REQUIRE(pistoris::image::rotateQuarterTurnToPng(source, pistoris::image::QuarterTurn::kClockwise180, rotated) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(rotated, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(rotated, 1, 0) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    REQUIRE(pistoris::image::rotateQuarterTurnToPng(source, pistoris::image::QuarterTurn::kClockwise270, rotated) ==
            pistoris::image::Error::kNone);
    CHECK(pixel(rotated, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(rotated, 0, 1) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
  }
}
