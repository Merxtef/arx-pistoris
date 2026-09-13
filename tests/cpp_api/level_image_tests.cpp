// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/paths.hpp"

#include "image_helpers.h"
#include "stb/stb_image.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>  // IWYU pragma: keep
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

pistoris::Level makeImageLevel() {
  pistoris::Level level;
  pistoris::RoomIndex room = pistoris::kInvalidRoomIndex;
  const ArxLevelRoom room_data{view("room")};
  REQUIRE(level.addRoom(room_data, room) == ARX_OK);

  const std::array<ArxLevelVertex, 3> vertices = {
      ArxLevelVertex{{100.0f, 0.0f, 200.0f}},
      ArxLevelVertex{{200.0f, 0.0f, 200.0f}},
      ArxLevelVertex{{100.0f, 0.0f, 300.0f}},
  };
  ArxLevelFace face{};
  face.texture = ARX_NO_TEXTURE;
  face.room = room;
  for (std::size_t index = 0; index < 3; ++index) {
    face.corners[index].vertex = static_cast<pistoris::VertexIndex>(index);
    face.corners[index].normal = {0.0f, -1.0f, 0.0f};
  }
  const ArxLevelMeshInput mesh{vertices.data(), vertices.size(), &face, 1, nullptr, 0};
  REQUIRE(level.replaceMesh(mesh) == ARX_OK);
  return pistoris::Level(level);
}

void checkImage(const std::vector<std::uint8_t>& encoded, std::uint32_t width, std::uint32_t height) {
  ArxImageInfo info{};
  REQUIRE(pistoris::binary::inspectEncodedImage(encoded, info) == ARX_OK);
  CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
  CHECK(info.width == width);
  CHECK(info.height == height);
}

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

