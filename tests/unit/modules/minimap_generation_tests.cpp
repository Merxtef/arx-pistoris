// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"

#include "image_helpers.h"
#include "modules/geometry.h"
#include "modules/minimap.h"
#include "stb/stb_image.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {

using namespace pistoris;

Face face(VertexIndex a, VertexIndex b, VertexIndex c, FaceType flags = 0) {
  Face out;
  out.corners[0].vertex = a;
  out.corners[1].vertex = b;
  out.corners[2].vertex = c;
  out.flags = flags;
  return out;
}

GeometryData squareGeometry(FaceType flags = 0, float y = 0.0f) {
  GeometryData geometry;
  geometry.vertices = {
      {{0.0f, y, 0.0f}},
      {{100.0f, y, 0.0f}},
      {{100.0f, y, 100.0f}},
      {{0.0f, y, 100.0f}},
  };
  geometry.faces = {face(0, 1, 2, flags), face(0, 2, 3, flags)};
  return geometry;
}

void appendGeometry(GeometryData& destination, const GeometryData& source) {
  const VertexIndex first = static_cast<VertexIndex>(destination.vertices.size());
  destination.vertices.insert(destination.vertices.end(), source.vertices.begin(), source.vertices.end());
  for (Face value : source.faces) {
    for (Corner& corner : value.corners) corner.vertex += first;
    destination.faces.push_back(value);
  }
}

std::vector<std::uint8_t> decodeRgba(std::span<const std::uint8_t> encoded, int& width, int& height) {
  int components = 0;
  stbi_uc* decoded =
      stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &width, &height, &components, 4);
  REQUIRE(decoded != nullptr);
  std::vector<std::uint8_t> pixels(decoded, decoded + static_cast<std::size_t>(width) * height * 4U);
  stbi_image_free(decoded);
  return pixels;
}

std::array<std::uint8_t, 4> pixel(const std::vector<std::uint8_t>& rgba, std::uint32_t x, std::uint32_t y) {
  const std::size_t offset = (static_cast<std::size_t>(y) * minimap::kGenerationWidth + x) * 4U;
  return {rgba[offset], rgba[offset + 1U], rgba[offset + 2U], rgba[offset + 3U]};
}

minimap::GenerationOptions options() {
  minimap::GenerationOptions out;
  out.foreground.color = {1.0f, 0.0f, 0.0f};
  out.background.color = {0.0f, 0.0f, 1.0f};
  out.water.color = {0.0f, 1.0f, 0.0f};
  out.lava.color = {1.0f, 0.5f, 0.0f};
  out.halo_color = {1.0f, 1.0f, 1.0f};
  out.halo_radius = 1;
  return out;
}

}  // namespace

