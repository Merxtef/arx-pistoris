// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/texture.h"

#include <utility>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/pistoris.hpp"

#include "helpers.h"
#include "image_helpers.h"

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

template <std::size_t N>
void setNativeText(char (&out)[N], std::string_view value) {
  REQUIRE(value.size() < N);
  std::memcpy(out, value.data(), value.size());
  std::memset(out + value.size(), 0, N - value.size());
}

template <std::size_t N>
std::string_view nativeText(const char (&value)[N]) {
  const void* terminator = std::memchr(value, '\0', N);
  return {value, terminator ? static_cast<const char*>(terminator) - value : N};
}

pistoris::dlf::Entity nativeEntity(std::string_view class_path, std::int32_t ident, pistoris::ArxVector3 position = {},
                                   pistoris::ArxAngle angle = {}) {
  pistoris::dlf::Entity result;
  setNativeText(result.class_path, class_path);
  result.ident = ident;
  result.position = position;
  result.angle = angle;
  return result;
}

}  // namespace

TEST_SUITE("cpp_api") {
  TEST_CASE("Metadata") {
    CHECK(pistoris::version() != nullptr);
    CHECK(std::string_view(pistoris::version()).size() > 0);
    CHECK(pistoris::buildTime() != nullptr);
    CHECK(std::string_view(pistoris::buildTime()).size() > 0);
    CHECK(pistoris::errorString(ARX_OK) != nullptr);
    CHECK(std::string_view(pistoris::errorString(ARX_OK)) == "ok");
  }

  TEST_CASE("FtlReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeMinimalFtl();
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(fixture, ftl) == ARX_OK);

    std::vector<std::uint8_t> out;
    CHECK(pistoris::writeFtl(ftl, out) == ARX_OK);
    CHECK(!out.empty());
  }

  TEST_CASE("TeaReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeKeyframeTea();
    pistoris::Tea tea;
    REQUIRE(pistoris::readTea(fixture, tea) == ARX_OK);

    std::vector<std::uint8_t> out;
    CHECK(pistoris::writeTea(tea, out) == ARX_OK);
    CHECK(!out.empty());
  }

  TEST_CASE("FtsReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeMinimalFts();
    pistoris::Fts fts;
    REQUIRE(pistoris::readFts(fixture, fts) == ARX_OK);

    std::vector<std::uint8_t> out;
    CHECK(pistoris::writeFts(fts, out) == ARX_OK);
    CHECK(!out.empty());
    CHECK(pistoris::validate(fts) == ARX_OK);
  }

  TEST_CASE("LlfReadWriteRoundtrip") {
    pistoris::Llf llf;
    pistoris::llf::Light light;
    light.color = {1.0f, 0.5f, 0.25f};
    light.fallstart = 10.0f;
    light.fallend = 20.0f;
    light.intensity = 1.0f;
    llf.lights.push_back(light);
    llf.colors.push_back({0.5f, 0.5f, 0.5f});

    std::vector<std::uint8_t> bytes;
    REQUIRE(pistoris::writeLlf(llf, bytes) == ARX_OK);
    CHECK(!bytes.empty());

    pistoris::Llf loaded;
    REQUIRE(pistoris::readLlf(bytes, loaded) == ARX_OK);
    CHECK(pistoris::validate(loaded) == ARX_OK);
    CHECK(loaded.lights.size() == 1);
    CHECK(loaded.colors.size() == 1);
  }

  TEST_CASE("DlfReadIsTransactional") {
    constexpr std::size_t kHeaderSize = 8520;
    constexpr std::size_t kSceneSize = 640;
    std::vector<std::uint8_t> bytes(kHeaderSize + kSceneSize);
    const float version = pistoris::kDlfVersion;
    const std::int32_t num_scene = 1;
    std::memcpy(bytes.data(), &version, sizeof(version));
    std::memcpy(bytes.data() + 4, "DANAE_FILE", 11);
    std::memcpy(bytes.data() + 304, &num_scene, sizeof(num_scene));
    std::memcpy(bytes.data() + kHeaderSize, "graph/levels/level1/", sizeof("graph/levels/level1/"));

    pistoris::Dlf dlf;
    std::optional<pistoris::Llf> lighting = pistoris::Llf{};
    REQUIRE(pistoris::readDlf(bytes, dlf, &lighting) == ARX_OK);
    CHECK_FALSE(lighting.has_value());
    CHECK(pistoris::validate(dlf) == ARX_OK);

    dlf.entities.resize(1);
    lighting = pistoris::Llf{};
    bytes.pop_back();
    CHECK(pistoris::readDlf(bytes, dlf, &lighting) == ARX_UNEXPECTED_EOF);
    CHECK(dlf.entities.size() == 1);
    CHECK(lighting.has_value());
  }

  TEST_CASE("DlfWriteReadRoundtrip") {
    pistoris::Dlf source;
    setNativeText(source.scene_path, "graph/levels/level1");
    source.entities.push_back(
        nativeEntity("graph/obj3d/interactive/items/key/key", 7, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}));

    pistoris::Llf lighting;
    pistoris::llf::Light light;
    light.color = {1.0f, 1.0f, 1.0f};
    light.fallend = 10.0f;
    light.intensity = 1.0f;
    lighting.lights.push_back(light);

    std::vector<std::uint8_t> bytes;
    pistoris::DlfWriteOptions options{&lighting, {}};
    REQUIRE(pistoris::writeDlf(source, options, bytes) == ARX_OK);

    pistoris::Dlf loaded;
    std::optional<pistoris::Llf> loaded_lighting;
    REQUIRE(pistoris::readDlf(bytes, loaded, &loaded_lighting) == ARX_OK);
    REQUIRE(loaded.entities.size() == 1);
    CHECK(nativeText(loaded.entities[0].class_path).compare(nativeText(source.entities[0].class_path)) == 0);
    REQUIRE(loaded_lighting.has_value());
    CHECK(loaded_lighting->lights.size() == 1);

    std::vector<std::uint8_t> unchanged = {1, 2, 3};
    std::memset(source.scene_path, 0, sizeof(source.scene_path));
    CHECK(pistoris::writeDlf(source, options, unchanged) == ARX_DLF_BAD_SCENE_PATH);
    CHECK(unchanged == std::vector<std::uint8_t>{1, 2, 3});
  }

  TEST_CASE("DlfAndLlfWritersStampAttribution") {
    constexpr std::size_t kLastUserOffset = sizeof(float) + 16;
    constexpr std::size_t kLastUserSize = 256;
    constexpr std::size_t kTimeOffset = kLastUserOffset + kLastUserSize;

    pistoris::Dlf dlf;
    setNativeText(dlf.scene_path, "graph/levels/level1");
    std::vector<std::uint8_t> dlf_bytes;
    REQUIRE(pistoris::writeDlf(dlf, {nullptr, "editor"}, dlf_bytes, false) == ARX_OK);
    REQUIRE(dlf_bytes.size() > kTimeOffset + sizeof(std::int32_t));
    CHECK(std::memcmp(dlf_bytes.data() + kLastUserOffset, "arx-pistoris/editor", sizeof("arx-pistoris/editor")) == 0);
    std::int32_t dlf_time = 0;
    std::memcpy(&dlf_time, dlf_bytes.data() + kTimeOffset, sizeof(dlf_time));
    CHECK(dlf_time > 0);

    pistoris::Llf llf;
    const std::string long_signer(300, 'x');
    std::vector<std::uint8_t> llf_bytes;
    REQUIRE(pistoris::writeLlf(llf, llf_bytes, false) == ARX_OK);
    CHECK(std::memcmp(llf_bytes.data() + kLastUserOffset, "arx-pistoris", sizeof("arx-pistoris")) == 0);

    REQUIRE(pistoris::writeLlf(llf, {long_signer}, llf_bytes, false) == ARX_OK);
    REQUIRE(llf_bytes.size() > kTimeOffset + sizeof(std::int32_t));
    const std::string expected = "arx-pistoris/" + std::string(242, 'x');
    REQUIRE(expected.size() == kLastUserSize - 1);
    CHECK(std::memcmp(llf_bytes.data() + kLastUserOffset, expected.data(), expected.size()) == 0);
    CHECK(llf_bytes[kLastUserOffset + expected.size()] == 0);
    std::int32_t llf_time = 0;
    std::memcpy(&llf_time, llf_bytes.data() + kTimeOffset, sizeof(llf_time));
    CHECK(llf_time > 0);

    const std::string utf8_signer = std::string(241, 'x') + "\xe2\x82\xac" + "z";
    CHECK(pistoris::writeLlf(llf, {utf8_signer}, llf_bytes, false) == ARX_INVALID_OPTIONS);
    CHECK(pistoris::writeLlf(llf, {std::string("bad\0signer", 10)}, llf_bytes, false) == ARX_INVALID_OPTIONS);

    std::string dlf_json;
    REQUIRE(pistoris::toJson(dlf, dlf_json, false, "editor") == ARX_OK);
    CHECK(dlf_json.find("\"lastModifiedBy\":\"arx-pistoris/editor\"") != std::string::npos);
    CHECK(dlf_json.find("\"lastModifiedAt\":0") == std::string::npos);

    std::string llf_json;
    REQUIRE(pistoris::toJson(llf, llf_json, false, "editor") == ARX_OK);
    CHECK(llf_json.find("\"lastModifiedBy\":\"arx-pistoris/editor\"") != std::string::npos);
    CHECK(llf_json.find("\"lastModifiedAt\":0") == std::string::npos);
  }

  TEST_CASE("NativeWritersDefaultToCompressionAndCanEmitRawStorage") {
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(makeMinimalFtl(), ftl) == ARX_OK);
    std::vector<std::uint8_t> compressed_ftl;
    std::vector<std::uint8_t> raw_ftl;
    REQUIRE(pistoris::writeFtl(ftl, compressed_ftl) == ARX_OK);
    REQUIRE(pistoris::writeFtl(ftl, raw_ftl, false) == ARX_OK);
    REQUIRE(compressed_ftl.size() >= 2);
    CHECK(compressed_ftl[0] == 0);
    CHECK(compressed_ftl[1] == 6);
    REQUIRE(raw_ftl.size() >= sizeof(pistoris::kFtlMagic));
    CHECK(std::memcmp(raw_ftl.data(), pistoris::kFtlMagic, sizeof(pistoris::kFtlMagic)) == 0);
    pistoris::Ftl loaded_ftl;
    CHECK(pistoris::readFtl(compressed_ftl, loaded_ftl) == ARX_OK);
    CHECK(pistoris::readFtl(raw_ftl, loaded_ftl) == ARX_OK);

    pistoris::Fts fts;
    REQUIRE(pistoris::readFts(makeMinimalFts(), fts) == ARX_OK);
    std::vector<std::uint8_t> compressed_fts;
    std::vector<std::uint8_t> raw_fts;
    REQUIRE(pistoris::writeFts(fts, compressed_fts) == ARX_OK);
    REQUIRE(pistoris::writeFts(fts, raw_fts, false) == ARX_OK);
    const std::size_t fts_prefix_size = sizeof(pistoris::fts::Header);
    REQUIRE(compressed_fts.size() >= fts_prefix_size + 2);
    CHECK(compressed_fts[fts_prefix_size] == 0);
    CHECK(compressed_fts[fts_prefix_size + 1] == 6);
    pistoris::fts::Header compressed_fts_header;
    std::memcpy(&compressed_fts_header, compressed_fts.data(), sizeof(compressed_fts_header));
    CHECK(compressed_fts_header.uncompressedsize > 0);
    pistoris::fts::Header raw_fts_header;
    std::memcpy(&raw_fts_header, raw_fts.data(), sizeof(raw_fts_header));
    CHECK(raw_fts_header.uncompressedsize == 0);
    pistoris::Fts loaded_fts;
    CHECK(pistoris::readFts(compressed_fts, loaded_fts) == ARX_OK);
    CHECK(pistoris::readFts(raw_fts, loaded_fts) == ARX_OK);

    pistoris::Llf llf;
    std::vector<std::uint8_t> compressed_llf;
    std::vector<std::uint8_t> raw_llf;
    REQUIRE(pistoris::writeLlf(llf, {}, compressed_llf) == ARX_OK);
    REQUIRE(pistoris::writeLlf(llf, {}, raw_llf, false) == ARX_OK);
    REQUIRE(compressed_llf.size() >= 2);
    CHECK(compressed_llf[0] == 0);
    CHECK(compressed_llf[1] == 6);
    REQUIRE(raw_llf.size() >= 20);
    CHECK(std::memcmp(raw_llf.data() + sizeof(float), "DANAE_LLH_FILE", 14) == 0);
    pistoris::Llf loaded_llf;
    CHECK(pistoris::readLlf(compressed_llf, loaded_llf) == ARX_OK);
    CHECK(pistoris::readLlf(raw_llf, loaded_llf) == ARX_OK);

    pistoris::Dlf dlf;
    setNativeText(dlf.scene_path, "graph/levels/level1");
    const pistoris::DlfWriteOptions dlf_options;
    std::vector<std::uint8_t> compressed_dlf;
    std::vector<std::uint8_t> raw_dlf;
    REQUIRE(pistoris::writeDlf(dlf, dlf_options, compressed_dlf) == ARX_OK);
    REQUIRE(pistoris::writeDlf(dlf, dlf_options, raw_dlf, false) == ARX_OK);
    constexpr std::size_t kDlfRawPrefixSize = 8520;
    REQUIRE(compressed_dlf.size() >= kDlfRawPrefixSize + 2);
    CHECK(compressed_dlf[kDlfRawPrefixSize] == 0);
    CHECK(compressed_dlf[kDlfRawPrefixSize + 1] == 6);
    CHECK(raw_dlf.size() > kDlfRawPrefixSize);
    pistoris::Dlf loaded_dlf;
    CHECK(pistoris::readDlf(compressed_dlf, loaded_dlf) == ARX_OK);
    CHECK(pistoris::readDlf(raw_dlf, loaded_dlf) == ARX_OK);
  }

  TEST_CASE("JsonRoundtrip") {
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(makeTriangleFtlWithTexture(), ftl) == ARX_OK);

    std::string json;
    REQUIRE(pistoris::toJson(ftl, json, true) == ARX_OK);
    CHECK(json.find("\"vertices\"") != std::string::npos);

    pistoris::Ftl imported;
    CHECK(pistoris::fromJson(json, imported) == ARX_OK);
    CHECK(!imported.vertices.empty());
  }

  TEST_CASE("TeaJsonRoundtrip") {
    pistoris::Tea tea;
    REQUIRE(pistoris::readTea(makeKeyframeTea(), tea) == ARX_OK);

    std::string json;
    REQUIRE(pistoris::toJson(tea, json, true) == ARX_OK);
    CHECK(json.find("\"keyframes\"") != std::string::npos);
    CHECK(json.find("\"totalNumberOfFrames\"") != std::string::npos);

    pistoris::Tea imported;
    CHECK(pistoris::fromJson(json, imported) == ARX_OK);
    CHECK(!imported.keyframes.empty());
  }

  TEST_CASE("NativeLevelJsonRoundtrip") {
    pistoris::Fts fts = makeTriangleFtsData();
    std::memcpy(fts.header.path, "game/graph/levels/level7/fast.fts", sizeof("game/graph/levels/level7/fast.fts"));
    fts.scene.sizex = 160;
    fts.scene.sizez = 160;
    fts.cells.resize(160 * 160);

    std::string fts_json;
    REQUIRE(pistoris::toJson(fts, fts_json, true) == ARX_OK);
    CHECK(fts_json.find("https://arx-tools.github.io/schemas/fts.schema.json") != std::string::npos);
    CHECK(fts_json.find("\"levelIdx\": 7") != std::string::npos);

    fts.scene.sizex = 1;
    fts.cells.resize(160);
    CHECK(pistoris::toJson(fts, fts_json, true) == ARX_JSON_BAD_SCHEMA);
    fts.scene.sizex = 160;
    fts.cells.resize(160 * 160);

    std::memset(fts.header.path, 0, sizeof(fts.header.path));
    std::memcpy(fts.header.path, "custom/fast.fts", sizeof("custom/fast.fts"));
    CHECK(pistoris::toJson(fts, fts_json, true) == ARX_JSON_BAD_SCHEMA);
    std::memset(fts.header.path, 0, sizeof(fts.header.path));
    std::memcpy(fts.header.path, "game/graph/levels/level7/fast.fts", sizeof("game/graph/levels/level7/fast.fts"));

    pistoris::Fts imported_fts;
    REQUIRE(pistoris::fromJson(fts_json, imported_fts) == ARX_OK);
    CHECK(imported_fts.cells.size() == 160 * 160);
    REQUIRE(imported_fts.cells[0].polygons.size() == 1);
    CHECK(imported_fts.cells[0].polygons[0].room == 1);

    pistoris::Dlf dlf;
    setNativeText(dlf.scene_path, "graph/levels/level7");
    dlf.entities.push_back(
        nativeEntity("graph/obj3d/interactive/items/key/key", 7, {1.0f, 2.0f, 3.0f}, {4.0f, 5.0f, 6.0f}));
    pistoris::dlf::Zone silent_zone;
    setNativeText(silent_zone.name, "silent");
    silent_zone.points = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f}};
    silent_zone.height = 5;
    silent_zone.ambiance.emplace();
    setNativeText(silent_zone.ambiance->name, "none");
    dlf.zones.push_back(std::move(silent_zone));
    pistoris::dlf::Zone unchanged_zone;
    setNativeText(unchanged_zone.name, "unchanged");
    unchanged_zone.points = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f}};
    unchanged_zone.height = 5;
    dlf.zones.push_back(std::move(unchanged_zone));
    std::string dlf_json;
    REQUIRE(pistoris::toJson(dlf, dlf_json, true) == ARX_OK);
    CHECK(dlf_json.find("https://arx-tools.github.io/schemas/dlf.schema.json") != std::string::npos);
    setNativeText(dlf.scene_path, "custom/scene");
    CHECK(pistoris::toJson(dlf, dlf_json, true) == ARX_JSON_BAD_SCHEMA);
    setNativeText(dlf.scene_path, "graph/levels/level7");
    pistoris::Dlf imported_dlf;
    REQUIRE(pistoris::fromJson(dlf_json, imported_dlf) == ARX_OK);
    REQUIRE(imported_dlf.entities.size() == 1);
    CHECK(nativeText(imported_dlf.entities[0].class_path).compare(nativeText(dlf.entities[0].class_path)) == 0);
    REQUIRE(imported_dlf.zones.size() == 2);
    REQUIRE(imported_dlf.zones[0].ambiance.has_value());
    CHECK(nativeText(imported_dlf.zones[0].ambiance->name).compare("none") == 0);
    CHECK(imported_dlf.zones[0].ambiance->volume == 100.0f);
    CHECK_FALSE(imported_dlf.zones[1].ambiance.has_value());

    pistoris::Llf llf;
    pistoris::llf::Light light;
    light.color = {1.0f, 0.5f, 0.25f};
    light.fallstart = 10.0f;
    light.fallend = 20.0f;
    light.intensity = 1.0f;
    llf.lights.push_back(light);
    llf.colors.push_back({0.5f, 0.5f, 0.5f});
    std::string llf_json;
    REQUIRE(pistoris::toJson(llf, llf_json, true) == ARX_OK);
    CHECK(llf_json.find("https://arx-tools.github.io/schemas/llf.schema.json") != std::string::npos);
    pistoris::Llf imported_llf;
    REQUIRE(pistoris::fromJson(llf_json, imported_llf) == ARX_OK);
    CHECK(imported_llf.lights.size() == 1);
    CHECK(imported_llf.colors.size() == 1);
  }

  TEST_CASE("FtsLevelGlbRoundtrip") {
    pistoris::Fts fts = makeTriangleFtsData();

    pistoris::Level level;
    REQUIRE(pistoris::Level::importNative(level, fts) == ARX_OK);
    std::vector<std::uint8_t> glb;
    REQUIRE(level.exportGlb(glb) == ARX_OK);
    CHECK(glb.size() > 12);

    pistoris::Level imported;
    CHECK(pistoris::Level::importGlb(imported, glb) == ARX_OK);
    CHECK(imported.validate() == ARX_OK);
  }

  TEST_CASE("Failed Level GLB conversions preserve result information") {
    pistoris::Level level;
    pistoris::Level::GlbImportOptions options;
    ArxLevelGlbImportInfo info{{1.0f, 2.0f, 3.0f}};
    const std::vector<std::uint8_t> invalid;

    CHECK(pistoris::Level::importGlb(level, invalid, options, &info) != ARX_OK);
    CHECK(info.applied_arx_offset.x == 1.0f);
    CHECK(info.applied_arx_offset.y == 2.0f);
    CHECK(info.applied_arx_offset.z == 3.0f);

    ArxLevelModelPreviewReport report{1, 2, 3, 4, 5, 6};
    std::vector<std::uint8_t> encoded;
    const std::span<const pistoris::Model* const> no_previews;
    CHECK(level.exportGlb(encoded, no_previews, &report) != ARX_OK);
    CHECK(report.mapped_models == 1);
    CHECK(report.previewed_entities == 2);
    CHECK(report.skipped_anonymous_models == 3);
    CHECK(report.skipped_unmappable_models == 4);
    CHECK(report.skipped_duplicate_models == 5);
    CHECK(report.skipped_invalid_models == 6);
  }

  TEST_CASE("Returns the first canonical FTS texture path for each Level texture") {
    pistoris::Fts fts = makeTriangleFtsData();
    pistoris::fts::Texture first{};
    std::memcpy(first.fic, "graph/obj3d/textures/wall", sizeof("graph/obj3d/textures/wall"));
    pistoris::fts::Texture second{};
    std::memcpy(second.fic, "graph/obj3d/textures/wall", sizeof("graph/obj3d/textures/wall"));
    fts.textures.emplace(1, first);
    fts.textures.emplace(2, second);
    fts.scene.num_textures = 2;
    fts.cells[0].polygons[0].tex = 1;

    pistoris::Level level;
    std::vector<std::string> sources;
    REQUIRE(pistoris::Level::importNative(level, fts, nullptr, nullptr, &sources) == ARX_OK);
    CHECK(level.textureCount() == 1);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0] == "graph/obj3d/textures/wall");
  }

  TEST_CASE("Level public values preserve semantic defaults") {
    CHECK(ArxLevelFace{}.texture == ARX_NO_TEXTURE);
    CHECK(ArxLevelPortal{}.shape == ARX_PORTAL_QUAD);
    CHECK(ArxLevelAnchor{}.radius == 50.0f);
    CHECK(ArxLevelAnchor{}.height == -165.0f);
    CHECK(ArxLevelLight{}.fallend == 1.0f);
    CHECK(ArxLevelEntity{}.ident == -1);
    CHECK(ArxLevelZoneAmbiance{}.volume == 100.0f);
  }

  TEST_CASE("Level exposes canonical resource identity") {
    pistoris::Level level;
    REQUIRE(level.setResourcePath("level:3") == ARX_OK);
    CHECK((level.resourcePath() == "graph/levels/level3/level3.dlf"));
    CHECK(level.setResourcePath("model:npc:human_base") == ARX_LEVEL_BAD_RESOURCE_PATH);
  }

  TEST_CASE("Level generation convenience overloads preserve explicit defaults") {
    SUBCASE("navigation surface") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateNavSurface() ==
            explicit_options.generateNavSurface(pistoris::Level::NavSurfaceGenOptions{}));
    }
    SUBCASE("navigation floor") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.setNavSurfaceFromFloor() ==
            explicit_options.setNavSurfaceFromFloor(pistoris::Level::NavSurfaceSourceOptions{}));
    }
    SUBCASE("navigation pruning") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.pruneNavSurfaceIslands() ==
            explicit_options.pruneNavSurfaceIslands(pistoris::Level::NavSurfacePruneOptions{}));
    }
    SUBCASE("anchors") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateAnchors() == explicit_options.generateAnchors(pistoris::Level::AnchorGenOptions{}));
    }
    SUBCASE("anchor connections") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateAnchorConnections() ==
            explicit_options.generateAnchorConnections(pistoris::Level::AnchorConnectionGenOptions{}));
    }
    SUBCASE("anchor pruning") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.pruneAnchorIslands() ==
            explicit_options.pruneAnchorIslands(pistoris::Level::AnchorPruneOptions{}));
    }
    SUBCASE("room distances") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateRoomDistances() ==
            explicit_options.generateRoomDistances(pistoris::Level::RoomDistanceGenOptions{}));
    }
    SUBCASE("static lighting") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateStaticLighting() ==
            explicit_options.generateStaticLighting(pistoris::Level::StaticLightingGenOptions{}));
    }
    SUBCASE("minimap") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateMinimap() ==
            explicit_options.generateMinimap(pistoris::Level::MinimapGenerationOptions{}));
    }
  }

  TEST_CASE("Level editing accepts its borrowed texture image") {
    pistoris::Level level;
    pistoris::RoomIndex room_index = pistoris::kInvalidRoomIndex;
    const char room_name[] = "room";
    REQUIRE(level.addRoom({{room_name, sizeof(room_name) - 1}}, room_index) == ARX_OK);

    const ArxLevelVertex vertices[] = {
        {{0.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}},
    };
    ArxLevelFace face{};
    face.corners[0] = {0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f, {1.0f, 1.0f, 1.0f}};
    face.corners[1] = {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f, {1.0f, 1.0f, 1.0f}};
    face.corners[2] = {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f, {1.0f, 1.0f, 1.0f}};
    face.texture = 0;
    face.room = room_index;

    const std::vector<std::uint8_t> image = makeTestBmp();
    const char texture_path[] = "graph/obj3d/textures/test.bmp";
    const ArxTextureView texture = {
        {texture_path, sizeof(texture_path) - 1},
        {image.data(), image.size()},
    };
    const ArxLevelMeshInput mesh = {
        vertices,
        std::size(vertices),
        &face,
        1,
        &texture,
        1,
    };
    REQUIRE(level.replaceMesh(mesh) == ARX_OK);

    ArxTextureView borrowed{};
    REQUIRE(level.copyTextureViews(0, 1, &borrowed) == ARX_OK);
    REQUIRE(level.setTextureImage(0, borrowed.encoded_image) == ARX_OK);

    ArxTextureView copied{};
    REQUIRE(level.copyTextureViews(0, 1, &copied) == ARX_OK);
    REQUIRE(copied.encoded_image.size == image.size());
    CHECK(std::memcmp(copied.encoded_image.data, image.data(), image.size()) == 0);
  }

  TEST_CASE("LevelConversionAndDebugExport") {
    pistoris::Fts fts = makeTriangleFtsData();
    pistoris::Level level;
    REQUIRE(pistoris::Level::importNative(level, fts) == ARX_OK);

    std::vector<std::uint8_t> navigation_glb;
    REQUIRE(pistoris::level_debug::exportNavigationDebugGlb(level, navigation_glb) == ARX_OK);
    CHECK(navigation_glb.size() > 12);

    pistoris::Level::GlbExportOptions cell_options;
    cell_options.arx_units_per_glb_unit = 50.0f;
    std::vector<std::uint8_t> cells_glb;
    REQUIRE(pistoris::level_debug::exportFtsCellsDebugGlb(fts, cells_glb, cell_options) == ARX_OK);
    CHECK(cells_glb.size() > 12);

    cell_options.arx_units_per_glb_unit = 0.0f;
    std::vector<std::uint8_t> unchanged_cells = {1, 2, 3};
    CHECK(pistoris::level_debug::exportFtsCellsDebugGlb(fts, unchanged_cells, cell_options) == ARX_INVALID_OPTIONS);
    CHECK(unchanged_cells == std::vector<std::uint8_t>{1, 2, 3});

    pistoris::Level invalid;
    std::vector<std::uint8_t> unchanged = {1, 2, 3};
    CHECK(pistoris::level_debug::exportNavigationDebugGlb(invalid, unchanged) == ARX_LEVEL_NO_GEOMETRY);
    CHECK(unchanged == std::vector<std::uint8_t>{1, 2, 3});

    pistoris::Fts invalid_fts = makeMinimalFtsData();
    invalid_fts.scene.sizex = 0;
    pistoris::Level unchanged_level;
    REQUIRE(pistoris::Level::importNative(unchanged_level, fts) == ARX_OK);
    std::vector<std::uint8_t> before;
    REQUIRE(unchanged_level.exportGlb(before) == ARX_OK);
    CHECK(pistoris::Level::importNative(unchanged_level, invalid_fts) != ARX_OK);
    std::vector<std::uint8_t> after;
    REQUIRE(unchanged_level.exportGlb(after) == ARX_OK);
    CHECK(after == before);
  }

  TEST_CASE("ResourcePathUtilities") {
    static_assert(sizeof(ArxResourceKind) == 1);
    CHECK(pistoris::paths::resourceSelectorKind("model:npc:hero") == ARX_RESOURCE_KIND_MODEL);
    CHECK(pistoris::paths::resourceSelectorKind("hero.ftl") == ARX_RESOURCE_KIND_NONE);

    CHECK(pistoris::paths::levelDlf(3) == "graph/levels/level3/level3.dlf");
    CHECK(pistoris::paths::isPortableFilename("new_texture.png"));
    CHECK(pistoris::paths::isPortableFilename("wall_[metal].png"));
    CHECK_FALSE(pistoris::paths::isPortableFilename("new__texture.png"));
    CHECK(pistoris::paths::sanitizePortableFilename("wall_[metal].png") == "wall_[metal].png");
    CHECK(pistoris::paths::sanitizePortableFilename("new??__texture.png") == "new_texture.png");
    CHECK(pistoris::paths::isPortableResourcePathComponent("My texture_(stone)&[wet]__01.png"));
    CHECK_FALSE(pistoris::paths::isPortableResourcePathComponent("my#texture.png"));
    CHECK(pistoris::paths::textureDirectory() == "graph/obj3d/textures");
    CHECK(pistoris::paths::soundDirectory() == "sfx");
    CHECK(pistoris::paths::ambianceSoundDirectory() == "sfx/ambiance");

    std::string path;
    REQUIRE(pistoris::paths::modelFtl({.type = "npc", .name = "hero"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/hero/hero.ftl");
    pistoris::paths::ModelPathView model;
    REQUIRE(pistoris::paths::modelFromFtl(path, model));
    CHECK(model.type == "npc");
    CHECK(model.name == "hero");
    REQUIRE(pistoris::paths::entityClassFromModel(model, path));
    CHECK(path == "graph/obj3d/interactive/npc/hero/hero");
    REQUIRE(pistoris::paths::baseEntityClassFromModel({.type = "npc", .name = "hero", .tweak = "red"}, path));
    CHECK(path == "graph/obj3d/interactive/npc/hero/hero");
    ArxEntityClassKind class_kind = ARX_ENTITY_CLASS_KIND_UNKNOWN;
    REQUIRE(pistoris::paths::entityClassKind(path, class_kind));
    CHECK(class_kind == ARX_ENTITY_CLASS_KIND_NPC);
    std::string icon_path;
    REQUIRE(pistoris::paths::itemIconFromEntityClass(path, icon_path));
    CHECK(icon_path.empty());
    REQUIRE(pistoris::paths::modelFromEntityClass(path, model));

    REQUIRE(pistoris::paths::entityClassKind("graph/obj3d/interactive/items/weapons/sword/sword", class_kind));
    CHECK(class_kind == ARX_ENTITY_CLASS_KIND_ITEM);
    REQUIRE(pistoris::paths::itemIconFromEntityClass("graph/obj3d/interactive/items/weapons/sword/sword", icon_path));
    CHECK(icon_path == "graph/obj3d/interactive/items/weapons/sword/sword[icon]");
    CHECK_FALSE(pistoris::paths::itemIconFromEntityClass("../items/sword", icon_path));
    REQUIRE(pistoris::paths::modelSelector(model, path));
    CHECK(path == "model:npc:hero");
    REQUIRE(pistoris::paths::animationTea({"fix_inter", "open"}, path));
    CHECK(path == "graph/obj3d/anims/fix_inter/open.tea");
    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromTea(path, animation));
    std::uint32_t level = 0;
    REQUIRE(pistoris::paths::levelFromDlf(pistoris::paths::levelDlf(3), level));
    CHECK(level == 3);
  }

  TEST_CASE("FailedImportsLeaveOutputsUnchanged") {
    pistoris::Ftl ftl = makeData(2);
    std::size_t original_vertices = ftl.vertices.size();

    std::uint8_t bad_ftl[4] = {};
    CHECK(pistoris::readFtl(bad_ftl, ftl) != ARX_OK);
    CHECK(ftl.vertices.size() == original_vertices);

    CHECK(pistoris::fromJson("{", ftl) != ARX_OK);
    CHECK(ftl.vertices.size() == original_vertices);
  }
}  // TEST_SUITE("cpp_api")