TEST_SUITE("C++ Level images") {
  TEST_CASE("Minimap projection uses Arx units") {
    pistoris::Level level = makeImageLevel();
    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    REQUIRE(level.setMinimapFromProjection({image.data(), image.size()}, {25.0f, 50.0f}) == ARX_OK);

    const pistoris::Level::MinimapView stored = level.minimap();
    CHECK(stored.world_xz_bounds.min.x == doctest::Approx(75.0f));
    CHECK(stored.world_xz_bounds.min.y == doctest::Approx(325.0f));
    CHECK(stored.world_xz_bounds.max.x == doctest::Approx(125.0f));
    CHECK(stored.world_xz_bounds.max.y == doctest::Approx(350.0f));

    std::vector<std::uint8_t> rendered;
    pistoris::Level::MinimapRenderOptions options;
    options.projection_offset = {25.0f, 50.0f};
    REQUIRE(level.renderMinimapPng(options, rendered) == ARX_OK);
    checkImage(rendered, 2, 1);

    ArxVector2 compact_offset{};
    REQUIRE(level.renderCompactMinimapPng(compact_offset, rendered) == ARX_OK);
    CHECK(compact_offset.x == doctest::Approx(25.0f));
    CHECK(compact_offset.y == doctest::Approx(50.0f));
    checkImage(rendered, 2, 1);
  }

  TEST_CASE("Game minimap rendering overwrites a one-pixel perimeter") {
    pistoris::Level level = makeImageLevel();
    const std::vector<std::uint8_t> image = makeSolidTestBmp(3, 3, 255, 0, 0);
    REQUIRE(level.setMinimapFromProjection({image.data(), image.size()}, {}) == ARX_OK);

    std::vector<std::uint8_t> rendered;
    REQUIRE(level.renderMinimapPng({}, rendered) == ARX_OK);
    checkImage(rendered, 3, 3);
    CHECK(pixel(rendered, 0, 0) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    REQUIRE(level.renderGameMinimapPng({.border_color = {0.0f, 1.0f, 0.0f}}, rendered) == ARX_OK);
    checkImage(rendered, 3, 3);
    CHECK(pixel(rendered, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(rendered, 2, 2) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(rendered, 1, 1) == std::array<std::uint8_t, 4>{255, 0, 0, 255});
  }

  TEST_CASE("Level generates a fixed minimap transactionally") {
    pistoris::Level level = makeImageLevel();
    REQUIRE(level.generateMinimap() == ARX_OK);
    pistoris::Level::MinimapView generated = level.minimap();
    checkImage({generated.encoded_image.data, generated.encoded_image.data + generated.encoded_image.size}, 640, 640);

    pistoris::Level::MinimapGenerationOptions options;
    options.foreground.color = {1.0f, 0.0f, 0.0f};
    options.background.color = {0.0f, 0.0f, 1.0f};
    options.water.color = {0.0f, 1.0f, 0.0f};
    options.lava.color = {1.0f, 0.5f, 0.0f};
    options.halo_color = {1.0f, 1.0f, 1.0f};
    options.halo_radius = 1;
    REQUIRE(level.generateMinimap(options) == ARX_OK);
    generated = level.minimap();
    CHECK(generated.world_xz_bounds.min.x == doctest::Approx(0.0f));
    CHECK(generated.world_xz_bounds.min.y == doctest::Approx(0.0f));
    CHECK(generated.world_xz_bounds.max.x == doctest::Approx(16000.0f));
    CHECK(generated.world_xz_bounds.max.y == doctest::Approx(16000.0f));
    checkImage({generated.encoded_image.data, generated.encoded_image.data + generated.encoded_image.size}, 640, 640);

    std::vector<std::uint8_t> rendered;
    REQUIRE(level.renderMinimapPng({}, rendered) == ARX_OK);
    checkImage(rendered, 636, 12);

    const pistoris::Level::MinimapView previous = level.minimap();
    options.halo_color.r = 1.1f;
    CHECK(level.generateMinimap(options) == ARX_INVALID_OPTIONS);
    CHECK(level.minimap().encoded_image.data == previous.encoded_image.data);
    CHECK(level.minimap().encoded_image.size == previous.encoded_image.size);
    options.halo_color.r = 1.0f;
    options.foreground.image = {nullptr, 1};
    CHECK(level.generateMinimap(options) == ARX_INVALID_DATA_POINTER);
    CHECK(level.minimap().encoded_image.data == previous.encoded_image.data);
    CHECK(level.minimap().encoded_image.size == previous.encoded_image.size);
  }

  TEST_CASE("Detached Level image helpers project encoded images without Level state") {
    ArxVector2 projection_offset{};
    REQUIRE(pistoris::level_images::projectionOffsetFromMiniOffset({1.0f, 2.0f}, projection_offset) == ARX_OK);
    CHECK(projection_offset.x == doctest::Approx(65.0f));
    CHECK(projection_offset.y == doctest::Approx(124.0f));
    ArxVector2 mini_offset{};
    REQUIRE(pistoris::level_images::miniOffsetFromProjectionOffset(projection_offset, mini_offset) == ARX_OK);
    CHECK(mini_offset.x == doctest::Approx(1.0f));
    CHECK(mini_offset.y == doctest::Approx(2.0f));

    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    std::vector<std::uint8_t> rendered;
    REQUIRE(pistoris::level_images::reprojectMinimapPng(
                image,
                {.source_projection_offset = {25.0f, 50.0f}, .target_projection_offset = {25.0f, 50.0f}},
                rendered) == ARX_OK);
    checkImage(rendered, 2, 1);

    REQUIRE(pistoris::level_images::reprojectGameMinimapPng(image,
                                                            {.source_projection_offset = {25.0f, 50.0f},
                                                             .target_projection_offset = {25.0f, 50.0f},
                                                             .border_color = {0.0f, 1.0f, 0.0f}},
                                                            rendered) == ARX_OK);
    checkImage(rendered, 2, 1);
    CHECK(pixel(rendered, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});

    REQUIRE(pistoris::level_images::renderLoadingScreenPng(
                image, pistoris::level_images::LoadingScreenLayout::kOriginal, rendered) == ARX_OK);
    checkImage(rendered, 2, 1);
    REQUIRE(pistoris::level_images::renderLoadingScreenPng(
                image, pistoris::level_images::LoadingScreenLayout::kNormal, rendered) == ARX_OK);
    checkImage(rendered, 320, 390);
  }

  TEST_CASE("Loading screen rendering and transcoding use their declared projections") {
    pistoris::Level level;
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setLoadingScreen({image.data(), image.size()}) == ARX_OK);

    std::vector<std::uint8_t> rendered;
    REQUIRE(level.renderLoadingScreenPng(rendered) == ARX_OK);
    checkImage(rendered, 320, 390);
    REQUIRE(level.renderFullscreenLoadingScreenPng(rendered) == ARX_OK);
    checkImage(rendered, 640, 480);
    REQUIRE(level.transcodeLoadingScreenPng(rendered) == ARX_OK);
    checkImage(rendered, 1, 1);
  }

  TEST_CASE("Level GLB preserves minimap placement and omits the loading sidecar") {
    pistoris::Level source = makeImageLevel();
    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    REQUIRE(source.setMinimap({image.data(), image.size()}, {{125.0f, 225.0f}, {175.0f, 250.0f}}) == ARX_OK);
    REQUIRE(source.setLoadingScreen({image.data(), image.size()}) == ARX_OK);

    std::vector<std::uint8_t> glb;
    REQUIRE(source.exportGlb(glb) == ARX_OK);
    pistoris::Level imported;
    pistoris::Level::GlbImportOptions options;
    options.arx_offset = ArxVector3{};
    REQUIRE(pistoris::Level::importGlb(imported, glb, options) == ARX_OK);

    const pistoris::Level::MinimapView minimap = imported.minimap();
    CHECK(minimap.encoded_image.size != 0);
    CHECK(minimap.world_xz_bounds.min.x == doctest::Approx(125.0f));
    CHECK(minimap.world_xz_bounds.min.y == doctest::Approx(225.0f));
    CHECK(minimap.world_xz_bounds.max.x == doctest::Approx(175.0f));
    CHECK(minimap.world_xz_bounds.max.y == doctest::Approx(250.0f));
    CHECK(imported.loadingScreen().size == 0);
  }

  TEST_CASE("Level images reject invalid data through focused errors") {
    pistoris::Level level = makeImageLevel();
    CHECK(level.setMinimap({}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) == ARX_LEVEL_BAD_MINIMAP_IMAGE);
    CHECK(level.setMinimapFromProjection({}, {}) == ARX_LEVEL_BAD_MINIMAP_IMAGE);
    const std::array<std::uint8_t, 3> invalid_image = {1, 2, 3};
    CHECK(level.setMinimap({invalid_image.data(), invalid_image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) ==
          ARX_LEVEL_BAD_MINIMAP_IMAGE);

    const std::vector<std::uint8_t> image = makeTestBmp();
    CHECK(level.setMinimap({image.data(), image.size()}, {{25.0f, 0.0f}, {0.0f, 25.0f}}) ==
          ARX_LEVEL_BAD_MINIMAP_BOUNDS);
    CHECK(level.setMinimapFromProjection({image.data(), image.size()}, {-3.0e38f, 0.0f}) ==
          ARX_LEVEL_BAD_MINIMAP_BOUNDS);
    CHECK(level.setLoadingScreen({}) == ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE);
    CHECK(level.setLoadingScreen({invalid_image.data(), invalid_image.size()}) == ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE);
  }

  TEST_CASE("Minimap rendering bounds conversion is checked before integer conversion") {
    pistoris::Level level = makeImageLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setMinimap({image.data(), image.size()}, {{-3.0e38f, -3.0e38f}, {-2.0e38f, -2.0e38f}}) == ARX_OK);

    std::vector<std::uint8_t> rendered = {42};
    REQUIRE(level.renderMinimapPng({}, rendered) == ARX_OK);
    CHECK(rendered.empty());

    REQUIRE(level.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {300000.0f, 25.0f}}) == ARX_OK);
    CHECK(level.renderMinimapPng({}, rendered) == ARX_LEVEL_BAD_MINIMAP_BOUNDS);
  }

  TEST_CASE("Minimap render options are validated without image data") {
    pistoris::Level level;
    pistoris::Level::MinimapRenderOptions options;
    options.projection_offset.x = std::numeric_limits<float>::infinity();
    std::vector<std::uint8_t> rendered = {42};
    CHECK(level.renderMinimapPng(options, rendered) == ARX_INVALID_OPTIONS);
    CHECK(rendered == std::vector<std::uint8_t>{42});

    pistoris::Level::GameMinimapRenderOptions game_options;
    game_options.border_color.r = 1.1f;
    CHECK(level.renderGameMinimapPng(game_options, rendered) == ARX_INVALID_OPTIONS);
    CHECK(rendered == std::vector<std::uint8_t>{42});
  }

  TEST_CASE("Level GLB omits malformed reserved minimap roots") {
    pistoris::Level source = makeImageLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(source.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) == ARX_OK);

    std::vector<std::uint8_t> glb;
    REQUIRE(source.exportGlb(glb) == ARX_OK);
    constexpr std::string_view kCanonicalName = "arx_minimap__map";
    constexpr std::string_view kMalformedName = "arx_minimap__m__";
    static_assert(kCanonicalName.size() == kMalformedName.size());
    const std::string_view glb_view(reinterpret_cast<const char*>(glb.data()), glb.size());
    const std::size_t name_offset = glb_view.find(kCanonicalName);
    REQUIRE(name_offset != std::string_view::npos);
    std::ranges::copy(kMalformedName, glb.begin() + static_cast<std::ptrdiff_t>(name_offset));

    pistoris::Level imported;
    REQUIRE(pistoris::Level::importGlb(imported, glb) == ARX_OK);
    CHECK(imported.minimap().encoded_image.size == 0);
    CHECK(imported.faceCount() == source.faceCount());
  }

  TEST_CASE("Level GLB omits invalid minimap payloads") {
    pistoris::Level source = makeImageLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(source.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) == ARX_OK);

    std::vector<std::uint8_t> glb;
    REQUIRE(source.exportGlb(glb) == ARX_OK);
    constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const auto signature = std::ranges::search(glb, kPngSignature);
    REQUIRE(signature.begin() != glb.end());
    const auto width = signature.begin() + 16;
    REQUIRE(width + 4 <= glb.end());
    std::ranges::fill(width, width + 4, 0);

    pistoris::Level imported;
    REQUIRE(pistoris::Level::importGlb(imported, glb) == ARX_OK);
    CHECK(imported.minimap().encoded_image.size == 0);
    CHECK(imported.faceCount() == source.faceCount());
  }

  TEST_CASE("Level image paths mirror game resource lookup") {
    CHECK(pistoris::paths::minimapResourceLevel(14) == 1);
    CHECK(pistoris::paths::minimapResourceLevel(24) == 24);
    CHECK(pistoris::paths::levelMinimap(14) == "graph/levels/level1/map");
    CHECK(pistoris::paths::levelLoadingScreen(14) == "graph/levels/level14/loading");
    CHECK(pistoris::paths::minimapOffsetsFile() == "graph/levels/mini_offsets.ini");
  }
}
