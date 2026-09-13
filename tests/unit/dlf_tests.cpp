/*
 * Copyright 2011-2019 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Based on:
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code').

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of
these additional terms immediately following the terms and conditions of the GNU General Public License which
accompanied the Arx Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address
below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane
Studios, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// Source: https://github.com/arx/ArxLibertatis/blob/5b95e4c5ca9d583f1b11c085326979772645e0f3/src/scene/LevelFormat.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/json.h"
#include "native/dlf.h"
#include "native/lighting_layout.h"
#include "paths/entity_class.h"
#include "utils/cursor.h"
#include "utils/log.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Header {
  float version = pistoris::kDlfVersion;
  char ident[16] = "DANAE_FILE";
  char unused0[260] = {};
  pistoris::ArxVector3 player_position = {};
  pistoris::ArxAngle player_angle = {};
  std::int32_t num_scenes = 1;
  std::int32_t num_entities = 0;
  std::int32_t num_nodes = 0;
  std::int32_t num_node_links = 0;
  std::int32_t num_zones = 0;
  std::int32_t lighting = 0;
  char unused1[1024] = {};
  std::int32_t num_lights = 0;
  std::int32_t num_fogs = 0;
  char unused2[12] = {};
  std::int32_t num_paths = 0;
  char unused3[7144] = {};
};
static_assert(sizeof(Header) == 8520);

struct Scene {
  char name[512] = "graph/levels/level1/";
  std::int32_t pad[16] = {};
  float fpad[16] = {};
};
static_assert(sizeof(Scene) == 640);

struct Entity {
  char name[512] = {};
  pistoris::ArxVector3 position = {};
  pistoris::ArxAngle angle = {};
  std::int32_t ident = -1;
  std::int32_t flags = 0;
  std::int32_t pad[14] = {};
  float fpad[16] = {};
};
static_assert(sizeof(Entity) == 664);

struct LogCapture {
  std::vector<std::string> warnings;

  LogCapture() {
    pistoris::log_fn = [](ArxLogLevel level, const char* message, void* userdata) {
      if (level == ARX_LOG_WARN) static_cast<LogCapture*>(userdata)->warnings.emplace_back(message);
    };
    pistoris::log_ud = this;
  }

  ~LogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }

  bool contains(std::string_view text) const {
    for (const std::string& warning : warnings)
      if (warning.find(text) != std::string::npos) return true;
    return false;
  }
};

ArxReturnCode load(const std::vector<std::uint8_t>& bytes, pistoris::dlf::Data& out,
                   std::optional<pistoris::llf::Data>* lighting = nullptr) {
  pistoris::ReadCursor cursor(bytes.data(), bytes.size());
  return pistoris::loadDlf(&out, lighting, cursor);
}

ArxReturnCode save(const pistoris::dlf::Data& data, std::vector<std::uint8_t>& out,
                   const pistoris::llf::Data* lighting = nullptr) {
  pistoris::WriteCursor cursor;
  ArxReturnCode rc = pistoris::saveDlf(&data, lighting, {}, cursor);
  if (rc == ARX_OK) out = cursor.take();
  return rc;
}

std::vector<std::uint8_t> minimalDlf(const Header& header = {}, const Scene& scene = {}) {
  std::vector<std::uint8_t> bytes(sizeof(header));
  std::memcpy(bytes.data(), &header, sizeof(header));
  if (header.num_scenes == 1) {
    const std::size_t offset = bytes.size();
    bytes.resize(offset + sizeof(scene));
    std::memcpy(bytes.data() + offset, &scene, sizeof(scene));
  }
  return bytes;
}

}  // namespace

TEST_SUITE("dlf") {
  TEST_CASE("DlfMinimalRead") {
    pistoris::dlf::Data data;
    std::optional<pistoris::llf::Data> lighting = pistoris::llf::Data{};
    REQUIRE(load(minimalDlf(), data, &lighting) == ARX_OK);
    CHECK(data.version == pistoris::kDlfVersion);
    CHECK(data.entities.empty());
    CHECK(data.fogs.empty());
    CHECK(data.zones.empty());
    CHECK(data.paths.empty());
    CHECK(data.scene_path == "graph/levels/level1/");
    CHECK_FALSE(lighting.has_value());
  }

  TEST_CASE("DlfRejectsHeaderErrors") {
    pistoris::dlf::Data data;
    CHECK(load({}, data) == ARX_UNEXPECTED_EOF);

    Header header;
    header.version = 1.43f;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_VERSION);

    header = {};
    header.ident[0] = 'X';
    CHECK(load(minimalDlf(header), data) == ARX_INVALID_IDENTIFIER);

    header = {};
    header.num_scenes = 0;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_SCENE_COUNT);

    header.num_scenes = 2;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_SCENE_COUNT);

    header = {};
    Scene scene;
    scene.name[0] = '\0';
    CHECK(load(minimalDlf(header, scene), data) == ARX_DLF_BAD_SCENE_PATH);

    std::memset(scene.name, 'x', sizeof(scene.name));
    CHECK(load(minimalDlf(header, scene), data) == ARX_DLF_BAD_SCENE_PATH);

    header = {};
    header.num_entities = -1;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_ENTITY_COUNT);

    header = {};
    header.num_nodes = -1;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_AI_NODE_COUNT);

    header = {};
    header.num_node_links = -1;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_AI_NODE_LINK_COUNT);

    header = {};
    header.num_lights = -1;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_EMBEDDED_LIGHT_COUNT);

    header = {};
    header.num_fogs = -1;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_FOG_COUNT);

    header = {};
    header.num_paths = -1;
    CHECK(load(minimalDlf(header), data) == ARX_DLF_BAD_PATH_RECORD_COUNT);
  }

  TEST_CASE("DlfRejectsInvalidEmbeddedColorCount") {
    Header header;
    header.lighting = 1;
    std::vector<std::uint8_t> bytes = minimalDlf(header);

    pistoris::native_lighting::Header lighting_header;
    lighting_header.num_values = -1;
    const auto* first = reinterpret_cast<const std::uint8_t*>(&lighting_header);
    bytes.insert(bytes.end(), first, first + sizeof(lighting_header));

    pistoris::dlf::Data data;
    std::optional<pistoris::llf::Data> lighting;
    CHECK(load(bytes, data, &lighting) == ARX_DLF_BAD_EMBEDDED_COLOR_COUNT);
  }

  TEST_CASE("DlfEmbeddedLightingIsOptionalAndTransactional") {
    Header header;
    header.lighting = 1;
    header.num_lights = 1;
    std::vector<std::uint8_t> bytes = minimalDlf(header);

    pistoris::native_lighting::Header lighting_header;
    lighting_header.num_values = 1;
    const std::uint32_t color = 0xff7f3f1fU;
    pistoris::native_lighting::Light light;
    light.color = {1.0f, 1.0f, 1.0f};
    light.fallend = 10.0f;
    light.intensity = 1.0f;
    const auto append = [&](const auto& value) {
      const auto* first = reinterpret_cast<const std::uint8_t*>(&value);
      bytes.insert(bytes.end(), first, first + sizeof(value));
    };
    append(lighting_header);
    append(color);
    append(light);

    pistoris::dlf::Data data;
    std::optional<pistoris::llf::Data> lighting;
    REQUIRE(load(bytes, data, &lighting) == ARX_OK);
    REQUIRE(lighting.has_value());
    CHECK(lighting->colors.size() == 1);
    CHECK(lighting->lights.size() == 1);

    pistoris::dlf::Data ignored;
    CHECK(load(bytes, ignored, nullptr) == ARX_OK);

    bytes.pop_back();
    pistoris::dlf::Data unchanged;
    unchanged.entities.resize(1);
    std::optional<pistoris::llf::Data> unchanged_lighting = pistoris::llf::Data{};
    CHECK(load(bytes, unchanged, &unchanged_lighting) == ARX_UNEXPECTED_EOF);
    CHECK(unchanged.entities.size() == 1);
    CHECK(unchanged_lighting.has_value());
  }

  TEST_CASE("DlfInvalidEmbeddedLightingIsDiscardedOnlyWhenRequested") {
    Header header;
    header.num_lights = 1;
    std::vector<std::uint8_t> bytes = minimalDlf(header);

    pistoris::native_lighting::Light light;
    light.color = {2.0f, 1.0f, 1.0f};
    const auto* first = reinterpret_cast<const std::uint8_t*>(&light);
    bytes.insert(bytes.end(), first, first + sizeof(light));

    pistoris::dlf::Data data;
    std::optional<pistoris::llf::Data> lighting = pistoris::llf::Data{};
    REQUIRE(load(bytes, data, &lighting) == ARX_OK);
    CHECK_FALSE(lighting.has_value());
    CHECK(load(bytes, data, nullptr) == ARX_OK);
  }

  TEST_CASE("DlfEntityClassPathNormalization") {
    std::string normalized;
    std::string_view removed_extension = "stale";
    CHECK(pistoris::normalizeEntityClassPath(
        R"(\ARKANESERVER\Public\Arx\Graph\Obj3D\Interactive\Fix_inter\Timed_lever\Timed_lever.teo)",
        normalized,
        removed_extension));
    CHECK(normalized == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    CHECK(std::string(removed_extension) == ".teo");
    CHECK(pistoris::normalizeEntityClassPath("graph/obj3d/interactive/items/key", normalized, removed_extension));
    CHECK(normalized == "graph/obj3d/interactive/items/key");
    CHECK(removed_extension.empty());
    CHECK(pistoris::normalizeEntityClassPath("graph/obj3d/interactive/npc/my__npc", normalized, removed_extension));
    CHECK(normalized == "graph/obj3d/interactive/npc/my__npc");
    CHECK(removed_extension.empty());
    CHECK_FALSE(
        pistoris::normalizeEntityClassPath("graph/obj3d/interactive/npc/my?npc", normalized, removed_extension));
    CHECK(pistoris::normalizeEntityClassPath("obj3d/interactive/items/key", normalized, removed_extension));
    CHECK(normalized == "obj3d/interactive/items/key");
    CHECK(removed_extension.empty());
    bool discarded_prefix = false;
    CHECK(
        pistoris::normalizeEntityClassPath("prefix/graph/items/key", normalized, removed_extension, &discarded_prefix));
    CHECK(normalized == "graph/items/key");
    CHECK(discarded_prefix);
    CHECK(pistoris::normalizeEntityClassPath(
        "graph/folder/graph/items/key", normalized, removed_extension, &discarded_prefix));
    CHECK(normalized == "graph/folder/graph/items/key");
    CHECK_FALSE(discarded_prefix);
    CHECK_FALSE(pistoris::normalizeEntityClassPath("graph/../items/key", normalized, removed_extension));
    CHECK_FALSE(pistoris::normalizeEntityClassPath(
        std::string("graph/obj3d/items/key\0hidden", 28), normalized, removed_extension));
  }

  TEST_CASE("EntityClassPathClassificationMatchesEnginePrecedence") {
    CHECK(pistoris::classifyEntityClassPath("graph/obj3d/interactive/items/npc_token/npc_token") ==
          pistoris::InteractiveKind::kItem);
    CHECK(pistoris::classifyEntityClassPath("graph/obj3d/interactive/npc/human_base/human_base") ==
          pistoris::InteractiveKind::kNpc);
    CHECK(pistoris::classifyEntityClassPath("graph/obj3d/interactive/fix_inter/door/door") ==
          pistoris::InteractiveKind::kFix);
    CHECK(pistoris::classifyEntityClassPath("graph/obj3d/interactive/camera/camera") ==
          pistoris::InteractiveKind::kCamera);
    CHECK(pistoris::classifyEntityClassPath("graph/obj3d/interactive/marker/marker") ==
          pistoris::InteractiveKind::kMarker);
    CHECK(pistoris::classifyEntityClassPath("graph/obj3d/interactive/system/system") ==
          pistoris::InteractiveKind::kUnknown);
  }

  TEST_CASE("DlfReadNormalizesLegacyTeoEntityClasses") {
    Header header;
    header.num_entities = 1;
    std::vector<std::uint8_t> bytes = minimalDlf(header);
    Entity entity;
    constexpr std::string_view kClassPath =
        R"(\ARKANESERVER\Public\Arx\Graph\Obj3D\Interactive\Fix_inter\Timed_lever\Timed_lever.TEO)";
    std::memcpy(entity.name, kClassPath.data(), kClassPath.size());
    const auto* first = reinterpret_cast<const std::uint8_t*>(&entity);
    bytes.insert(bytes.end(), first, first + sizeof(entity));

    pistoris::dlf::Data data;
    LogCapture logs;
    REQUIRE(load(bytes, data) == ARX_OK);
    REQUIRE(data.entities.size() == 1);
    CHECK(data.entities[0].class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    CHECK(logs.contains("normalized 1 legacy .teo entity class path(s)"));
  }

  TEST_CASE("DlfJsonReadNormalizesLegacyTeoEntityClasses") {
    pistoris::dlf::Data source;
    source.scene_path = "graph/levels/level1/";
    source.entities.push_back({"graph/obj3d/interactive/fix_inter/timed_lever/timed_lever"});
    std::string json;
    REQUIRE(pistoris::exportDlfToJson(source, false, {}, json) == ARX_OK);
    const std::size_t name = json.find("fix_inter/timed_lever");
    REQUIRE(name != std::string::npos);
    json.insert(name + std::string_view("fix_inter/timed_lever").size(), ".TEO");

    pistoris::dlf::Data data;
    LogCapture logs;
    REQUIRE(pistoris::importJsonToDlf(json, &data) == ARX_OK);
    REQUIRE(data.entities.size() == 1);
    CHECK(data.entities[0].class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    CHECK(logs.contains("normalized 1 legacy .teo entity class path(s)"));
  }

  TEST_CASE("DlfValidation") {
    pistoris::dlf::Data data;
    CHECK(pistoris::validateDlf(nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_SCENE_PATH);

    data.scene_path = "graph/levels/level1/";
    CHECK(pistoris::validateDlf(&data) == ARX_OK);

    data.player_spawn.position.x = std::numeric_limits<float>::infinity();
    CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PLAYER_SPAWN);
  }

  TEST_CASE("DlfValidationReportsSpecificPayloadErrors") {
    pistoris::dlf::Data data;
    data.scene_path = "graph/levels/level1/";
    const float not_finite = std::numeric_limits<float>::quiet_NaN();

    SUBCASE("entity") {
      data.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door"});
      REQUIRE(pistoris::validateDlf(&data) == ARX_OK);

      data.entities[0].class_path = "prefix/graph/obj3d/interactive/fix_inter/door/door";
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ENTITY_CLASS_PATH);
      data.entities[0].class_path = "graph/obj3d/interactive/fix_inter/door/door";
      data.entities[0].position.x = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ENTITY_POSITION);
      data.entities[0].position = {};
      data.entities[0].angle.yaw = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ENTITY_ANGLE);
    }

    SUBCASE("fog") {
      data.fogs.push_back({});
      REQUIRE(pistoris::validateDlf(&data) == ARX_OK);

      data.fogs[0].position.x = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_FOG_POSITION);
      data.fogs[0].position = {};
      data.fogs[0].color.r = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_FOG_COLOR);
      data.fogs[0].color = {};
      data.fogs[0].angle.roll = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_FOG_ANGLE);
      data.fogs[0].angle = {};
      data.fogs[0].size = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_FOG_EFFECT);
    }

    SUBCASE("zone") {
      pistoris::dlf::Zone zone;
      zone.name = "zone";
      zone.points = {{}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
      zone.height = 1;
      data.zones.push_back(std::move(zone));
      REQUIRE(pistoris::validateDlf(&data) == ARX_OK);

      data.zones[0].name.clear();
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_NAME);
      data.zones[0].name = std::string("zo\0ne", 5);
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_NAME);
      data.zones[0].name = "zone";
      data.zones[0].position.y = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_POSITION);
      data.zones[0].position = {};
      data.zones[0].points.pop_back();
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_POINT_COUNT);
      data.zones[0].points.push_back({0.0f, 0.0f, 1.0f});
      data.zones[0].points[1].z = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_POINT);
      data.zones[0].points[1].z = 0.0f;
      data.zones[0].height = 0;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_HEIGHT);
      data.zones[0].height = 1;
      data.zones[0].color = pistoris::ArxColor3{not_finite, 0.0f, 0.0f};
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_COLOR);
      data.zones[0].color.reset();
      data.zones[0].farclip = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_FARCLIP);
      data.zones[0].farclip.reset();
      data.zones[0].ambiance = pistoris::dlf::ZoneAmbiance{"ambient", not_finite};
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_AMBIANCE);
      data.zones[0].ambiance = pistoris::dlf::ZoneAmbiance{std::string("amb\0ient", 8), 1.0f};
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_ZONE_AMBIANCE);
    }

    SUBCASE("path") {
      data.paths.push_back({"path", {}, {{{}, pistoris::dlf::PathNodeType::kStandard, 0}}});
      REQUIRE(pistoris::validateDlf(&data) == ARX_OK);

      data.paths[0].name.clear();
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_NAME);
      data.paths[0].name = std::string("pa\0th", 5);
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_NAME);
      data.paths[0].name = "path";
      data.paths[0].position.z = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_POSITION);
      data.paths[0].position = {};
      data.paths[0].nodes.clear();
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_NODE_COUNT);
      data.paths[0].nodes.push_back({});
      data.paths[0].nodes[0].relative_position.x = not_finite;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_NODE_POSITION);
      data.paths[0].nodes[0].relative_position = {};
      data.paths[0].nodes[0].type = static_cast<pistoris::dlf::PathNodeType>(99);
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_NODE_TYPE);
      data.paths[0].nodes[0].type = pistoris::dlf::PathNodeType::kStandard;
      data.paths[0].nodes[0].time_ms = 1;
      CHECK(pistoris::validateDlf(&data) == ARX_DLF_BAD_PATH_FIRST_NODE);
    }
  }

  TEST_CASE("DlfWriteReadCanonicalRoundtrip") {
    pistoris::dlf::Data source;
    source.player_spawn = {{1.0f, 2.0f, 3.0f}, {10.0f, 20.0f, 30.0f}};
    source.scene_path = "graph/levels/level1/";
    source.entities.push_back(
        {"graph/obj3d/interactive/fix_inter/door/door", 42, {4.0f, 5.0f, 6.0f}, {40.0f, 50.0f, 60.0f}});
    source.fogs.push_back(
        {{7.0f, 8.0f, 9.0f}, {0.1f, 0.2f, 0.3f}, 12.0f, true, 1.5f, {1.0f, 2.0f, 3.0f}, 4.0f, 5.0f, 600, 7.0f});

    pistoris::dlf::Zone zone;
    zone.name = "hall";
    zone.position = {100.0f, 200.0f, 300.0f};
    zone.points = {{0.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 0.0f}, {100.0f, 0.0f, 100.0f}};
    zone.height = 150;
    zone.color = pistoris::ArxColor3{0.4f, 0.5f, 0.6f};
    zone.farclip = 1200.0f;
    zone.ambiance = pistoris::dlf::ZoneAmbiance{"ambient_cave_a", 80.0f};
    source.zones.push_back(zone);

    pistoris::dlf::Path path;
    path.name = "patrol";
    path.position = {10.0f, 20.0f, 30.0f};
    path.nodes = {
        {{0.0f, 0.0f, 0.0f}, pistoris::dlf::PathNodeType::kStandard, 0},
        {{25.0f, 0.0f, 0.0f}, pistoris::dlf::PathNodeType::kBezier, 500},
        {{50.0f, 0.0f, 10.0f}, pistoris::dlf::PathNodeType::kControlPoint, 750},
    };
    source.paths.push_back(path);

    pistoris::llf::Data lighting;
    lighting.colors.push_back({0.25f, 0.5f, 0.75f});
    pistoris::llf::Light light;
    light.position = {11.0f, 12.0f, 13.0f};
    light.color = {1.0f, 0.5f, 0.25f};
    light.fallstart = 20.0f;
    light.fallend = 40.0f;
    light.intensity = 1.0f;
    lighting.lights.push_back(light);

    std::vector<std::uint8_t> bytes;
    REQUIRE(save(source, bytes, &lighting) == ARX_OK);
    CHECK(bytes.size() == 8520 + 640 + 664 + 16 + 4 + 296 + 592 + 608 + 3 * 68 + 608 + 3 * 68);

    pistoris::dlf::Data loaded;
    std::optional<pistoris::llf::Data> loaded_lighting;
    REQUIRE(load(bytes, loaded, &loaded_lighting) == ARX_OK);
    REQUIRE(loaded.entities.size() == 1);
    CHECK(loaded.entities[0].class_path == source.entities[0].class_path);
    CHECK(loaded.entities[0].ident == 42);
    REQUIRE(loaded.fogs.size() == 1);
    CHECK(loaded.fogs[0].directional);
    REQUIRE(loaded.zones.size() == 1);
    CHECK(loaded.zones[0].name == "hall");
    CHECK(loaded.zones[0].height == 150);
    REQUIRE(loaded.zones[0].ambiance.has_value());
    CHECK(loaded.zones[0].ambiance->name == "ambient_cave_a");
    REQUIRE(loaded.paths.size() == 1);
    REQUIRE(loaded.paths[0].nodes.size() == 3);
    CHECK(loaded.paths[0].nodes[1].type == pistoris::dlf::PathNodeType::kBezier);
    CHECK(loaded.paths[0].nodes[2].type == pistoris::dlf::PathNodeType::kControlPoint);
    REQUIRE(loaded_lighting.has_value());
    CHECK(loaded_lighting->colors.size() == 1);
    CHECK(loaded_lighting->lights.size() == 1);
  }

  TEST_CASE("DlfWriteRejectsSceneAndFixedStringErrors") {
    pistoris::dlf::Data data;
    std::vector<std::uint8_t> out;
    CHECK(save(data, out) == ARX_DLF_BAD_SCENE_PATH);
    data.scene_path = std::string(512, 'x');
    CHECK(save(data, out) == ARX_DLF_BAD_SCENE_PATH);
    data.scene_path = std::string("level\0scene", 11);
    CHECK(save(data, out) == ARX_DLF_BAD_SCENE_PATH);

    data.scene_path = "graph/levels/level1/";
    data.entities.push_back({std::string(512, 'x')});
    CHECK(save(data, out) == ARX_DLF_BAD_ENTITY_CLASS_PATH);
  }

  TEST_CASE("DlfReadRetainsScenePath") {
    pistoris::dlf::Data source;
    source.scene_path = R"(Graph\Levels\Level12\)";
    std::vector<std::uint8_t> bytes;
    REQUIRE(save(source, bytes) == ARX_OK);

    pistoris::dlf::Data loaded;
    REQUIRE(load(bytes, loaded) == ARX_OK);
    CHECK(loaded.scene_path == source.scene_path);
  }
}
