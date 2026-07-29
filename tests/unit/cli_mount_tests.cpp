// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/indices.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "../../cli/io/files.h"
#include "../../cli/io/service.h"
#include "../../cli/routes/level/invocation.h"
#include "../../cli/routes/level/mounted_textures.h"
#include "image_helpers.h"
#include "io/path_location.h"
#include "io/policy.h"
#include "level_add_helpers.h"
#include "modules/geometry.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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

void writeBytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  REQUIRE_FALSE(ec);
  std::ofstream output(path, std::ios::binary);
  REQUIRE(output.good());
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

pistoris::Level textureLevel(std::string path) {
  pistoris::Level level;
  REQUIRE(test::addRoom(level, {"room"}) == 0);

  test::MeshSnapshot mesh;
  mesh.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
  pistoris::Face face;
  face.corners[0] = {0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f};
  face.corners[1] = {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f};
  face.corners[2] = {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f};
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
  TEST_CASE("Mounted resources resolve path components case insensitively") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> expected = {1, 2, 3, 4};
    writeBytes(temp.path() / "GrApH" / "Obj3D" / "Textures" / "Stone.PNG", expected);

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
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

    pistoris::Level level = textureLevel(R"(GRAPH\OBJ3D\TEXTURES\STONE.JPG)");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(high, low));
    cli::level::loadMountedTextureImages(level, io);

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

    pistoris::Level level = textureLevel("graph/obj3d/textures/stone.jpg");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(high, low));
    cli::level::loadMountedTextureImages(level, io);

    CHECK(test::texture(level, 0).encoded_image == bmp);
  }

  TEST_CASE("Mounted texture loading resolves library aliases to exact game paths") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> mapped = makeTestBmp(255, 0, 0);
    const std::vector<std::uint8_t> collision = makeTestBmp(0, 0, 255);
    const std::filesystem::path texture_folder = temp.path() / "graph" / "obj3d" / "textures";
    writeBytes(texture_folder / "npc_human__base_hero_head.bmp", mapped);
    writeBytes(texture_folder / "npc_human_base_hero_head.bmp", collision);

    pistoris::Level level = textureLevel("graph/obj3d/textures/npc_human_base_hero_head_1.jpg");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::level::loadMountedTextureImages(level, io);

    CHECK(test::texture(level, 0).encoded_image == mapped);
  }

  TEST_CASE("Mounted texture loading is independent of the Level input format") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.bmp", bmp);

    pistoris::Level authored = textureLevel("graph/obj3d/textures/stone.jpg");
    std::vector<std::uint8_t> glb;
    REQUIRE(authored.exportGlb(glb) == ARX_OK);
    pistoris::Level imported;
    REQUIRE(pistoris::Level::fromGlb(imported, glb) == ARX_OK);
    REQUIRE(cli::level::hasMissingReferencedTextureImages(imported));

    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::level::loadMountedTextureImages(imported, io);

    CHECK_FALSE(cli::level::hasMissingReferencedTextureImages(imported));
    CHECK(test::texture(imported, 0).encoded_image == bmp);
  }

  TEST_CASE("Invalid first existing candidate does not fall through") {
    TemporaryDirectory temp;
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.png", {1, 2, 3});
    writeBytes(temp.path() / "graph" / "obj3d" / "textures" / "stone.bmp", makeTestBmp());

    pistoris::Level level = textureLevel("graph/obj3d/textures/stone.tga");
    cli::IoService io(cli::OverwriteMode::kAsk, false, mountPaths(temp.path()));
    cli::level::loadMountedTextureImages(level, io);

    CHECK(test::texture(level, 0).encoded_image.empty());
  }

  TEST_CASE("Flat texture folders ignore resource directories") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> bmp = makeTestBmp();
    writeBytes(temp.path() / "stone.bmp", bmp);

    pistoris::Level level = textureLevel("custom/path/stone.jpg");
    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    cli::level::TextureInput input{
        .mode = cli::level::TextureLookupMode::kFlatFolder,
        .folder = {.path = temp.path().string(), .address = cli::PathAddress::kAbsolute},
    };
    cli::level::loadMountedTextureImages(level, io, input);

    CHECK(test::texture(level, 0).encoded_image == bmp);
  }

  TEST_CASE("Flat texture folders reject ambiguous logical texture stems") {
    TemporaryDirectory temp;
    writeBytes(temp.path() / "stone.bmp", makeTestBmp());

    pistoris::Level level = textureLevel("first/stone.jpg");
    REQUIRE(test::setTexture(level, 1, "second/stone.png") == ARX_OK);
    pistoris::Face second = test::face(level, 0);
    second.texture = 1;
    REQUIRE(test::addFace(level, second, 0) != pistoris::kInvalidFaceIndex);

    cli::IoService io(cli::OverwriteMode::kAsk, false, {});
    cli::level::TextureInput input{
        .mode = cli::level::TextureLookupMode::kFlatFolder,
        .folder = {.path = temp.path().string(), .address = cli::PathAddress::kAbsolute},
    };
    cli::level::loadMountedTextureImages(level, io, input);

    CHECK(test::texture(level, 0).encoded_image.empty());
    CHECK(test::texture(level, 1).encoded_image.empty());
  }

  TEST_CASE("Resource output writes only through the first mount") {
    TemporaryDirectory temp;
    const std::filesystem::path first = temp.path() / "first";
    const std::filesystem::path second = temp.path() / "second";
    REQUIRE(std::filesystem::create_directories(first));
    REQUIRE(std::filesystem::create_directories(second));

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(first, second));
    REQUIRE(io.valid());
    const std::vector<std::uint8_t> bytes = {5, 4, 3, 2, 1};
    REQUIRE(io.writeResource("game/graph/test.bin", bytes.data(), bytes.size()));
    CHECK(std::filesystem::is_regular_file(first / "game" / "graph" / "test.bin"));
    CHECK_FALSE(std::filesystem::exists(second / "game" / "graph" / "test.bin"));
  }

  TEST_CASE("Output locations distinguish mount-relative and absolute paths") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
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

  TEST_CASE("Resource output preserves texture extension variants") {
    TemporaryDirectory temp;
    const std::filesystem::path old_path = temp.path() / "graph" / "textures" / "stone.bmp";
    writeBytes(old_path, makeTestBmp());
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));

    const std::vector<std::uint8_t> replacement = makeTestTga();
    REQUIRE(io.writeResource("graph/textures/stone.tga", replacement.data(), replacement.size()));
    CHECK(std::filesystem::is_regular_file(old_path));
    const std::filesystem::path new_path = temp.path() / "graph" / "textures" / "stone.tga";
    CHECK(std::filesystem::is_regular_file(new_path));
    std::vector<std::uint8_t> actual;
    REQUIRE(readFile(new_path.string().c_str(), actual));
    CHECK(actual == replacement);
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
    std::vector<std::uint8_t> actual;
    REQUIRE(readFile(requested.string().c_str(), actual));
    CHECK(actual == replacement);
  }

  TEST_CASE("Resource output does not fall through an invalid first mount") {
    TemporaryDirectory temp;
    const std::filesystem::path invalid = temp.path() / "not-a-directory";
    const std::filesystem::path second = temp.path() / "second";
    writeBytes(invalid, {1});
    REQUIRE(std::filesystem::create_directories(second));

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(invalid, second));
    CHECK_FALSE(io.valid());
    const std::vector<std::uint8_t> bytes = {1, 2, 3};
    CHECK_FALSE(io.writeResource("graph/test.bin", bytes.data(), bytes.size()));
    CHECK_FALSE(std::filesystem::exists(second / "graph" / "test.bin"));
  }

  TEST_CASE("Dry run resource output creates no directories") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, true, mountPaths(temp.path()));
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

  TEST_CASE("Prospective first mount is created on resource output") {
    TemporaryDirectory temp;
    const std::filesystem::path mount = temp.path() / "prospective";
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(mount));
    REQUIRE(io.valid());

    const std::vector<std::uint8_t> bytes = {9, 8, 7};
    REQUIRE(io.writeResource("game/graph/test.bin", bytes.data(), bytes.size()));
    CHECK(std::filesystem::is_regular_file(mount / "game" / "graph" / "test.bin"));
  }

  TEST_CASE("Dry run accepts prospective mount without creating it") {
    TemporaryDirectory temp;
    const std::filesystem::path mount = temp.path() / "prospective";
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, true, mountPaths(mount));
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
      cli::IoService write_only(cli::OverwriteMode::kAlwaysYes, true, {prospective});
      CHECK(write_only.valid());
      CHECK_FALSE(write_only.hasReadMounts());
      CHECK_FALSE(std::filesystem::exists(prospective_path));
    };

    check_separator('/');
#ifdef _WIN32
    check_separator('\\');
#endif
  }

  TEST_CASE("Mount with an existing file ancestor is invalid even in dry run") {
    TemporaryDirectory temp;
    const std::filesystem::path file = temp.path() / "not-a-directory";
    writeBytes(file, {1});

    cli::IoService io(cli::OverwriteMode::kAlwaysYes, true, mountPaths(file / "child"));
    CHECK_FALSE(io.valid());
  }

  TEST_CASE("Portable reserved resource components are rejected") {
    TemporaryDirectory temp;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, mountPaths(temp.path()));
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
    std::vector<std::uint8_t> actual;
    REQUIRE(readFile(output.string().c_str(), actual));
    CHECK(actual == replacement);
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
    std::vector<std::uint8_t> actual;
    REQUIRE(readFile(target.string().c_str(), actual));
    CHECK(actual == replacement);
  }
}
