// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/result.hpp"
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
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

template <class T, class Location>
T take(pistoris::Result<T, Location>&& result) {
  REQUIRE(result);
  return std::move(*result);
}

pistoris::Level makeImageLevel() {
  pistoris::Level level;
  const ArxLevelRoom room_data{view("room")};
  const pistoris::RoomIndex room = take(level.addRoom(room_data));

  const std::array<float, 9> positions = {100, 0, 200, 200, 0, 200, 100, 0, 300};
  const std::array<std::uint32_t, 3> indices = {0, 1, 2};
  const std::array<float, 6> uvs = {0, 0, 1, 0, 0, 1};
  const std::array<float, 9> normals = {0, -1, 0, 0, -1, 0, 0, -1, 0};
  const std::array<pistoris::TextureIndex, 1> textures = {ARX_NO_TEXTURE};
  const std::array<float, 1> transvals = {0};
  const std::array<float, 9> colors = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
  const std::array<pistoris::RoomIndex, 1> rooms = {room};
  REQUIRE(level.replaceVertices(positions));
  REQUIRE(level.replaceFaces(indices, uvs, normals, textures, transvals, colors));
  REQUIRE(level.replaceFaceRooms(rooms));
  return pistoris::Level(level);
}

void checkImage(const std::vector<std::uint8_t>& encoded, std::uint32_t width, std::uint32_t height,
                ArxImageFormat format = ARX_IMAGE_FORMAT_PNG) {
  ArxImageInfo info{};
  REQUIRE(pistoris::binary::inspectEncodedImage(encoded, info) == ARX_OK);
  CHECK(info.format == format);
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
  TEST_CASE("Raw Level copies preflight grouped spans and flatten collection data") {
    pistoris::Level level = makeImageLevel();
    std::array<float, 9> positions{};
    REQUIRE(level.copyVertexPositions(positions));
    CHECK(positions[3] == doctest::Approx(200.0f));

    std::array<std::uint32_t, 3> indices{};
    std::array<float, 6> uvs{};
    std::array<float, 9> corner_normals{};
    std::array<pistoris::TextureIndex, 1> textures{};
    std::array<float, 1> transvals{};
    std::array<float, 9> colors{};
    std::array<float, 3> face_normals{};
    std::array<pistoris::FaceType, 1> flags{};
    pistoris::Level::FacesOutput faces;
    faces.vertex_indices = indices;
    faces.uvs = uvs;
    faces.corner_normals = corner_normals;
    faces.textures = textures;
    faces.transvals = transvals;
    faces.corner_colors = colors;
    faces.face_normals = face_normals;
    faces.flags = flags;
    REQUIRE(level.copyFaces(faces));
    CHECK(indices == std::array<std::uint32_t, 3>{0, 1, 2});
    CHECK(textures[0] == ARX_NO_TEXTURE);
    CHECK(colors[0] == doctest::Approx(0.5f));
    CHECK(face_normals[1] == doctest::Approx(-1.0f));

    indices.fill(99);
    pistoris::Level::FacesOutput invalid;
    invalid.vertex_indices = indices;
    invalid.uvs = std::span<float>(uvs.data(), uvs.size() - 1U);
    CHECK(level.copyFaces(invalid).code() == ARX_BUFFER_TOO_SMALL);
    CHECK(indices == std::array<std::uint32_t, 3>{99, 99, 99});

    pistoris::Level::FacesOutput none;
    CHECK(level.copyFaces(none).code() == ARX_INVALID_OPTIONS);
    pistoris::Level::FacesOutput overlapping;
    overlapping.vertex_indices = std::span<std::uint32_t>(indices.data(), 3U);
    overlapping.flags = std::span<pistoris::FaceType>(reinterpret_cast<pistoris::FaceType*>(indices.data()), 1U);
    CHECK(level.copyFaces(overlapping).code() == ARX_INVALID_OPTIONS);

    std::array<pistoris::RoomIndex, 1> rooms{};
    REQUIRE(level.copyFaceRooms(rooms));
    CHECK(rooms[0] == 0);
    std::array<pistoris::TextureIndex, 1> face_textures{};
    REQUIRE(level.copyFaceTextures(face_textures));
    CHECK(face_textures[0] == ARX_NO_TEXTURE);
  }

  TEST_CASE("Raw Level copies expose room distances and navigation collections") {
    pistoris::Level level = makeImageLevel();
    const ArxLevelRoom room{view("second")};
    REQUIRE(take(level.addRoom(room)) == 1);
    std::array<float, 1> distances = {};
    std::array<pistoris::PortalIndex, 2> portals = {};
    pistoris::Level::RoomDistancesOutput room_output;
    room_output.distances = distances;
    room_output.endpoint_portals = portals;
    REQUIRE(level.copyRoomDistances(room_output));
    CHECK(distances[0] == -1.0f);
    CHECK(portals[0] == ARX_INVALID_INDEX);

    const std::array<float, 6> anchor_positions = {0, 0, 0, 100, 0, 0};
    const std::array<float, 2> radii = {50, 50};
    const std::array<float, 2> heights = {-165, -165};
    const std::array<std::uint32_t, 2> anchor_flags = {0, 0};
    REQUIRE(level.replaceAnchors(anchor_positions, radii, heights, anchor_flags));
    const std::array<pistoris::AnchorIndex, 2> connection = {0, 1};
    REQUIRE(level.replaceAnchorConnections(connection));
    std::array<float, 6> copied_anchor_positions{};
    std::array<float, 2> copied_radii{};
    std::array<float, 2> copied_heights{};
    std::array<std::uint32_t, 2> copied_flags{};
    pistoris::Level::AnchorsOutput anchors;
    anchors.positions = copied_anchor_positions;
    anchors.radii = copied_radii;
    anchors.heights = copied_heights;
    anchors.flags = copied_flags;
    REQUIRE(level.copyAnchors(anchors));
    CHECK(copied_anchor_positions[3] == doctest::Approx(100.0f));
    CHECK(copied_radii[0] == doctest::Approx(50.0f));
    std::array<pistoris::AnchorIndex, 2> copied_connection{};
    REQUIRE(level.copyAnchorConnections(copied_connection));
    CHECK(copied_connection == connection);

    const std::array<pistoris::NavSurfaceVertexIndex, 3> triangles = {0, 1, 2};
    const std::array<float, 9> surface_positions = {0, 0, 0, 1, 0, 0, 0, 0, 1};
    REQUIRE(level.setNavSurface(surface_positions, triangles));
    std::array<float, 9> copied_surface_positions{};
    std::array<pistoris::NavSurfaceVertexIndex, 3> copied_triangles{};
    pistoris::Level::NavSurfaceOutput nav_surface;
    nav_surface.positions = copied_surface_positions;
    nav_surface.triangle_indices = copied_triangles;
    REQUIRE(level.copyNavSurface(nav_surface));
    CHECK(copied_surface_positions[3] == doctest::Approx(1.0f));
    CHECK(copied_triangles == triangles);
  }

  TEST_CASE("Minimap projection uses Arx units") {
    pistoris::Level level = makeImageLevel();
    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    REQUIRE(level.setMinimapFromProjection({image.data(), image.size()}, {25.0f, 50.0f}));

    const pistoris::Level::MinimapView stored = level.minimap();
    CHECK(stored.world_xz_bounds.min.x == doctest::Approx(75.0f));
    CHECK(stored.world_xz_bounds.min.y == doctest::Approx(325.0f));
    CHECK(stored.world_xz_bounds.max.x == doctest::Approx(125.0f));
    CHECK(stored.world_xz_bounds.max.y == doctest::Approx(350.0f));

    pistoris::Level::MinimapRenderOptions options;
    options.projection_offset = {25.0f, 50.0f};
    pistoris::Level::RenderedMinimap rendered = take(level.renderMinimap(options));
    CHECK(rendered.projection_offset.x == doctest::Approx(25.0f));
    CHECK(rendered.projection_offset.y == doctest::Approx(50.0f));
    checkImage(rendered.encoded_image, 2, 1);

    pistoris::Level::RenderedMinimap compact = take(level.renderMinimap());
    CHECK(compact.projection_offset.x == doctest::Approx(25.0f));
    CHECK(compact.projection_offset.y == doctest::Approx(50.0f));
    checkImage(compact.encoded_image, 2, 1);
  }

  TEST_CASE("Void geometry does not re-anchor Level minimap projection") {
    pistoris::Level level = makeImageLevel();
    const std::array<float, 18> positions = {
        100,
        0,
        200,
        200,
        0,
        200,
        100,
        0,
        300,
        1000,
        0,
        1000,
        1100,
        0,
        1000,
        1000,
        0,
        1100,
    };
    const std::array<std::uint32_t, 6> indices = {0, 1, 2, 3, 4, 5};
    const std::array<float, 12> uvs = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
    const std::array<float, 18> normals = {
        0,
        -1,
        0,
        0,
        -1,
        0,
        0,
        -1,
        0,
        0,
        -1,
        0,
        0,
        -1,
        0,
        0,
        -1,
        0,
    };
    const std::array<pistoris::TextureIndex, 2> textures = {ARX_NO_TEXTURE, ARX_NO_TEXTURE};
    const std::array<float, 2> transvals = {0, 0};
    const std::array<float, 18> colors = {
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
        0.5f,
    };
    const std::array<pistoris::RoomIndex, 2> rooms = {0, ARX_NO_ROOM};
    REQUIRE(level.replaceVertices(positions));
    REQUIRE(level.replaceFaces(indices, uvs, normals, textures, transvals, colors));
    REQUIRE(level.replaceFaceRooms(rooms));

    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    REQUIRE(level.setMinimapFromProjection({image.data(), image.size()}, {25.0f, 50.0f}));
    const pistoris::Level::MinimapView stored = level.minimap();
    CHECK(stored.world_xz_bounds.min.x == doctest::Approx(75.0f));
    CHECK(stored.world_xz_bounds.min.y == doctest::Approx(325.0f));
    CHECK(stored.world_xz_bounds.max.x == doctest::Approx(125.0f));
    CHECK(stored.world_xz_bounds.max.y == doctest::Approx(350.0f));
    const pistoris::Level::RenderedMinimap compact = take(level.renderMinimap());
    CHECK(compact.projection_offset.x == doctest::Approx(25.0f));
    CHECK(compact.projection_offset.y == doctest::Approx(50.0f));
  }

  TEST_CASE("Void-only Level cannot generate a minimap") {
    pistoris::Level level = makeImageLevel();
    const std::array<pistoris::RoomIndex, 1> rooms = {ARX_NO_ROOM};
    REQUIRE(level.replaceFaceRooms(rooms));
    CHECK(level.generateMinimap().code() == ARX_LEVEL_NO_GEOMETRY);
  }

  TEST_CASE("Game minimap rendering overwrites a one-pixel perimeter") {
    pistoris::Level level = makeImageLevel();
    const std::vector<std::uint8_t> image = makeSolidTestBmp(3, 3, 255, 0, 0);
    REQUIRE(level.setMinimapFromProjection({image.data(), image.size()}, {}));

    pistoris::Level::MinimapRenderOptions render_options;
    render_options.projection_offset = ArxVector2{};
    pistoris::Level::RenderedMinimap rendered = take(level.renderMinimap(render_options));
    checkImage(rendered.encoded_image, 3, 3);
    CHECK(pixel(rendered.encoded_image, 0, 0) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    pistoris::Level::MinimapRenderOptions game_options;
    game_options.mode = pistoris::level_images::MinimapRenderMode::kGame;
    game_options.projection_offset = ArxVector2{};
    game_options.border_color = ArxColor3{0.0f, 1.0f, 0.0f};
    rendered = take(level.renderMinimap(game_options));
    checkImage(rendered.encoded_image, 3, 3);
    CHECK(pixel(rendered.encoded_image, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(rendered.encoded_image, 2, 2) == std::array<std::uint8_t, 4>{0, 255, 0, 255});
    CHECK(pixel(rendered.encoded_image, 1, 1) == std::array<std::uint8_t, 4>{255, 0, 0, 255});

    game_options.format = pistoris::ImageFormat::kBmp;
    rendered = take(level.renderMinimap(game_options));
    checkImage(rendered.encoded_image, 3, 3, ARX_IMAGE_FORMAT_BMP);
    game_options.format = pistoris::ImageFormat::kTga;
    rendered = take(level.renderMinimap(game_options));
    checkImage(rendered.encoded_image, 3, 3, ARX_IMAGE_FORMAT_TGA);
  }

  TEST_CASE("Level generates a fixed minimap transactionally") {
    pistoris::Level level = makeImageLevel();
    REQUIRE(level.generateMinimap());
    pistoris::Level::MinimapView generated = level.minimap();
    checkImage({generated.encoded_image.data, generated.encoded_image.data + generated.encoded_image.size}, 640, 640);

    pistoris::Level::MinimapGenerationOptions options;
    options.foreground.color = {1.0f, 0.0f, 0.0f};
    options.background.color = {0.0f, 0.0f, 1.0f};
    options.water.color = {0.0f, 1.0f, 0.0f};
    options.lava.color = {1.0f, 0.5f, 0.0f};
    options.halo_color = {1.0f, 1.0f, 1.0f};
    options.halo_radius = 1;
    REQUIRE(level.generateMinimap(options));
    generated = level.minimap();
    CHECK(generated.world_xz_bounds.min.x == doctest::Approx(0.0f));
    CHECK(generated.world_xz_bounds.min.y == doctest::Approx(0.0f));
    CHECK(generated.world_xz_bounds.max.x == doctest::Approx(16000.0f));
    CHECK(generated.world_xz_bounds.max.y == doctest::Approx(16000.0f));
    checkImage({generated.encoded_image.data, generated.encoded_image.data + generated.encoded_image.size}, 640, 640);

    pistoris::Level::MinimapRenderOptions render_options;
    render_options.projection_offset = ArxVector2{};
    pistoris::Level::RenderedMinimap rendered = take(level.renderMinimap(render_options));
    checkImage(rendered.encoded_image, 636, 12);

    const pistoris::Level::MinimapView previous = level.minimap();
    options.halo_color.r = 1.1f;
    CHECK(level.generateMinimap(options).code() == ARX_INVALID_OPTIONS);
    CHECK(level.minimap().encoded_image.data == previous.encoded_image.data);
    CHECK(level.minimap().encoded_image.size == previous.encoded_image.size);
    options.halo_color.r = 1.0f;
    options.foreground.image = {nullptr, 1};
    CHECK(level.generateMinimap(options).code() == ARX_INVALID_DATA_POINTER);
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
    REQUIRE(pistoris::level_images::projectionOffsetForLevel(15, {}, projection_offset) == ARX_OK);
    CHECK(projection_offset.x == doctest::Approx(2015.0f));
    CHECK(projection_offset.y == doctest::Approx(-217.0f));
    REQUIRE(pistoris::level_images::projectionOffsetForLevel(20, {1.0f, 2.0f}, projection_offset) == ARX_OK);
    CHECK(projection_offset.x == doctest::Approx(65.0f));
    CHECK(projection_offset.y == doctest::Approx(124.0f));

    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    std::vector<std::uint8_t> rendered;
    pistoris::level_images::MinimapReprojectionOptions reprojection;
    reprojection.source_projection_offset = {25.0f, 50.0f};
    reprojection.target_projection_offset = {25.0f, 50.0f};
    REQUIRE(pistoris::level_images::reprojectMinimap(image, reprojection, rendered) == ARX_OK);
    checkImage(rendered, 2, 1);

    reprojection.mode = pistoris::level_images::MinimapRenderMode::kGame;
    reprojection.border_color = ArxColor3{0.0f, 1.0f, 0.0f};
    REQUIRE(pistoris::level_images::reprojectMinimap(image, reprojection, rendered) == ARX_OK);
    checkImage(rendered, 2, 1);
    CHECK(pixel(rendered, 0, 0) == std::array<std::uint8_t, 4>{0, 255, 0, 255});

    pistoris::level_images::LoadingScreenRenderOptions loading_options;
    REQUIRE(pistoris::level_images::renderLoadingScreen(image, loading_options, rendered) == ARX_OK);
    checkImage(rendered, 2, 1);
    loading_options.layout = pistoris::level_images::LoadingScreenLayout::kNormal;
    REQUIRE(pistoris::level_images::renderLoadingScreen(image, loading_options, rendered) == ARX_OK);
    checkImage(rendered, 320, 390);
  }

  TEST_CASE("Loading screen rendering and transcoding use their declared projections") {
    pistoris::Level level;
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setLoadingScreen({image.data(), image.size()}));

    pistoris::Level::LoadingScreenRenderOptions options;
    options.layout = pistoris::level_images::LoadingScreenLayout::kNormal;
    std::vector<std::uint8_t> rendered = take(level.renderLoadingScreen(options));
    checkImage(rendered, 320, 390);
    options.layout = pistoris::level_images::LoadingScreenLayout::kFullscreen;
    rendered = take(level.renderLoadingScreen(options));
    checkImage(rendered, 640, 480);
    rendered = take(level.renderLoadingScreen());
    checkImage(rendered, 1, 1);

    options = {};
    options.format = pistoris::ImageFormat::kBmp;
    rendered = take(level.renderLoadingScreen(options));
    checkImage(rendered, 1, 1, ARX_IMAGE_FORMAT_BMP);
    options.format = pistoris::ImageFormat::kTga;
    rendered = take(level.renderLoadingScreen(options));
    checkImage(rendered, 1, 1, ARX_IMAGE_FORMAT_TGA);
    options.format = pistoris::ImageFormat::kJpeg;
    CHECK(level.renderLoadingScreen(options).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Level GLB preserves minimap placement and omits the loading sidecar") {
    pistoris::Level source = makeImageLevel();
    const std::vector<std::uint8_t> image = makeSolidTestBmp(2, 1);
    REQUIRE(source.setMinimap({image.data(), image.size()}, {{125.0f, 225.0f}, {175.0f, 250.0f}}));
    REQUIRE(source.setLoadingScreen({image.data(), image.size()}));

    std::vector<std::uint8_t> glb = take(source.exportGlb());
    pistoris::Level::GlbImportOptions options;
    options.arx_offset = ArxVector3{};
    pistoris::Level imported = take(pistoris::Level::importGlb(glb, options));

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
    CHECK(level.setMinimap({}, {{0.0f, 0.0f}, {25.0f, 25.0f}}).code() == ARX_LEVEL_BAD_MINIMAP_IMAGE);
    CHECK(level.setMinimapFromProjection({}, {}).code() == ARX_LEVEL_BAD_MINIMAP_IMAGE);
    const std::array<std::uint8_t, 3> invalid_image = {1, 2, 3};
    CHECK(level.setMinimap({invalid_image.data(), invalid_image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}).code() ==
          ARX_LEVEL_BAD_MINIMAP_IMAGE);

    const std::vector<std::uint8_t> image = makeTestBmp();
    CHECK(level.setMinimap({image.data(), image.size()}, {{25.0f, 0.0f}, {0.0f, 25.0f}}).code() ==
          ARX_LEVEL_BAD_MINIMAP_BOUNDS);
    CHECK(level.setMinimapFromProjection({image.data(), image.size()}, {-3.0e38f, 0.0f}).code() ==
          ARX_LEVEL_BAD_MINIMAP_BOUNDS);
    CHECK(level.setLoadingScreen({}).code() == ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE);
    CHECK(level.setLoadingScreen({invalid_image.data(), invalid_image.size()}).code() ==
          ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE);
  }

  TEST_CASE("Minimap rendering bounds conversion is checked before integer conversion") {
    pistoris::Level level = makeImageLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(level.setMinimap({image.data(), image.size()}, {{-3.0e38f, -3.0e38f}, {-2.0e38f, -2.0e38f}}));

    pistoris::Level::MinimapRenderOptions options;
    options.projection_offset = ArxVector2{};
    pistoris::Level::RenderedMinimap rendered = take(level.renderMinimap(options));
    CHECK(rendered.encoded_image.empty());

    REQUIRE(level.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {300000.0f, 25.0f}}));
    CHECK(level.renderMinimap(options).code() == ARX_LEVEL_BAD_MINIMAP_BOUNDS);
  }

  TEST_CASE("Minimap render options are validated without image data") {
    pistoris::Level level;
    pistoris::Level::MinimapRenderOptions options;
    options.projection_offset = ArxVector2{std::numeric_limits<float>::infinity(), 0.0f};
    CHECK(level.renderMinimap(options).code() == ARX_INVALID_OPTIONS);

    options = {};
    options.border_color = ArxColor3{};
    CHECK(level.renderMinimap(options).code() == ARX_INVALID_OPTIONS);
    options.mode = pistoris::level_images::MinimapRenderMode::kGame;
    CHECK(level.renderMinimap(options).code() == ARX_INVALID_OPTIONS);
    options.projection_offset = ArxVector2{};
    options.border_color = ArxColor3{1.1f, 0.0f, 0.0f};
    CHECK(level.renderMinimap(options).code() == ARX_INVALID_OPTIONS);
    options.border_color.reset();
    options.format = pistoris::ImageFormat::kJpeg;
    CHECK(level.renderMinimap(options).code() == ARX_INVALID_OPTIONS);
    options.format = pistoris::ImageFormat::kUnknown;
    CHECK(level.renderMinimap(options).code() == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("Level GLB omits malformed reserved minimap roots") {
    pistoris::Level source = makeImageLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(source.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}));

    std::vector<std::uint8_t> glb = take(source.exportGlb());
    constexpr std::string_view kCanonicalName = "arx_minimap__map";
    constexpr std::string_view kMalformedName = "arx_minimap__m__";
    static_assert(kCanonicalName.size() == kMalformedName.size());
    const std::string_view glb_view(reinterpret_cast<const char*>(glb.data()), glb.size());
    const std::size_t name_offset = glb_view.find(kCanonicalName);
    REQUIRE(name_offset != std::string_view::npos);
    std::ranges::copy(kMalformedName, glb.begin() + static_cast<std::ptrdiff_t>(name_offset));

    pistoris::Level imported = take(pistoris::Level::importGlb(glb));
    CHECK(imported.minimap().encoded_image.size == 0);
    CHECK(imported.faceCount() == source.faceCount());
  }

  TEST_CASE("Level GLB omits invalid minimap payloads") {
    pistoris::Level source = makeImageLevel();
    const std::vector<std::uint8_t> image = makeTestBmp();
    REQUIRE(source.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}));

    std::vector<std::uint8_t> glb = take(source.exportGlb());
    constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    const auto signature = std::ranges::search(glb, kPngSignature);
    REQUIRE(signature.begin() != glb.end());
    const auto width = signature.begin() + 16;
    REQUIRE(width + 4 <= glb.end());
    std::ranges::fill(width, width + 4, 0);

    pistoris::Level imported = take(pistoris::Level::importGlb(glb));
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