TEST_SUITE("minimap::generation") {
  TEST_CASE("Samples occupied pixels and produces exact Chebyshev halo rings") {
    const GeometryData geometry = squareGeometry();
    MinimapData generated;
    minimap::GenerationDiagnostics diagnostics;
    REQUIRE(minimap::generate(generated, geometry, options(), &diagnostics) == minimap::Error::kNone);

    int width = 0;
    int height = 0;
    const std::vector<std::uint8_t> rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(width == 640);
    CHECK(height == 640);
    CHECK(pixel(rgba, 0, 639) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
    CHECK(pixel(rgba, 4, 639) == std::array<std::uint8_t, 4>{255, 255, 255, 255});
    CHECK(pixel(rgba, 5, 639) == std::array<std::uint8_t, 4>{0, 0, 255, 255});
    CHECK(generated.world_xz_bounds.min.x == doctest::Approx(0.0f));
    CHECK(generated.world_xz_bounds.min.y == doctest::Approx(0.0f));
    CHECK(generated.world_xz_bounds.max.x == doctest::Approx(16000.0f));
    CHECK(generated.world_xz_bounds.max.y == doctest::Approx(16000.0f));
    CHECK(diagnostics.foreground_pixels == 16);
    CHECK(diagnostics.halo_pixels == 9);
    CHECK(diagnostics.sampled_cells + diagnostics.skipped_cells == 25600);
    CHECK(diagnostics.sampled_cells < 16);
  }

  TEST_CASE("Highest eligible surface determines the pixel class") {
    GeometryData geometry = squareGeometry();
    appendGeometry(geometry, squareGeometry(kFaceBitWater, 20.0f));
    appendGeometry(geometry, squareGeometry(kFaceBitLava, 30.0f));

    MinimapData generated;
    REQUIRE(minimap::generate(generated, geometry, options()) == minimap::Error::kNone);
    int width = 0;
    int height = 0;
    const std::vector<std::uint8_t> rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(rgba, 0, 639) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    geometry.faces[0].flags = kFaceBitWater | kFaceBitLava;
    geometry.faces[1].flags = kFaceBitWater | kFaceBitLava;
    REQUIRE(minimap::generate(generated, geometry, options()) == minimap::Error::kNone);
    const std::vector<std::uint8_t> lava_rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(lava_rgba, 0, 639) == std::array<std::uint8_t, 4>{255, 128, 0, 255});

    GeometryData tied = squareGeometry(kFaceBitWater);
    appendGeometry(tied, squareGeometry());
    REQUIRE(minimap::generate(generated, tied, options()) == minimap::Error::kNone);
    const std::vector<std::uint8_t> tied_rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(tied_rgba, 0, 639) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
  }

  TEST_CASE("Steep and downward-facing surfaces are ignored") {
    GeometryData gentle = squareGeometry();
    gentle.vertices[1].position.y = -1000.0f;
    gentle.vertices[2].position.y = -1000.0f;
    MinimapData generated;
    REQUIRE(minimap::generate(generated, gentle, options()) == minimap::Error::kNone);
    int width = 0;
    int height = 0;
    const std::vector<std::uint8_t> gentle_rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(gentle_rgba, 0, 639) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    GeometryData steep = squareGeometry();
    steep.vertices[1].position.y = -2000.0f;
    steep.vertices[2].position.y = -2000.0f;
    REQUIRE(minimap::generate(generated, steep, options()) == minimap::Error::kNone);
    const std::vector<std::uint8_t> steep_rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(steep_rgba, 0, 639) == std::array<std::uint8_t, 4>{0, 0, 255, 255});

    GeometryData downward = squareGeometry();
    for (Face& value : downward.faces) std::swap(value.corners[1], value.corners[2]);
    REQUIRE(minimap::generate(generated, downward, options()) == minimap::Error::kNone);
    const std::vector<std::uint8_t> downward_rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(downward_rgba, 0, 639) == std::array<std::uint8_t, 4>{0, 0, 255, 255});
  }

  TEST_CASE("Sampler images resize, tint, and fail transactionally") {
    const GeometryData geometry = squareGeometry();
    minimap::GenerationOptions generation = options();
    const std::vector<std::uint8_t> red = makeSolidTestBmp(1, 1, 255, 0, 0);
    generation.foreground = {{red.data(), red.size()}, {0.5f, 1.0f, 1.0f}};
    MinimapData generated;
    REQUIRE(minimap::generate(generated, geometry, generation) == minimap::Error::kNone);
    int width = 0;
    int height = 0;
    const std::vector<std::uint8_t> rgba = decodeRgba(generated.encoded_image, width, height);
    CHECK(pixel(rgba, 0, 639) == std::array<std::uint8_t, 4>{128, 0, 0, 255});

    const std::vector<std::uint8_t> previous = generated.encoded_image;
    const std::array<std::uint8_t, 3> invalid = {1, 2, 3};
    generation.foreground.encoded_image = invalid;
    CHECK(minimap::generate(generated, geometry, generation) == minimap::Error::kBadImage);
    CHECK(generated.encoded_image == previous);

    generation.foreground.encoded_image = {};
    generation.foreground.color.r = 1.1f;
    CHECK(minimap::generate(generated, geometry, generation) == minimap::Error::kInvalidOptions);
    CHECK(generated.encoded_image == previous);
  }
}
