// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "../../cli/io/default_mounts.h"
#include "../../cli/io/service.h"
#include "../../cli/resources/inventory_icon_io.h"
#include "../../cli/resources/level_image_io.h"
#include "../../cli/resources/sound_io.h"
#include "../../cli/resources/texture_io.h"
#include "base/resource_path.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "image_helpers.h"
#include "io/path_location.h"
#include "io/policy.h"
#include "level_add_helpers.h"
#include "media/encoded.h"
#include "modules/geometry.h"
#include "resources/input.h"
#include "resources/layout.h"
#include "resources/resource_output.h"
#include "resources/selector.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <cstdlib>
#include <optional>
#include <stdlib.h>
#endif

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    static std::atomic<unsigned> sequence = 0;
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("arx-pistoris-cli-mount-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
    std::error_code ec;
    REQUIRE(std::filesystem::create_directories(path_, ec));
    REQUIRE_FALSE(ec);
  }

  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  ~TemporaryDirectory() noexcept {
    try {
      std::error_code ec;
      std::filesystem::remove_all(path_, ec);
    } catch (...) {
      // Cleanup must not let allocation failures escape a destructor
      return;
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

class ScopedCurrentDirectory {
 public:
  explicit ScopedCurrentDirectory(const std::filesystem::path& path) : original_(std::filesystem::current_path()) {
    std::error_code ec;
    std::filesystem::current_path(path, ec);
    REQUIRE_FALSE(ec);
  }

  ScopedCurrentDirectory(const ScopedCurrentDirectory&) = delete;
  ScopedCurrentDirectory& operator=(const ScopedCurrentDirectory&) = delete;

  ~ScopedCurrentDirectory() noexcept {
    std::error_code ec;
    std::filesystem::current_path(original_, ec);
  }

 private:
  std::filesystem::path original_;
};

#ifndef _WIN32
class ScopedEnvironmentOverride {
 public:
  ScopedEnvironmentOverride(const char* name, const char* value) : name_(name) {
    if (const char* original = std::getenv(name)) original_ = original;
    REQUIRE(::setenv(name, value, 1) == 0);
  }

  ScopedEnvironmentOverride(const ScopedEnvironmentOverride&) = delete;
  ScopedEnvironmentOverride& operator=(const ScopedEnvironmentOverride&) = delete;

  ~ScopedEnvironmentOverride() {
    if (original_) {
      (void)::setenv(name_.c_str(), original_->c_str(), 1);
    } else {
      (void)::unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  std::optional<std::string> original_;
};
#endif

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  REQUIRE_FALSE(ec);
  std::ofstream output(path, std::ios::binary);
  REQUIRE(output.good());
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  return {std::istreambuf_iterator<char>(input), {}};
}

pistoris::Level textureLevel(std::string path) {
  pistoris::Level level;
  REQUIRE(test::addRoom(level, {"room"}) == 0);

  test::MeshSnapshot mesh;
  mesh.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
  pistoris::Face face;
  face.corners[0] = test::corner(0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f);
  face.corners[1] = test::corner(1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f);
  face.corners[2] = test::corner(2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f);
  face.texture = 0;
  mesh.faces.push_back(face);
  mesh.textures.emplace_back(std::move(path));
  mesh.textures.emplace_back("graph/obj3d/textures/unused.bmp");
  mesh.face_rooms.push_back(0);
  REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);
  return pistoris::Level(level);
}

std::vector<std::string> mountPaths(const std::filesystem::path& first, const std::filesystem::path& second = {}) {
  std::vector<std::string> result = {first.string()};
  if (!second.empty()) result.push_back(second.string());
  return result;
}

}  // namespace

TEST_SUITE("CLI mounts") {
  TEST_CASE("Mounted paths use portable components while absolute paths retain host syntax") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});
    cli::PathLocation location;
    std::string error;

    REQUIRE(io.resolvePathLocation("Folder/My texture_(stone)&[wet]__01.ftl", location, error));
    CHECK(location.address == cli::PathAddress::kMountRelative);
    CHECK(location.path == "Folder/My texture_(stone)&[wet]__01.ftl");
    CHECK_FALSE(io.resolvePathLocation("folder/my#texture.ftl", location, error));

    const std::filesystem::path absolute = temp.path() / "my#texture.ftl";
    REQUIRE(io.resolvePathLocation(absolute.string(), location, error));
    CHECK(location.address == cli::PathAddress::kAbsolute);
  }

  TEST_CASE("Default game resource root is absolute or explicitly unavailable") {
    std::filesystem::path root;
    std::string error;
    if (cli::defaultGameResourceRoot(root, error)) {
      CHECK(root.is_absolute());
      CHECK(error.empty());
    } else {
      CHECK(root.empty());
      CHECK_FALSE(error.empty());
    }
  }

#ifndef _WIN32
  TEST_CASE("Default game resource root rejects relative environment paths") {
    TemporaryDirectory temp;
    REQUIRE(temp.path().is_absolute());

    {
      ScopedEnvironmentOverride data_home("XDG_DATA_HOME", "relative-data");
      ScopedEnvironmentOverride home("HOME", temp.path().string().c_str());
      std::filesystem::path root;
      std::string error;
      REQUIRE(cli::defaultGameResourceRoot(root, error));
      CHECK(root == temp.path() / ".local" / "share" / "arx");
      CHECK(error.empty());
    }

    {
      ScopedEnvironmentOverride data_home("XDG_DATA_HOME", "relative-data");
      ScopedEnvironmentOverride home("HOME", "relative-home");
      std::filesystem::path root;
      std::string error;
      CHECK_FALSE(cli::defaultGameResourceRoot(root, error));
      CHECK(root.empty());
      CHECK_FALSE(error.empty());
    }
  }
