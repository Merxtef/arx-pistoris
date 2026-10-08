// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/texture.h"

#include <utility>
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/pistoris.hpp"

#include "helpers.h"
#include "image_helpers.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

template <class T, class Location>
T take(pistoris::Result<T, Location>&& result) {
  REQUIRE(result);
  return std::move(*result);
}

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
    auto loaded = pistoris::readFtl(fixture);
    REQUIRE(loaded);

    auto out = pistoris::writeFtl(*loaded);
    REQUIRE(out);
    CHECK(!out->empty());
  }

  TEST_CASE("TeaReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeKeyframeTea();
    auto loaded = pistoris::readTea(fixture);
    REQUIRE(loaded);

    auto out = pistoris::writeTea(*loaded);
    REQUIRE(out);
    CHECK(!out->empty());
  }

  TEST_CASE("FtsReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeMinimalFts();
    auto loaded = pistoris::readFts(fixture);
    REQUIRE(loaded);

    auto out = pistoris::writeFts(*loaded);
    REQUIRE(out);
    CHECK(!out->empty());
    CHECK(pistoris::validate(*loaded));
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

    auto bytes = pistoris::writeLlf(llf);
    REQUIRE(bytes);
    CHECK(!bytes->empty());

    auto loaded = pistoris::readLlf(*bytes);
    REQUIRE(loaded);
    CHECK(pistoris::validate(*loaded));
    CHECK(loaded->lights.size() == 1);
    CHECK(loaded->colors.size() == 1);
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

    auto loaded = pistoris::readDlf(bytes);
    REQUIRE(loaded);
    CHECK_FALSE(loaded->embedded_lighting.has_value());
    CHECK(pistoris::validate(loaded->dlf));

    bytes.pop_back();
    auto failed = pistoris::readDlf(bytes);
    CHECK(failed.code() == ARX_UNEXPECTED_EOF);
  }

  TEST_CASE("Native binary failures identify the unread field and byte range") {
    const std::array<std::uint8_t, 1> truncated{};
    const auto result = pistoris::readFtl(truncated);

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_UNEXPECTED_EOF);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    const pistoris::FtlBinaryLocation& location = *result.error()->location();
    CHECK(location.element == pistoris::FtlElement::kHeader);
    CHECK(location.field == "identifier");
    CHECK(location.region == pistoris::NativeBinaryRegion::kStored);
    CHECK(location.byte_offset == 0);
    CHECK(location.requested_bytes > truncated.size());
  }

  TEST_CASE("JSON syntax failures retain their source byte") {
    const auto result = pistoris::fromFtlJson("}");

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_JSON_BAD_FORMAT);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->byte_offset == 0);
  }

  TEST_CASE("JSON schema failures identify nested values") {
    const auto result = pistoris::fromFtlJson(
        R"({"header":{"origin":0,"name":"test"},"vertices":[{"vector":{"x":"bad","y":0,"z":0},"norm":{"x":0,"y":0,"z":0}}]})");

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_JSON_BAD_SCHEMA);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->pointer == "/vertices/0");
  }

  TEST_CASE("JSON semantic failures identify their native property") {
    const auto result = pistoris::fromFtlJson(
        R"({"header":{"origin":0,"name":"test"},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[{"faceType":0,"vertexIdx":[1,0,0],"textureIdx":-1,"u":[0,0,0],"v":[0,0,0],"norm":{"x":0,"y":1,"z":0}}],"textureContainers":[],"groups":[],"actions":[],"selections":[]})");

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_FTL_BAD_FACE_VERT_IDX);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->pointer == "/faces/0/vertexIdx/0");
  }

  TEST_CASE("JSON UTF-8 failures identify the invalid source byte") {
    constexpr char kInvalidUtf8[] = {'{', static_cast<char>(0xff), '}'};
    const auto result = pistoris::fromFtlJson(std::string_view(kInvalidUtf8, sizeof(kInvalidUtf8)));

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_TEXT_INVALID_UTF8);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->byte_offset == 1);
    CHECK(result.error()->location()->pointer.empty());
  }

  TEST_CASE("JSON schema failures identify the schema property") {
    const auto result = pistoris::fromFtlJson(R"({"$schema":"unsupported"})");

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_JSON_BAD_SCHEMA);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->pointer == "/$schema");
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

    pistoris::DlfWriteOptions options{&lighting, {}};
    auto bytes = pistoris::writeDlf(source, options);
    REQUIRE(bytes);

    auto loaded = pistoris::readDlf(*bytes);
    REQUIRE(loaded);
    REQUIRE(loaded->dlf.entities.size() == 1);
    CHECK(nativeText(loaded->dlf.entities[0].class_path).compare(nativeText(source.entities[0].class_path)) == 0);
    REQUIRE(loaded->embedded_lighting.has_value());
    CHECK(loaded->embedded_lighting->lights.size() == 1);

    std::memset(source.scene_path, 0, sizeof(source.scene_path));
    CHECK(pistoris::writeDlf(source, options).code() == ARX_DLF_BAD_SCENE_PATH);
  }

  TEST_CASE("DlfAndLlfWritersStampAttribution") {
    constexpr std::size_t kLastUserOffset = sizeof(float) + 16;
    constexpr std::size_t kLastUserSize = 256;
    constexpr std::size_t kTimeOffset = kLastUserOffset + kLastUserSize;

    pistoris::Dlf dlf;
    setNativeText(dlf.scene_path, "graph/levels/level1");
    auto dlf_bytes_result = pistoris::writeDlf(dlf, {nullptr, "editor"}, false);
    REQUIRE(dlf_bytes_result);
    const auto& dlf_bytes = *dlf_bytes_result;
    REQUIRE(dlf_bytes.size() > kTimeOffset + sizeof(std::int32_t));
    CHECK(std::memcmp(dlf_bytes.data() + kLastUserOffset, "arx-pistoris/editor", sizeof("arx-pistoris/editor")) == 0);
    std::int32_t dlf_time = 0;
    std::memcpy(&dlf_time, dlf_bytes.data() + kTimeOffset, sizeof(dlf_time));
    CHECK(dlf_time > 0);

    pistoris::Llf llf;
    const std::string long_signer(300, 'x');
    auto llf_bytes_result = pistoris::writeLlf(llf, false);
    REQUIRE(llf_bytes_result);
    auto llf_bytes = std::move(*llf_bytes_result);
    CHECK(std::memcmp(llf_bytes.data() + kLastUserOffset, "arx-pistoris", sizeof("arx-pistoris")) == 0);

    llf_bytes_result = pistoris::writeLlf(llf, {long_signer}, false);
    REQUIRE(llf_bytes_result);
    llf_bytes = std::move(*llf_bytes_result);
    REQUIRE(llf_bytes.size() > kTimeOffset + sizeof(std::int32_t));
    const std::string expected = "arx-pistoris/" + std::string(242, 'x');
    REQUIRE(expected.size() == kLastUserSize - 1);
    CHECK(std::memcmp(llf_bytes.data() + kLastUserOffset, expected.data(), expected.size()) == 0);
    CHECK(llf_bytes[kLastUserOffset + expected.size()] == 0);
    std::int32_t llf_time = 0;
    std::memcpy(&llf_time, llf_bytes.data() + kTimeOffset, sizeof(llf_time));
    CHECK(llf_time > 0);

    const std::string utf8_signer = std::string(241, 'x') + "\xe2\x82\xac" + "z";
    CHECK(pistoris::writeLlf(llf, {utf8_signer}, false).code() == ARX_INVALID_OPTIONS);
    CHECK(pistoris::writeLlf(llf, {std::string("bad\0signer", 10)}, false).code() == ARX_INVALID_OPTIONS);

    auto dlf_json = pistoris::toDlfJson(dlf, false, "editor");
    REQUIRE(dlf_json);
    CHECK(dlf_json->find("\"lastModifiedBy\":\"arx-pistoris/editor\"") != std::string::npos);
    CHECK(dlf_json->find("\"lastModifiedAt\":0") == std::string::npos);

    auto llf_json = pistoris::toLlfJson(llf, false, "editor");
    REQUIRE(llf_json);
    CHECK(llf_json->find("\"lastModifiedBy\":\"arx-pistoris/editor\"") != std::string::npos);
    CHECK(llf_json->find("\"lastModifiedAt\":0") == std::string::npos);
  }

  TEST_CASE("NativeWritersDefaultToCompressionAndCanEmitRawStorage") {
    auto ftl = pistoris::readFtl(makeMinimalFtl());
    REQUIRE(ftl);
    auto compressed_ftl_result = pistoris::writeFtl(*ftl);
    auto raw_ftl_result = pistoris::writeFtl(*ftl, false);
    REQUIRE(compressed_ftl_result);
    REQUIRE(raw_ftl_result);
    const auto& compressed_ftl = *compressed_ftl_result;
    const auto& raw_ftl = *raw_ftl_result;
    REQUIRE(compressed_ftl.size() >= 2);
    CHECK(compressed_ftl[0] == 0);
    CHECK(compressed_ftl[1] == 6);
    REQUIRE(raw_ftl.size() >= sizeof(pistoris::kFtlMagic));
    CHECK(std::memcmp(raw_ftl.data(), pistoris::kFtlMagic, sizeof(pistoris::kFtlMagic)) == 0);
    CHECK(pistoris::readFtl(compressed_ftl));
    CHECK(pistoris::readFtl(raw_ftl));

    auto fts = pistoris::readFts(makeMinimalFts());
    REQUIRE(fts);
    auto compressed_fts_result = pistoris::writeFts(*fts);
    auto raw_fts_result = pistoris::writeFts(*fts, false);
    REQUIRE(compressed_fts_result);
    REQUIRE(raw_fts_result);
    const auto& compressed_fts = *compressed_fts_result;
    const auto& raw_fts = *raw_fts_result;
    const std::size_t fts_prefix_size = sizeof(FtsStorageHeader);
    REQUIRE(compressed_fts.size() >= fts_prefix_size + 2);
    CHECK(compressed_fts[fts_prefix_size] == 0);
    CHECK(compressed_fts[fts_prefix_size + 1] == 6);
    FtsStorageHeader compressed_fts_header;
    std::memcpy(&compressed_fts_header, compressed_fts.data(), sizeof(compressed_fts_header));
    CHECK(compressed_fts_header.uncompressed_size > 0);
    FtsStorageHeader raw_fts_header;
    std::memcpy(&raw_fts_header, raw_fts.data(), sizeof(raw_fts_header));
    CHECK(raw_fts_header.uncompressed_size == 0);
    CHECK(pistoris::readFts(compressed_fts));
    CHECK(pistoris::readFts(raw_fts));

    pistoris::Llf llf;
    auto compressed_llf_result = pistoris::writeLlf(llf, pistoris::LlfWriteOptions{});
    auto raw_llf_result = pistoris::writeLlf(llf, pistoris::LlfWriteOptions{}, false);
    REQUIRE(compressed_llf_result);
    REQUIRE(raw_llf_result);
    const auto& compressed_llf = *compressed_llf_result;
    const auto& raw_llf = *raw_llf_result;
    REQUIRE(compressed_llf.size() >= 2);
    CHECK(compressed_llf[0] == 0);
    CHECK(compressed_llf[1] == 6);
    REQUIRE(raw_llf.size() >= 20);
    CHECK(std::memcmp(raw_llf.data() + sizeof(float), "DANAE_LLH_FILE", 14) == 0);
    CHECK(pistoris::readLlf(compressed_llf));
    CHECK(pistoris::readLlf(raw_llf));

    pistoris::Dlf dlf;
    setNativeText(dlf.scene_path, "graph/levels/level1");
    const pistoris::DlfWriteOptions dlf_options;
    auto compressed_dlf_result = pistoris::writeDlf(dlf, dlf_options);
    auto raw_dlf_result = pistoris::writeDlf(dlf, dlf_options, false);
    REQUIRE(compressed_dlf_result);
    REQUIRE(raw_dlf_result);
    const auto& compressed_dlf = *compressed_dlf_result;
    const auto& raw_dlf = *raw_dlf_result;
    constexpr std::size_t kDlfRawPrefixSize = 8520;
    REQUIRE(compressed_dlf.size() >= kDlfRawPrefixSize + 2);
    CHECK(compressed_dlf[kDlfRawPrefixSize] == 0);
    CHECK(compressed_dlf[kDlfRawPrefixSize + 1] == 6);
    CHECK(raw_dlf.size() > kDlfRawPrefixSize);
    CHECK(pistoris::readDlf(compressed_dlf));
    CHECK(pistoris::readDlf(raw_dlf));
  }

  TEST_CASE("JsonRoundtrip") {
    auto ftl = pistoris::readFtl(makeTriangleFtlWithTexture());
    REQUIRE(ftl);

    auto json = pistoris::toFtlJson(*ftl, true);
    REQUIRE(json);
    CHECK(json->find("\"vertices\"") != std::string::npos);

    auto imported = pistoris::fromFtlJson(*json);
    REQUIRE(imported);
    CHECK(!imported->vertices.empty());
  }

  TEST_CASE("TeaJsonRoundtrip") {
    auto tea = pistoris::readTea(makeKeyframeTea());
    REQUIRE(tea);

    auto json = pistoris::toTeaJson(*tea, true);
    REQUIRE(json);
    CHECK(json->find("\"keyframes\"") != std::string::npos);
    CHECK(json->find("\"totalNumberOfFrames\"") != std::string::npos);

    auto imported = pistoris::fromTeaJson(*json);
    REQUIRE(imported);
    CHECK(!imported->keyframes.empty());
  }

  TEST_CASE("NativeLevelJsonRoundtrip") {
    pistoris::Fts fts = makeTriangleFtsData();
    fts.scene.sizex = 160;
    fts.scene.sizez = 160;
    fts.cells.resize(160 * 160);

    auto fts_json = pistoris::toFtsJson(fts, 7, true);
    REQUIRE(fts_json);
    CHECK(fts_json->find("https://arx-tools.github.io/schemas/fts.schema.json") != std::string::npos);
    CHECK(fts_json->find("\"levelIdx\": 7") != std::string::npos);

    fts.scene.sizex = 1;
    fts.cells.resize(160);
    auto bad_grid = pistoris::toFtsJson(fts, 7, true);
    CHECK(bad_grid.code() == ARX_JSON_BAD_SCHEMA);
    REQUIRE(bad_grid.error());
    REQUIRE(bad_grid.error()->location());
    CHECK(bad_grid.error()->location()->element == pistoris::FtsElement::kHeader);
    CHECK(bad_grid.error()->location()->field == "scene.sizex");
    fts.scene.sizex = 160;
    fts.cells.resize(160 * 160);

    auto imported_fts = pistoris::fromFtsJson(*fts_json);
    REQUIRE(imported_fts);
    CHECK(imported_fts->level == 7);
    CHECK(imported_fts->fts.cells.size() == 160 * 160);
    REQUIRE(imported_fts->fts.cells[0].polygons.size() == 1);
    CHECK(imported_fts->fts.cells[0].polygons[0].room == 1);

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
    auto dlf_json = pistoris::toDlfJson(dlf, true);
    REQUIRE(dlf_json);
    CHECK(dlf_json->find("https://arx-tools.github.io/schemas/dlf.schema.json") != std::string::npos);
    setNativeText(dlf.scene_path, "custom/scene");
    auto bad_dlf_path = pistoris::toDlfJson(dlf, true);
    CHECK(bad_dlf_path.code() == ARX_JSON_BAD_SCHEMA);
    REQUIRE(bad_dlf_path.error());
    REQUIRE(bad_dlf_path.error()->location());
    CHECK(bad_dlf_path.error()->location()->field == "scene_path");
    setNativeText(dlf.scene_path, "graph/levels/level7");
    auto imported_dlf = pistoris::fromDlfJson(*dlf_json);
    REQUIRE(imported_dlf);
    REQUIRE(imported_dlf->entities.size() == 1);
    CHECK(nativeText(imported_dlf->entities[0].class_path).compare(nativeText(dlf.entities[0].class_path)) == 0);
    REQUIRE(imported_dlf->zones.size() == 2);
    REQUIRE(imported_dlf->zones[0].ambiance.has_value());
    CHECK(nativeText(imported_dlf->zones[0].ambiance->name).compare("none") == 0);
    CHECK(imported_dlf->zones[0].ambiance->volume == 100.0f);
    CHECK_FALSE(imported_dlf->zones[1].ambiance.has_value());

    pistoris::Llf llf;
    pistoris::llf::Light light;
    light.color = {1.0f, 0.5f, 0.25f};
    light.fallstart = 10.0f;
    light.fallend = 20.0f;
    light.intensity = 1.0f;
    llf.lights.push_back(light);
    llf.colors.push_back({0.5f, 0.5f, 0.5f});
    auto llf_json = pistoris::toLlfJson(llf, true);
    REQUIRE(llf_json);
    CHECK(llf_json->find("https://arx-tools.github.io/schemas/llf.schema.json") != std::string::npos);
    auto imported_llf = pistoris::fromLlfJson(*llf_json);
    REQUIRE(imported_llf);
    CHECK(imported_llf->lights.size() == 1);
    CHECK(imported_llf->colors.size() == 1);
  }

  TEST_CASE("FtsLevelGlbRoundtrip") {
    pistoris::Fts fts = makeTriangleFtsData();

    pistoris::Level level = take(pistoris::Level::importNative(fts));
    std::vector<std::uint8_t> glb = take(level.exportGlb());
    CHECK(glb.size() > 12);

    pistoris::Level imported = take(pistoris::Level::importGlb(glb));
    CHECK(imported.validate());
  }

  TEST_CASE("Failed Level GLB conversions preserve result information") {
    pistoris::Level::GlbImportOptions options;
    ArxLevelGlbImportInfo info{{1.0f, 2.0f, 3.0f}};
    const std::vector<std::uint8_t> invalid;

    const pistoris::GlbResult<pistoris::Level> import_result = pistoris::Level::importGlb(invalid, options, &info);
    CHECK_FALSE(import_result);
    REQUIRE(import_result.error() != nullptr);
    REQUIRE(import_result.error()->location());
    CHECK(import_result.error()->location()->element == pistoris::GlbElement::kDocument);
    CHECK(info.applied_arx_offset.x == 1.0f);
    CHECK(info.applied_arx_offset.y == 2.0f);
    CHECK(info.applied_arx_offset.z == 3.0f);

    ArxLevelModelPreviewReport report{1, 2, 3, 4, 5, 6};
    const std::span<const pistoris::Model* const> no_previews;
    pistoris::Level level;
    CHECK_FALSE(level.exportGlb(no_previews, &report));
    CHECK(report.mapped_models == 1);
    CHECK(report.previewed_entities == 2);
    CHECK(report.skipped_anonymous_models == 3);
    CHECK(report.skipped_unmappable_models == 4);
    CHECK(report.skipped_duplicate_models == 5);
    CHECK(report.skipped_invalid_models == 6);
  }

  TEST_CASE("Native Level failures identify exact carrier records") {
    pistoris::Fts bad_fts = makeTriangleFtsData();
    bad_fts.cells[0].polygons[0].v[1].ssx = std::numeric_limits<float>::quiet_NaN();
    const pistoris::LevelNativeResult<pistoris::Level> fts_result = pistoris::Level::importNative(bad_fts);
    REQUIRE_FALSE(fts_result);
    REQUIRE(fts_result.error() != nullptr);
    REQUIRE(fts_result.error()->location());
    const auto* fts_location = std::get_if<pistoris::FtsLocation>(&*fts_result.error()->location());
    REQUIRE(fts_location != nullptr);
    CHECK(fts_location->element == pistoris::FtsElement::kFace);
    CHECK(fts_location->index == 0);
    CHECK(fts_location->subindex == 1);

    const pistoris::Fts fts = makeTriangleFtsData();
    pistoris::Llf llf;
    llf.lights.resize(2);
    llf.lights[1].position.x = std::numeric_limits<float>::infinity();
    const pistoris::LevelNativeResult<pistoris::Level> llf_result = pistoris::Level::importNative(fts, &llf);
    REQUIRE_FALSE(llf_result);
    REQUIRE(llf_result.error() != nullptr);
    REQUIRE(llf_result.error()->location());
    const auto* llf_location = std::get_if<pistoris::LlfLocation>(&*llf_result.error()->location());
    REQUIRE(llf_location != nullptr);
    CHECK(llf_location->element == pistoris::LlfElement::kLight);
    CHECK(llf_location->index == 1);

    pistoris::Dlf dlf;
    setNativeText(dlf.scene_path, "graph/levels/level1");
    pistoris::dlf::Path path;
    setNativeText(path.name, "route");
    path.nodes.resize(2);
    path.nodes[1].relative_position.x = std::numeric_limits<float>::quiet_NaN();
    dlf.paths.push_back(path);
    const pistoris::LevelNativeResult<pistoris::Level> dlf_result = pistoris::Level::importNative(fts, nullptr, &dlf);
    REQUIRE_FALSE(dlf_result);
    REQUIRE(dlf_result.error() != nullptr);
    REQUIRE(dlf_result.error()->location());
    const auto* dlf_location = std::get_if<pistoris::DlfLocation>(&*dlf_result.error()->location());
    REQUIRE(dlf_location != nullptr);
    CHECK(dlf_location->element == pistoris::DlfElement::kPathNode);
    CHECK(dlf_location->index == 0);
    CHECK(dlf_location->subindex == 1);
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

    std::vector<std::string> sources;
    pistoris::Level level = take(pistoris::Level::importNative(fts, nullptr, nullptr, &sources));
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
    REQUIRE(level.setResourcePath("level:3"));
    CHECK((level.resourcePath() == "graph/levels/level3/level3.dlf"));
    CHECK(level.setResourcePath("model:npc:human_base").code() == ARX_LEVEL_BAD_RESOURCE_PATH);
  }

  TEST_CASE("Composed Level operations preserve prerequisite locations") {
    pistoris::Level level;
    REQUIRE(level.setResourcePath("level:3"));

    const pistoris::LevelResult<std::size_t> result = level.compactVertices();

    REQUIRE_FALSE(result);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->resource_path == "graph/levels/level3/level3.dlf");
    CHECK(result.error()->location()->element == pistoris::LevelElement::kFace);
  }

  TEST_CASE("Level generation convenience overloads preserve explicit defaults") {
    SUBCASE("navigation surface") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateNavSurface().code() ==
            explicit_options.generateNavSurface(pistoris::Level::NavSurfaceGenOptions{}).code());
    }
    SUBCASE("navigation floor") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.setNavSurfaceFromFloor().code() ==
            explicit_options.setNavSurfaceFromFloor(pistoris::Level::NavSurfaceSourceOptions{}).code());
    }
    SUBCASE("navigation pruning") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.pruneNavSurfaceIslands().code() ==
            explicit_options.pruneNavSurfaceIslands(pistoris::Level::NavSurfacePruneOptions{}).code());
    }
    SUBCASE("anchors") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateAnchors().code() ==
            explicit_options.generateAnchors(pistoris::Level::AnchorGenOptions{}).code());
    }
    SUBCASE("anchor connections") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateAnchorConnections().code() ==
            explicit_options.generateAnchorConnections(pistoris::Level::AnchorConnectionGenOptions{}).code());
    }
    SUBCASE("anchor pruning") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.pruneAnchorIslands().code() ==
            explicit_options.pruneAnchorIslands(pistoris::Level::AnchorPruneOptions{}).code());
    }
    SUBCASE("room distances") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateRoomDistances().code() ==
            explicit_options.generateRoomDistances(pistoris::Level::RoomDistanceGenOptions{}).code());
    }
    SUBCASE("static lighting") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateStaticLighting().code() ==
            explicit_options.generateStaticLighting(pistoris::Level::StaticLightingGenOptions{}).code());
    }
    SUBCASE("minimap") {
      pistoris::Level defaults;
      pistoris::Level explicit_options;
      CHECK(defaults.generateMinimap().code() ==
            explicit_options.generateMinimap(pistoris::Level::MinimapGenerationOptions{}).code());
    }
  }

  TEST_CASE("Level editing accepts its borrowed texture image") {
    pistoris::Level level;
    const char room_name[] = "room";
    const pistoris::RoomIndex room_index = take(level.addRoom({{room_name, sizeof(room_name) - 1}}));

    const std::vector<std::uint8_t> image = makeTestBmp();
    const char texture_path[] = "graph/obj3d/textures/test.bmp";
    const ArxTextureView texture = {
        {texture_path, sizeof(texture_path) - 1},
        {image.data(), image.size()},
    };
    const std::array<float, 9> positions = {0, 0, 0, 1, 0, 0, 0, 0, 1};
    const std::array<std::uint32_t, 3> indices = {0, 1, 2};
    const std::array<float, 6> uvs = {0, 0, 1, 0, 0, 1};
    const std::array<float, 9> normals = {0, -1, 0, 0, -1, 0, 0, -1, 0};
    const std::array<std::uint32_t, 1> texture_indices = {0};
    const std::array<float, 1> transvals = {0};
    const std::array<float, 9> colors = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    const std::array<pistoris::RoomIndex, 1> rooms = {room_index};
    REQUIRE(level.addTexture(texture));
    REQUIRE(level.replaceVertices(positions));
    REQUIRE(level.replaceFaces(indices, uvs, normals, texture_indices, transvals, colors));
    REQUIRE(level.replaceFaceRooms(rooms));

    const ArxTextureView borrowed = level.textures()[0];
    REQUIRE(level.setTextureImage(0, borrowed.encoded_image));

    const ArxTextureView copied = level.textures()[0];
    REQUIRE(copied.encoded_image.size == image.size());
    CHECK(std::memcmp(copied.encoded_image.data, image.data(), image.size()) == 0);
  }

  TEST_CASE("LevelConversionAndDebugExport") {
    pistoris::Fts fts = makeTriangleFtsData();
    pistoris::Level level = take(pistoris::Level::importNative(fts));

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
    pistoris::Level unchanged_level = take(pistoris::Level::importNative(fts));
    std::vector<std::uint8_t> before = take(unchanged_level.exportGlb());
    CHECK_FALSE(pistoris::Level::importNative(invalid_fts));
    std::vector<std::uint8_t> after = take(unchanged_level.exportGlb());
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
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = "hero"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/hero/hero.ftl");
    pistoris::paths::ModelPathView model;
    REQUIRE(pistoris::paths::modelFromFtl(path, model));
    CHECK(model.type == pistoris::paths::ModelPathType::kNpc);
    CHECK(model.name == "hero");
    REQUIRE(pistoris::paths::entityClassFromModel(model, path));
    CHECK(path == "graph/obj3d/interactive/npc/hero/hero");
    REQUIRE(pistoris::paths::baseEntityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "hero", .tweak = "red"}, path));
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
    REQUIRE(pistoris::paths::animationTea({pistoris::paths::AnimationPathType::kFixInter, "open"}, path));
    CHECK(path == "graph/obj3d/anims/fix_inter/open.tea");
    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromTea(path, animation));
    std::uint32_t level = 0;
    REQUIRE(pistoris::paths::levelFromDlf(pistoris::paths::levelDlf(3), level));
    CHECK(level == 3);
  }

  TEST_CASE("FailedImportsReturnNoPartialValue") {
    pistoris::Ftl ftl = makeData(2);

    std::uint8_t bad_ftl[4] = {};
    CHECK_FALSE(pistoris::readFtl(bad_ftl));

    CHECK_FALSE(pistoris::fromFtlJson("{"));
  }
}  // TEST_SUITE("cpp_api")