#endif

  TEST_CASE("Immediate file enumeration supports mounted and absolute directories") {
    TemporaryDirectory temp;
    writeBytes(temp.path() / "images" / "a.png", makeTestBmp());
    writeBytes(temp.path() / "images" / "b.bmp", makeTestBmp());
    REQUIRE(std::filesystem::create_directories(temp.path() / "images" / "nested"));
    writeBytes(temp.path() / "images" / "nested" / "ignored.png", makeTestBmp());
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));

    std::vector<cli::PathLocation> files;
    REQUIRE(io.enumerateFiles({.path = "images", .address = cli::PathAddress::kMountRelative}, files) ==
            cli::ResourceEnumerationResult::kSuccess);
    REQUIRE(files.size() == 2);
    CHECK(cli::resourceFilename(files[0].path) == "a.png");
    CHECK(cli::resourceFilename(files[1].path) == "b.bmp");

    REQUIRE(io.enumerateFiles({.path = (temp.path() / "images").string(), .address = cli::PathAddress::kAbsolute},
                              files) == cli::ResourceEnumerationResult::kSuccess);
    CHECK(files.size() == 2);
  }

  TEST_CASE("Loose Level minimap discovery prefers the plain sidecar name") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> plain = makeTestBmp();
    writeBytes(temp.path() / "level[map].bmp", plain);
    writeBytes(temp.path() / "level[map][offset_4_-2].png", makeSolidTestBmp(2, 1));
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
    const cli::ClassifiedPath input{
        .path = (temp.path() / "level.fts").string(),
        .location = {.path = (temp.path() / "level.fts").string(), .address = cli::PathAddress::kAbsolute},
        .buffer = {},
        .facts = {.format = cli::Format::kFts},
        .positional_index = 0,
        .resource_kind = ARX_RESOURCE_KIND_NONE,
    };
    cli::level::LevelImageInput resolved;
    REQUIRE(cli::level::resolveLevelImageInput(input, io, resolved));
    cli::level::LoadedLevelImages loaded;
    cli::level::loadLevelImages(io, resolved, loaded);
    CHECK(loaded.minimap.image.encoded == plain);
    CHECK(loaded.minimap.projection_offset.x == 0.0f);
    CHECK(loaded.minimap.projection_offset.y == 0.0f);

    std::error_code ec;
    REQUIRE(std::filesystem::remove(temp.path() / "level[map].bmp", ec));
    REQUIRE_FALSE(ec);
    cli::IoService offset_io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
    REQUIRE(cli::level::resolveLevelImageInput(input, offset_io, resolved));
    cli::level::loadLevelImages(offset_io, resolved, loaded);
    CHECK_FALSE(loaded.minimap.image.encoded.empty());
    CHECK(loaded.minimap.projection_offset.x == 4.0f);
    CHECK(loaded.minimap.projection_offset.y == -2.0f);
  }

  TEST_CASE("Direct loose Level images preserve their source encoding and minimap offset") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> minimap = makeTestBmp();
    cli::level::LoadedLevelImages images;
    REQUIRE(cli::media::prepareImage(minimap, images.minimap.image) == ARX_OK);
    images.minimap.projection_offset = {4.0f, -2.0f};

    cli::level::LevelImageInput input;
    cli::level::LevelImageOutput output{
        .minimap_enabled = true,
        .minimap_stem = {.path = (temp.path() / "copy[map]").string(), .address = cli::PathAddress::kAbsolute},
        .loading_screen_stem = {},
    };
    cli::level::GeneratedLevelImages generated;
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kLevel, "copy.fts");
    REQUIRE(cli::level::addDirectLevelImageOutputs(plan, input, output, images, generated, asset));
    REQUIRE(plan.resolve(false, false));
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});
    REQUIRE(plan.write(io));

    CHECK(readBytes(temp.path() / "copy[map][offset_4_-2].bmp") == minimap);
  }

  TEST_CASE("Direct game Level images frame loose input but preserve matching game input") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> minimap = makeSolidTestBmp(3, 3);
    cli::level::LoadedLevelImages images;
    REQUIRE(cli::media::prepareImage(minimap, images.minimap.image) == ARX_OK);

    cli::level::LevelImageOutput output{
        .minimap_enabled = true,
        .minimap_stem = {.path = (temp.path() / "map").string(), .address = cli::PathAddress::kAbsolute},
        .loading_screen_stem = {},
        .layout = cli::ResourceLayout::kGame,
    };
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    cli::level::GeneratedLevelImages loose_generated;
    cli::ResourceOutputPlan loose_plan;
    const cli::ResourceAssetId loose_asset = loose_plan.addAsset(cli::ResourceAssetKind::kLevel, "loose.fts");
    REQUIRE(cli::level::addDirectLevelImageOutputs(loose_plan, {}, output, images, loose_generated, loose_asset));
    REQUIRE(loose_plan.resolve(false, false));
    REQUIRE(loose_plan.write(io));
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(readBytes(temp.path() / "map.png"), info) == ARX_OK);
    CHECK(info.width == 3);
    CHECK(info.height == 3);

    output.minimap_stem.path = (temp.path() / "preserved").string();
    cli::level::LevelImageInput game_input;
    game_input.layout = cli::ResourceLayout::kGame;
    cli::level::GeneratedLevelImages game_generated;
    cli::ResourceOutputPlan game_plan;
    const cli::ResourceAssetId game_asset = game_plan.addAsset(cli::ResourceAssetKind::kLevel, "level:1");
    REQUIRE(cli::level::addDirectLevelImageOutputs(game_plan, game_input, output, images, game_generated, game_asset));
    REQUIRE(game_plan.resolve(false, false));
    REQUIRE(game_plan.write(io));
    CHECK(readBytes(temp.path() / "preserved.bmp") == minimap);
  }

  TEST_CASE("Direct game Level images are reprojected and rendered to game dimensions") {
    TemporaryDirectory temp;
    cli::level::LoadedLevelImages images;
    REQUIRE(cli::media::prepareImage(makeSolidTestBmp(2, 1), images.minimap.image) == ARX_OK);
    REQUIRE(cli::media::prepareImage(makeTestBmp(), images.loading_screen) == ARX_OK);
    images.minimap.projection_offset = {-25.0f, 0.0f};
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    cli::level::LevelImageInput input;
    cli::level::LevelImageOutput normal{
        .minimap_enabled = true,
        .loading_screen_enabled = true,
        .minimap_stem = {.path = (temp.path() / "map9").string(), .address = cli::PathAddress::kAbsolute},
        .loading_screen_stem = {.path = (temp.path() / "loading9").string(), .address = cli::PathAddress::kAbsolute},
        .layout = cli::ResourceLayout::kGame,
        .level = 9,
    };
    cli::level::GeneratedLevelImages normal_generated;
    cli::ResourceOutputPlan normal_plan;
    const cli::ResourceAssetId normal_asset = normal_plan.addAsset(cli::ResourceAssetKind::kLevel, "level:9");
    REQUIRE(cli::level::addDirectLevelImageOutputs(normal_plan, input, normal, images, normal_generated, normal_asset));
    REQUIRE(normal_plan.resolve(false, false));
    REQUIRE(normal_plan.write(io));

    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(readBytes(temp.path() / "map9.png"), info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 3);
    CHECK(info.height == 1);
    REQUIRE(pistoris::binary::inspectEncodedImage(readBytes(temp.path() / "loading9.png"), info) == ARX_OK);
    CHECK(info.width == 320);
    CHECK(info.height == 390);

    cli::level::LevelImageOutput fullscreen{
        .loading_screen_enabled = true,
        .minimap_stem = {},
        .loading_screen_stem = {.path = (temp.path() / "loading10").string(), .address = cli::PathAddress::kAbsolute},
        .layout = cli::ResourceLayout::kGame,
        .level = 10,
    };
    cli::level::GeneratedLevelImages fullscreen_generated;
    cli::ResourceOutputPlan fullscreen_plan;
    const cli::ResourceAssetId fullscreen_asset = fullscreen_plan.addAsset(cli::ResourceAssetKind::kLevel, "level:10");
    REQUIRE(cli::level::addDirectLevelImageOutputs(
        fullscreen_plan, input, fullscreen, images, fullscreen_generated, fullscreen_asset));
    REQUIRE(fullscreen_plan.resolve(false, false));
    REQUIRE(fullscreen_plan.write(io));

    REQUIRE(pistoris::binary::inspectEncodedImage(readBytes(temp.path() / "loading10.png"), info) == ARX_OK);
    CHECK(info.width == 640);
    CHECK(info.height == 480);
  }

  TEST_CASE("Intermediate Level images render to loose PNG sidecars") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> image = makeTestBmp();
    pistoris::Level level = textureLevel("graph/obj3d/textures/stone");
    REQUIRE(level.setMinimap({image.data(), image.size()}, {{0.0f, 0.0f}, {25.0f, 25.0f}}) == ARX_OK);
    REQUIRE(level.setLoadingScreen({image.data(), image.size()}) == ARX_OK);

    cli::level::LevelImageOutput output{
        .minimap_enabled = true,
        .loading_screen_enabled = true,
        .minimap_stem = {.path = (temp.path() / "level[map]").string(), .address = cli::PathAddress::kAbsolute},
        .loading_screen_stem = {.path = (temp.path() / "level[loading]").string(),
                                .address = cli::PathAddress::kAbsolute},
    };
    cli::level::GeneratedLevelImages generated;
    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kLevel, "level.glb");
    REQUIRE(cli::level::addIntermediateLevelImageOutputs(plan, output, level, generated, asset));
    REQUIRE(plan.resolve(false, false));
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});
    REQUIRE(plan.write(io));

    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(readBytes(temp.path() / "level[map][offset_0_24].png"), info) ==
            ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
    REQUIRE(pistoris::binary::inspectEncodedImage(readBytes(temp.path() / "level[loading].png"), info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 1);
    CHECK(info.height == 1);
  }

  TEST_CASE("Texture hydration rejects mismatched source projections") {
    pistoris::Level level = textureLevel("graph/obj3d/textures/stone");
    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    const std::vector<std::string> sources = {"stone.bmp"};

    CHECK_FALSE(cli::loadTextureImages(level, io, {}, sources));
  }

  TEST_CASE("Sound hydration rejects invalid source indices") {
    pistoris::Animation animation;
    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    const std::vector<pistoris::SoundSourceReference> sources = {{0, "sound.wav"}};

    CHECK_FALSE(cli::loadSoundData(animation, io, {}, sources));
  }

  TEST_CASE("Mounted resources resolve path components case insensitively") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> expected = {1, 2, 3, 4};
    writeBytes(temp.path() / "GrApH" / "Obj3D" / "Textures" / "Stone.PNG", expected);

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()), temp.path().string());
    std::vector<std::uint8_t> actual;
    CHECK(io.readResource(R"(GRAPH\OBJ3D\TEXTURES\STONE.PNG)", actual) == cli::ResourceReadResult::kSuccess);
    CHECK(actual == expected);
    CHECK(io.readResource("graph/obj3d/textures/missing.png", actual) == cli::ResourceReadResult::kNotFound);
    CHECK(io.readResource("../stone.png", actual) == cli::ResourceReadResult::kInvalidPath);
  }

  TEST_CASE("Earlier mount wins for the same texture extension") {
    TemporaryDirectory temp;
    const std::filesystem::path high = temp.path() / "high";
    const std::filesystem::path low = temp.path() / "low";
    const std::vector<std::uint8_t> high_image = makeTestBmp(255, 0, 0);
    const std::vector<std::uint8_t> low_image = makeTestBmp(0, 0, 255);
    writeBytes(high / "Graph" / "Obj3D" / "Textures" / "Stone.BMP", high_image);
    writeBytes(low / "graph" / "obj3d" / "textures" / "stone.bmp", low_image);

    pistoris::Level level = textureLevel("GRAPH/OBJ3D/TEXTURES/STONE");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(high, low));
    cli::loadTextureImages(level, io, {});

    CHECK(test::texture(level, 0).encoded_image == high_image);
    CHECK(test::texture(level, 1).encoded_image.empty());
  }

  TEST_CASE("Resource enumeration merges mounts with earlier precedence and bounded depth") {
    TemporaryDirectory temp;
    const std::filesystem::path high = temp.path() / "high";
    const std::filesystem::path low = temp.path() / "low";
    writeBytes(high / "Graph" / "Levels" / "Level2" / "Level2.DLF", {2});
    writeBytes(low / "graph" / "levels" / "level2" / "level2.dlf", {20});
    writeBytes(low / "graph" / "levels" / "level3" / "level3.dlf", {3});
    writeBytes(low / "graph" / "levels" / "level3" / "nested" / "ignored.dlf", {4});

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(high, low));
    std::vector<std::string> resources;
    REQUIRE(io.enumerateResources("graph/levels", 2, resources) == cli::ResourceEnumerationResult::kSuccess);
    REQUIRE(resources.size() == 2);
    CHECK(std::find(resources.begin(), resources.end(), "graph/levels/Level2/Level2.DLF") != resources.end());
    CHECK(std::find(resources.begin(), resources.end(), "graph/levels/level3/level3.dlf") != resources.end());

    REQUIRE(io.enumerateResources("graph/levels", 1, resources) == cli::ResourceEnumerationResult::kSuccess);
    CHECK(resources.empty());
    REQUIRE(io.enumerateResources("graph/missing", 2, resources) == cli::ResourceEnumerationResult::kSuccess);
    CHECK(resources.empty());
    CHECK(io.enumerateResources("../graph", 2, resources) == cli::ResourceEnumerationResult::kInvalidPath);
  }

  TEST_CASE("Texture extension priority applies across all mounts") {
    TemporaryDirectory temp;
    const std::filesystem::path high = temp.path() / "high";
    const std::filesystem::path low = temp.path() / "low";
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(high / "graph" / "obj3d" / "textures" / "stone.tga", makeTestTga());
    writeBytes(low / "graph" / "obj3d" / "textures" / "stone.bmp", bmp);

    pistoris::Level level = textureLevel("graph/obj3d/textures/stone");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(high, low));
    cli::loadTextureImages(level, io, {});

    CHECK(test::texture(level, 0).encoded_image == bmp);
  }

  TEST_CASE("Mounted texture loading preserves exact logical texture identities") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> mapped = makeTestBmp(255, 0, 0);
    const std::vector<std::uint8_t> collision = makeTestBmp(0, 0, 255);
    const std::filesystem::path texture_folder = temp.path() / "graph" / "obj3d" / "textures";
    writeBytes(texture_folder / "npc_human__base_hero_head.bmp", mapped);
    writeBytes(texture_folder / "npc_human_base_hero_head.bmp", collision);

    pistoris::Level level = textureLevel("graph/obj3d/textures/npc_human__base_hero_head");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::loadTextureImages(level, io, {});

    CHECK(test::texture(level, 0).encoded_image == mapped);
  }

  TEST_CASE("Mounted item icons use their game resource path") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> icon = makeTestBmp(255, 0, 0);
    writeBytes(temp.path() / "graph" / "obj3d" / "interactive" / "items" / "weapons" / "sword" / "sword[icon].bmp",
               icon);

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::ClassifiedPath input{
        .path = "game/graph/obj3d/interactive/items/weapons/sword/sword.ftl",
        .location = {.path = "game/graph/obj3d/interactive/items/weapons/sword/sword.ftl",
                     .address = cli::PathAddress::kMountRelative},
        .buffer = {},
        .facts = {.format = cli::Format::kFtl, .kind = cli::PayloadKind::kFtl},
        .positional_index = 0,
        .resource_kind = ARX_RESOURCE_KIND_MODEL,
        .layout = cli::ResourceLayout::kGame,
    };
    cli::InventoryIconInput descriptor;
    REQUIRE(cli::resolveInventoryIconInput(input, {}, io, descriptor));
    CHECK(descriptor.enabled);
    CHECK(descriptor.path == "graph/obj3d/interactive/items/weapons/sword/sword[icon]");

    cli::LoadedInventoryIcon loaded;
    REQUIRE(cli::readInventoryIcon(io, descriptor, loaded));
    CHECK(loaded.encoded == icon);
    CHECK_FALSE(loaded.resolved_path.empty());

    input.path = "game/graph/obj3d/interactive/items/weapons/sword/tweaks/golden.ftl";
    REQUIRE(cli::resolveInventoryIconInput(input, {}, io, descriptor));
    CHECK(descriptor.enabled);
    CHECK(descriptor.path == "graph/obj3d/interactive/items/weapons/sword/sword[icon]");

    input.path = "game/graph/obj3d/interactive/npc/human_base/human_base.ftl";
    REQUIRE(cli::resolveInventoryIconInput(input, {}, io, descriptor));
    CHECK_FALSE(descriptor.enabled);
  }

  TEST_CASE("Mounted item icon output uses its game resource path") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> icon = makeTestBmp(255, 0, 0);
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()), temp.path().string());

    cli::OutputTarget output;
    output.path = "game/graph/obj3d/interactive/items/weapons/shield/shield.ftl";
    output.address = cli::PathAddress::kMountRelative;
    output.format = cli::Format::kFtl;
    output.layout = cli::ResourceLayout::kGame;
    cli::InventoryIconOutput descriptor;
    REQUIRE(cli::resolveInventoryIconOutput(output, io, descriptor));
    CHECK(descriptor.enabled);
    CHECK(descriptor.item_only);
    CHECK(descriptor.stem.path == "graph/obj3d/interactive/items/weapons/shield/shield[icon]");
    CHECK(descriptor.stem.address == cli::PathAddress::kMountRelative);

    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kModel, output.path);
    REQUIRE(cli::addInventoryIconOutput(plan, descriptor, icon, ARX_IMAGE_FORMAT_BMP, asset));
    REQUIRE(plan.resolve(false, false));
    REQUIRE(plan.write(io));
    CHECK(readBytes(temp.path() / "graph" / "obj3d" / "interactive" / "items" / "weapons" / "shield" /
                    "shield[icon].bmp") == icon);

    output.path = "game/graph/obj3d/interactive/items/weapons/shield/tweaks/golden.ftl";
    REQUIRE(cli::resolveInventoryIconOutput(output, io, descriptor));
    CHECK(descriptor.enabled);
    CHECK(descriptor.stem.path == "graph/obj3d/interactive/items/weapons/shield/shield[icon]");

    output.path = "game/graph/obj3d/interactive/npc/human_base/human_base.ftl";
    REQUIRE(cli::resolveInventoryIconOutput(output, io, descriptor));
    CHECK_FALSE(descriptor.enabled);
    CHECK(descriptor.item_only);
  }

  TEST_CASE("External Model icons use a sibling image") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> icon = makeTestTga();
    writeBytes(temp.path() / "sword[icon].tga", icon);

    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    const std::string model_path = (temp.path() / "sword.glb").string();
    const cli::ClassifiedPath input{
        .path = model_path,
        .location = {.path = model_path, .address = cli::PathAddress::kAbsolute},
        .buffer = {},
        .facts = {.format = cli::Format::kGlb, .kind = cli::PayloadKind::kGlb},
        .positional_index = 0,
        .resource_kind = ARX_RESOURCE_KIND_MODEL,
    };
    cli::InventoryIconInput descriptor;
    REQUIRE(cli::resolveInventoryIconInput(input, {}, io, descriptor));
    CHECK(descriptor.enabled);
    CHECK(descriptor.path == "sword[icon]");

    cli::LoadedInventoryIcon loaded;
    REQUIRE(cli::readInventoryIcon(io, descriptor, loaded));
    CHECK(loaded.encoded == icon);
    CHECK_FALSE(loaded.resolved_path.empty());

    cli::OutputTarget output;
    output.path = (temp.path() / "copy.glb").string();
    output.address = cli::PathAddress::kAbsolute;
    output.format = cli::Format::kGlb;
    cli::InventoryIconOutput output_descriptor;
    REQUIRE(cli::resolveInventoryIconOutput(output, io, output_descriptor));

    cli::ResourceOutputPlan plan;
    const cli::ResourceAssetId asset = plan.addAsset(cli::ResourceAssetKind::kModel, output.path);
    REQUIRE(cli::addInventoryIconOutput(plan, output_descriptor, icon, ARX_IMAGE_FORMAT_TGA, asset));
    REQUIRE(plan.resolve(false, false));
    REQUIRE(plan.write(io));
    CHECK(readBytes(temp.path() / "copy[icon].tga") == icon);
  }

  TEST_CASE("Intermediate Model icons always render as PNG") {
    const std::vector<std::uint8_t> icon = makeTestBmp();
    pistoris::Model model;
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}) == ARX_OK);

    std::vector<std::uint8_t> rendered;
    REQUIRE(cli::projectInventoryIcon(model, {}, rendered));
    CHECK_FALSE(rendered.empty());
    ArxImageInfo info{};
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.format == ARX_IMAGE_FORMAT_PNG);
    CHECK(info.width == 32);
    CHECK(info.height == 32);

    pistoris::Model::InventoryIconSetOptions set_options;
    set_options.width_slots = 2;
    set_options.height_slots = 1;
    REQUIRE(model.setInventoryIcon({icon.data(), icon.size()}, set_options) == ARX_OK);
    REQUIRE(cli::projectInventoryIcon(model, {}, rendered));
    REQUIRE(pistoris::binary::inspectEncodedImage(rendered, info) == ARX_OK);
    CHECK(info.width == 64);
    CHECK(info.height == 32);
  }

  TEST_CASE("Mounted texture loading is independent of the Level input format") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.bmp", bmp);

    pistoris::Level authored = textureLevel("graph/obj3d/textures/stone");
    std::vector<std::uint8_t> glb;
    REQUIRE(authored.exportGlb(glb) == ARX_OK);
    pistoris::Level imported;
    REQUIRE(pistoris::Level::importGlb(imported, glb) == ARX_OK);
    REQUIRE(test::texture(imported, 0).encoded_image.empty());

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::loadTextureImages(imported, io, {});

    CHECK(test::texture(imported, 0).encoded_image == bmp);
  }

  TEST_CASE("Mounted texture loading preserves game filename punctuation") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "wall [metal] (old)&new.bmp", bmp);

    pistoris::Level level = textureLevel("graph/obj3d/textures/wall [metal] (old)&new");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::loadTextureImages(level, io, {});

    CHECK(test::texture(level, 0).encoded_image == bmp);
  }

  TEST_CASE("Mounted texture loading covers every retained texture") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> used = makeTestBmp(255, 0, 0);
    const std::vector<std::uint8_t> unused = makeTestBmp(0, 0, 255);
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.bmp", used);
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "unused.bmp", unused);

    pistoris::Level level = textureLevel("graph/obj3d/textures/stone");
    REQUIRE(test::setTexture(level, 1, "graph/obj3d/textures/unused") == ARX_OK);
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::loadTextureImages(level, io, {});

    CHECK(test::texture(level, 0).encoded_image == used);
    CHECK(test::texture(level, 1).encoded_image == unused);
  }

  TEST_CASE("Invalid first existing candidate does not fall through") {
    TemporaryDirectory temp;
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.png", {1, 2, 3});
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.bmp", makeTestBmp());

    pistoris::Level level = textureLevel("graph/obj3d/textures/stone");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::loadTextureImages(level, io, {});

    CHECK(test::texture(level, 0).encoded_image.empty());
  }

  TEST_CASE("Format texture sources resolve relative to their input base") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "materials" / "stone.bmp", bmp);

    pistoris::Level level = textureLevel("custom/path/stone");
    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    cli::TextureInput input{
        .use_format_sources = true,
        .source_lookup = cli::ImageLookupMode::kExact,
        .source_base = {.path = temp.path().string(), .address = cli::PathAddress::kAbsolute},
    };
    const std::vector<std::string> sources = {"materials/stone.bmp", {}};
    cli::loadTextureImages(level, io, input, sources);

    CHECK(test::texture(level, 0).encoded_image == bmp);
  }

  TEST_CASE("Format texture sources preserve resource directories") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> first = makeTestBmp(255, 0, 0);
    const std::vector<std::uint8_t> second_image = makeTestBmp(0, 0, 255);
    writeBytes(temp.path() / "first" / "stone.bmp", first);
    writeBytes(temp.path() / "second" / "stone.bmp", second_image);

    pistoris::Level level = textureLevel("first/stone");
    REQUIRE(test::setTexture(level, 1, "second/stone") == ARX_OK);
    pistoris::Face second = test::face(level, 0);
    second.texture = 1;
    REQUIRE(test::addFace(level, second, 0) != pistoris::kInvalidFaceIndex);

    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    cli::TextureInput input{
        .use_format_sources = true,
        .source_lookup = cli::ImageLookupMode::kExact,
        .source_base = {.path = temp.path().string(), .address = cli::PathAddress::kAbsolute},
    };
    const std::vector<std::string> sources = {"first/stone.bmp", "second/stone.bmp"};
    cli::loadTextureImages(level, io, input, sources);

    CHECK(test::texture(level, 0).encoded_image == first);
    CHECK(test::texture(level, 1).encoded_image == second_image);
  }

  TEST_CASE("Missing format texture source falls back to the game resource") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.bmp", bmp);

    pistoris::Level level = textureLevel("graph/obj3d/textures/stone");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::TextureInput input{
        .use_format_sources = true,
        .source_lookup = cli::ImageLookupMode::kExact,
        .source_base = {.path = (temp.path() / "project").string(), .address = cli::PathAddress::kAbsolute},
    };
    const std::vector<std::string> sources = {"missing.bmp", {}};
    cli::loadTextureImages(level, io, input, sources);

    CHECK(test::texture(level, 0).encoded_image == bmp);
  }

  TEST_CASE("Direct native texture lookup collapses extension aliases") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.png", bmp);

    pistoris::ftl::Data ftl;
    ftl.texture_containers.resize(2);
    std::memcpy(
        ftl.texture_containers[0].filename, "graph/obj3d/textures/stone.bmp", sizeof("graph/obj3d/textures/stone.bmp"));
    std::memcpy(
        ftl.texture_containers[1].filename, "graph/obj3d/textures/stone.tga", sizeof("graph/obj3d/textures/stone.tga"));

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    std::vector<pistoris::NativeTextureFile> files;
    cli::loadNativeTextureFiles(ftl, io, {}, files);

    REQUIRE(files.size() == 1);
    CHECK(files[0].resource_path == "graph/obj3d/textures/stone.png");
    CHECK(files[0].encoded_image == bmp);
  }

  TEST_CASE("Resource output uses the independent write mount") {
    TemporaryDirectory temp;
    const std::filesystem::path first = temp.path() / "first";
    const std::filesystem::path second = temp.path() / "second";
    const std::filesystem::path output = temp.path() / "output";
    REQUIRE(std::filesystem::create_directories(first));
    REQUIRE(std::filesystem::create_directories(second));

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(first, second), output.string());
    REQUIRE(io.valid());
    const std::vector<std::uint8_t> bytes = {5, 4, 3, 2, 1};
    REQUIRE(io.writeResource("game/graph/test.bin", bytes.data(), bytes.size()));
    CHECK(std::filesystem::is_regular_file(output / "game" / "graph" / "test.bin"));
    CHECK_FALSE(std::filesystem::exists(first / "game" / "graph" / "test.bin"));
    CHECK_FALSE(std::filesystem::exists(second / "game" / "graph" / "test.bin"));
  }

  TEST_CASE("Write mount is not searched for reads") {
    TemporaryDirectory temp;
    const std::filesystem::path reads = temp.path() / "reads";
    const std::filesystem::path writes = temp.path() / "writes";
    REQUIRE(std::filesystem::create_directories(reads));
    writeBytes(writes / "graph" / "write-only.bin", {1, 2, 3});

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(reads), writes.string());
    std::vector<std::uint8_t> bytes;
    CHECK(io.readResource("graph/write-only.bin", bytes) == cli::ResourceReadResult::kNotFound);
  }

  TEST_CASE("Output locations distinguish mount-relative and absolute paths") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()), temp.path().string());
    cli::OutputLocation location;
    std::string error;
    REQUIRE(io.resolveOutputLocation("./proj/result.bin", location, error));
    CHECK(location.address == cli::OutputAddress::kMountRelative);
    CHECK(location.path == "proj/result.bin");

    const std::filesystem::path absolute = temp.path() / "absolute.bin";
    REQUIRE(io.resolveOutputLocation(absolute.string(), location, error));
    CHECK(location.address == cli::OutputAddress::kAbsolute);
    CHECK(std::filesystem::path(location.path).lexically_normal() == absolute.lexically_normal());
  }

  TEST_CASE("Path component queries support mounted and absolute locations") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
    std::string error;
    bool found = false;

    const cli::PathLocation mounted{"game/graph/obj3d/interactive/NPC/human/human.ftl",
                                    cli::PathAddress::kMountRelative};
    REQUIRE(io.hasPathComponent(mounted, "npc", found, error));
    CHECK(found);
    REQUIRE(io.hasPathComponent(mounted, "fix_inter", found, error));
    CHECK_FALSE(found);

    cli::PathLocation absolute;
    absolute.path = (temp.path() / "Graph" / "Obj3D" / "Interactive" / "Npc" / "human.ftl").string();
    absolute.address = cli::PathAddress::kAbsolute;
    REQUIRE(io.hasPathComponent(absolute, "NPC", found, error));
    CHECK(found);
    CHECK_FALSE(io.hasPathComponent(absolute, "bad/component", found, error));
  }

  TEST_CASE("Resource output preserves texture extension variants") {
    TemporaryDirectory temp;
    const std::filesystem::path old_path = temp.path() / "graph" / "textures" / "stone.bmp";
    writeBytes(old_path, makeTestBmp());
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()), temp.path().string());

    const std::vector<std::uint8_t> replacement = makeTestTga();
    REQUIRE(io.writeResource("graph/textures/stone.tga", replacement.data(), replacement.size()));
    CHECK(std::filesystem::is_regular_file(old_path));
    const std::filesystem::path new_path = temp.path() / "graph" / "textures" / "stone.tga";
    CHECK(std::filesystem::is_regular_file(new_path));
    CHECK(readBytes(new_path) == replacement);
  }

  TEST_CASE("Absolute output creates directories and preserves texture extension variants") {
    TemporaryDirectory temp;
    const std::filesystem::path directory = temp.path() / "absolute" / "textures";
    const std::filesystem::path old_path = directory / "stone.bmp";
    writeBytes(old_path, makeTestBmp());
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));

    const std::vector<std::uint8_t> replacement = makeTestTga();
    const std::filesystem::path requested = directory / "stone.tga";
    REQUIRE(io.writeFile(requested.string().c_str(), replacement.data(), replacement.size()));

    CHECK(std::filesystem::is_regular_file(old_path));
    CHECK(readBytes(requested) == replacement);
  }

  TEST_CASE("Invalid write mount does not fall through to read mounts") {
    TemporaryDirectory temp;
    const std::filesystem::path invalid = temp.path() / "not-a-directory";
    const std::filesystem::path second = temp.path() / "second";
    writeBytes(invalid, {1});
    REQUIRE(std::filesystem::create_directories(second));

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(second), invalid.string());
    CHECK_FALSE(io.valid());
    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    CHECK_FALSE(io.writeResource("graph/test.bin", bytes.data(), bytes.size()));
    CHECK_FALSE(std::filesystem::exists(second / "graph" / "test.bin"));
  }

  TEST_CASE("Dry run resource output creates no directories") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, true, mountPaths(temp.path()), temp.path().string());
    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    REQUIRE(io.writeResource("game/new/path/test.bin", bytes.data(), bytes.size()));
    CHECK_FALSE(std::filesystem::exists(temp.path() / "game"));
  }

  TEST_CASE("Absolute output creates parent directories") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    const std::filesystem::path output = temp.path() / "absolute" / "nested" / "test.bin";
    REQUIRE(io.writeFile(output.string().c_str(), bytes.data(), bytes.size()));
    CHECK(std::filesystem::is_regular_file(output));
  }

  TEST_CASE("Dry run absolute output creates no directories") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, true, mountPaths(temp.path()));
    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    const std::filesystem::path output = temp.path() / "absolute" / "nested" / "test.bin";
    REQUIRE(io.writeFile(output.string().c_str(), bytes.data(), bytes.size()));
    CHECK_FALSE(std::filesystem::exists(temp.path() / "absolute"));
  }

  TEST_CASE("Prospective write mount is created on resource output") {
    TemporaryDirectory temp;
    const std::filesystem::path mount = temp.path() / "prospective";
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {}, mount.string());
    REQUIRE(io.valid());

    const std::vector<std::uint8_t> bytes = {9, 8, 7};
    REQUIRE(io.writeResource("game/graph/test.bin", bytes.data(), bytes.size()));
    CHECK(std::filesystem::is_regular_file(mount / "game" / "graph" / "test.bin"));
  }

  TEST_CASE("Dry run accepts prospective write mount without creating it") {
    TemporaryDirectory temp;
    const std::filesystem::path mount = temp.path() / "prospective";
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, true, {}, mount.string());
    REQUIRE(io.valid());

    const std::vector<std::uint8_t> bytes = {9, 8, 7};
    REQUIRE(io.writeResource("game/graph/test.bin", bytes.data(), bytes.size()));
    CHECK_FALSE(std::filesystem::exists(mount));
  }

  TEST_CASE("Mounts accept trailing directory separators") {
    TemporaryDirectory temp;
    const auto check_separator = [&](char separator) {
      std::string existing = temp.path().string();
      existing.push_back(separator);
      cli::IoService readable(cli::OverwriteMode::kAlwaysYes, true, {existing});
      CHECK(readable.valid());
      CHECK(readable.hasReadMounts());

      const std::filesystem::path prospective_path = temp.path() / "prospective";
      std::string prospective = prospective_path.string();
      prospective.push_back(separator);
      cli::IoService write_only(cli::OverwriteMode::kAlwaysYes, true, {}, prospective);
      CHECK(write_only.valid());
      CHECK_FALSE(std::filesystem::exists(prospective_path));
    };

    check_separator('/');
#ifdef _WIN32
    check_separator('\\');
#endif
  }

  TEST_CASE("Automatic mounts retain explicit read mounts") {
    TemporaryDirectory temp;
    writeBytes(temp.path() / "graph" / "explicit.bin", {4, 2});
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()), {}, true);
    REQUIRE(io.valid());
    std::vector<std::uint8_t> bytes;
    CHECK(io.readResource("graph/explicit.bin", bytes) == cli::ResourceReadResult::kSuccess);
    CHECK(bytes == std::vector<std::uint8_t>{4, 2});
  }

  TEST_CASE("Automatic mounts retain the implicit current directory unless explicit mounts are supplied") {
    TemporaryDirectory temp;
    const std::filesystem::path current = temp.path() / "current";
    const std::filesystem::path explicit_mount = temp.path() / "explicit";
    const std::string local_resource =
        "graph/arx-pistoris-tests/" + temp.path().filename().string() + "-implicit-mount.bin";
    writeBytes(current / local_resource, {1});
    writeBytes(explicit_mount / "graph" / "explicit.bin", {2});
    ScopedCurrentDirectory scoped_current(current);

    std::vector<std::uint8_t> bytes;
    cli::IoService automatic(cli::OverwriteMode::kAlwaysYes, false, {}, {}, true);
    REQUIRE(automatic.valid());
    CHECK(automatic.readResource(local_resource, bytes) == cli::ResourceReadResult::kSuccess);
    CHECK(bytes == std::vector<std::uint8_t>{1});

    cli::IoService explicit_automatic(cli::OverwriteMode::kAlwaysYes, false, mountPaths(explicit_mount), {}, true);
    REQUIRE(explicit_automatic.valid());
    CHECK(explicit_automatic.readResource(local_resource, bytes) != cli::ResourceReadResult::kSuccess);
    CHECK(explicit_automatic.readResource("graph/explicit.bin", bytes) == cli::ResourceReadResult::kSuccess);
    CHECK(bytes == std::vector<std::uint8_t>{2});
  }

#ifndef _WIN32
  TEST_CASE("Automatic mounts follow explicit game and unpacked priority") {
    TemporaryDirectory temp;
    const std::filesystem::path data_home = temp.path() / "xdg";
    const std::filesystem::path game = data_home / "arx";
    const std::filesystem::path unpacked = game / "unpacked";
    const std::filesystem::path explicit_mount = temp.path() / "explicit";
    ScopedEnvironmentOverride data_home_override("XDG_DATA_HOME", data_home.string().c_str());
    ScopedEnvironmentOverride home_override("HOME", (temp.path() / "home").string().c_str());

    writeBytes(explicit_mount / "graph" / "shared.bin", {1});
    writeBytes(game / "graph" / "shared.bin", {2});
    writeBytes(unpacked / "graph" / "shared.bin", {3});
    writeBytes(game / "graph" / "default.bin", {4});
    writeBytes(unpacked / "graph" / "default.bin", {5});
    writeBytes(unpacked / "graph" / "unpacked.bin", {6});

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(explicit_mount), {}, true);
    REQUIRE(io.valid());
    std::vector<std::uint8_t> bytes;
    CHECK(io.readResource("graph/shared.bin", bytes) == cli::ResourceReadResult::kSuccess);
    CHECK(bytes == std::vector<std::uint8_t>{1});
    CHECK(io.readResource("graph/default.bin", bytes) == cli::ResourceReadResult::kSuccess);
    CHECK(bytes == std::vector<std::uint8_t>{4});
    CHECK(io.readResource("graph/unpacked.bin", bytes) == cli::ResourceReadResult::kSuccess);
    CHECK(bytes == std::vector<std::uint8_t>{6});
  }
#endif

  TEST_CASE("Classified inputs retain primary resource layout") {
    TemporaryDirectory temp;
    writeBytes(temp.path() / "model.ftl", {0xff});
    const std::string json_text = R"({"$schema":"https://arx-tools.github.io/schemas/ftl.schema.json"})";
    writeBytes(temp.path() / "model.json", std::vector<std::uint8_t>(json_text.begin(), json_text.end()));
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));

    std::vector<cli::ClassifiedPath> inputs;
    const char* mounted[] = {"model.ftl"};
    REQUIRE(cli::loadClassifiedInputs(mounted, io, inputs));
    REQUIRE(inputs.size() == 1);
    CHECK(inputs[0].layout == cli::ResourceLayout::kGame);

    const std::string absolute_path = (temp.path() / "model.ftl").string();
    const char* absolute[] = {absolute_path.c_str()};
    REQUIRE(cli::loadClassifiedInputs(absolute, io, inputs));
    CHECK(inputs[0].layout == cli::ResourceLayout::kLoose);

    const char* json[] = {"model.json"};
    REQUIRE(cli::loadClassifiedInputs(json, io, inputs));
    CHECK(inputs[0].layout == cli::ResourceLayout::kLoose);
  }

  TEST_CASE("Read and write mounts with an existing file ancestor are invalid even in dry run") {
    TemporaryDirectory temp;
    const std::filesystem::path file = temp.path() / "not-a-directory";
    writeBytes(file, {1});

    cli::IoService invalid_read(cli::OverwriteMode::kAlwaysYes, true, mountPaths(file / "child"));
    CHECK_FALSE(invalid_read.valid());
    cli::IoService invalid_write(cli::OverwriteMode::kAlwaysYes, true, {}, (file / "child").string());
    CHECK_FALSE(invalid_write.valid());
  }

  TEST_CASE("Portable reserved resource components are rejected") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()), temp.path().string());
    REQUIRE(io.valid());

    const std::vector<std::uint8_t> bytes = {1};
    CHECK_FALSE(io.writeResource("graph/NUL/test.bin", bytes.data(), bytes.size()));
    CHECK_FALSE(std::filesystem::exists(temp.path() / "graph"));
  }

  TEST_CASE("Raw writes replace through a temporary sibling") {
    TemporaryDirectory temp;
    const std::filesystem::path output = temp.path() / "result.bin";
    writeBytes(output, {1, 2, 3});
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));

    const std::vector<std::uint8_t> replacement = {4, 5};
    REQUIRE(io.writeFile(output.string().c_str(), replacement.data(), replacement.size()));
    CHECK(readBytes(output) == replacement);
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(temp.path())) {
      CHECK(entry.path().filename().string().find(".arx-pistor.tmp.") == std::string::npos);
    }
  }

  TEST_CASE("Writing through a final symlink preserves the symlink") {
    TemporaryDirectory temp;
    const std::filesystem::path target = temp.path() / "target.bin";
    const std::filesystem::path link = temp.path() / "link.bin";
    writeBytes(target, {1});
    std::error_code ec;
    std::filesystem::create_symlink(target, link, ec);
    if (ec) return;

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
    const std::vector<std::uint8_t> replacement = {2, 3};
    REQUIRE(io.writeFile(link.string().c_str(), replacement.data(), replacement.size()));
    CHECK(std::filesystem::is_symlink(std::filesystem::symlink_status(link)));
    CHECK(readBytes(target) == replacement);
  }
}
