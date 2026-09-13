// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/scene.h"
#include "utils/math/quat.h"
#include "utils/math/rotation.h"

#include <limits>
#include <string>
#include <string_view>
#include <utility>

using namespace pistoris;

namespace {

SceneData makeScene() {
  SceneData scene;
  scene.player_spawn = PlayerSpawn{{1.0f, 2.0f, 3.0f}, math::angleToQuat({4.0f, 5.0f, 6.0f})};
  scene.entities.push_back({"graph/obj3d/interactive/items/torch", -1, {1.0f, 2.0f, 3.0f}, {}, "torch"});
  Fog fog;
  fog.position = {1.0f, 2.0f, 3.0f};
  scene.fogs.push_back(fog);
  scene.zones.push_back({"zone",
                         {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}},
                         0.0f,
                         ZoneHeightMode::kFinite,
                         3.0f,
                         ArxColor3{},
                         500.0f,
                         ZoneAmbiance{"ambient_cave_a", 80.0f}});
  scene.paths.push_back(
      {"patrol", {}, {{{}, PathNodeType::kStandard, 0}, {{1.0f, 0.0f, 0.0f}, PathNodeType::kBezier, 100}}});
  return scene;
}

}  // namespace

TEST_SUITE("scene::validation") {
  TEST_CASE("Accepts valid scene data") {
    SceneData scene = makeScene();
    REQUIRE(scene.player_spawn.has_value());
    CHECK(scene::validatePlayerSpawn(scene.player_spawn.value()) == scene::Error::kNone);
    CHECK(scene::validateEntities(scene.entities) == scene::Error::kNone);
    CHECK(scene::validateFogs(scene.fogs) == scene::Error::kNone);
    CHECK(scene::validateZones(scene.zones) == scene::Error::kNone);
    CHECK(scene::validatePaths(scene.paths) == scene::Error::kNone);
    CHECK(scene::validate(scene) == scene::Error::kNone);
  }

  TEST_CASE("Rejects bad player spawn") {
    SceneData scene = makeScene();
    scene.player_spawn.value().rotation.x = std::numeric_limits<float>::infinity();
    CHECK(scene::validate(scene) == scene::Error::kBadPlayerSpawn);

    scene = makeScene();
    scene.player_spawn.value().rotation.w = 2.0f;
    scene.player_spawn.value().rotation.x = 0.0f;
    scene.player_spawn.value().rotation.y = 0.0f;
    scene.player_spawn.value().rotation.z = 0.0f;
    CHECK(scene::validate(scene) == scene::Error::kBadPlayerSpawn);
  }

  TEST_CASE("Accepts absent player spawn") {
    SceneData scene = makeScene();
    scene.player_spawn.reset();
    CHECK(scene::validatePlayerSpawn(scene) == scene::Error::kNone);
    CHECK(scene::validate(scene) == scene::Error::kNone);
  }

  TEST_CASE("Normalizes valid rotations and rejects near-zero rotations") {
    ArxQuat rotation{2.0f, 0.0f, 0.0f, 0.0f};
    CHECK(math::normalizeRotation(rotation));
    CHECK(rotation.w == doctest::Approx(1.0f));

    rotation = {1.0e-7f, 0.0f, 0.0f, 0.0f};
    CHECK_FALSE(math::normalizeRotation(rotation));
  }

  TEST_CASE("Rejects bad entities") {
    SceneData scene = makeScene();
    scene.entities[0].name = "entity__one";
    CHECK(scene::validateEntity(scene.entities[0]) == scene::Error::kBadEntityName);

    scene = makeScene();
    scene.entities[0].name = std::string("entity\0one", 10);
    CHECK(scene::validateEntity(scene.entities[0]) == scene::Error::kBadEntityName);

    scene = makeScene();
    scene.entities[0].class_path = "Graph\\Obj3D\\Interactive\\Items\\Torch.teo";
    CHECK(scene::validate(scene) == scene::Error::kBadEntityClassPath);

    scene = makeScene();
    scene.entities[0].class_path = "graph/obj3d/interactive/items/torch__lit";
    CHECK(scene::validateEntity(scene.entities[0]) == scene::Error::kNone);

    scene = makeScene();
    scene.entities[0].class_path = "graph/obj3d/interactive/items/torch?lit";
    CHECK(scene::validateEntity(scene.entities[0]) == scene::Error::kBadEntityClassPath);

    scene = makeScene();
    scene.entities[0].position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene::validate(scene) == scene::Error::kBadEntityPosition);

    scene = makeScene();
    scene.entities[0].rotation.z = std::numeric_limits<float>::infinity();
    CHECK(scene::validate(scene) == scene::Error::kBadEntityRotation);

    scene = makeScene();
    scene.entities[0].rotation.w = 2.0f;
    CHECK(scene::validate(scene) == scene::Error::kBadEntityRotation);
  }

  TEST_CASE("Entity collection validation requires resolved unique names") {
    SceneData scene = makeScene();
    scene.entities[0].name.clear();
    CHECK(scene::validateEntity(scene.entities[0]) == scene::Error::kNone);
    CHECK(scene::validateEntities(scene.entities) == scene::Error::kBadEntityName);

    scene = makeScene();
    scene.entities.push_back(scene.entities.front());
    CHECK(scene::validateEntities(scene.entities) == scene::Error::kDuplicateEntityName);
  }

  TEST_CASE("Derives and uniquifies entity names without taking natural candidates") {
    SceneData scene = makeScene();
    Entity entity = scene.entities.front();
    scene.entities.clear();
    auto add = [&](std::string class_path, std::string name = {}) {
      entity.class_path = std::move(class_path);
      entity.name = std::move(name);
      scene.entities.push_back(entity);
    };
    add("graph/obj3d/interactive/npc/spider_base/spider_base");
    add("graph/obj3d/interactive/npc/spider_base/spider_base");
    add("graph/obj3d/interactive/fix_inter/marker/marker", "spider_1");
    add("graph/obj3d/interactive/npc/human_base/human_base");
    add("graph/obj3d/interactive/fix_inter/marker/marker", "human");
    add("graph/obj3d/interactive/fix_inter/marker/marker", "human");
    add("graph/obj3d/interactive/fix_inter/_base/_base");
    add("graph/obj3d/interactive/fix_inter/marker/marker", "human_base");

    CHECK(scene::repairEntityNames(scene.entities) == 6);
    REQUIRE(scene.entities.size() == 8);
    CHECK(scene.entities[0].name == "spider");
    CHECK(scene.entities[1].name == "spider_2");
    CHECK(scene.entities[2].name == "spider_1");
    CHECK(scene.entities[3].name == "human");
    CHECK(scene.entities[4].name == "human_1");
    CHECK(scene.entities[5].name == "human_2");
    CHECK(scene.entities[6].name == "base");
    CHECK(scene.entities[7].name == "human_base");
    CHECK(scene::validateEntities(scene.entities) == scene::Error::kNone);
  }

  TEST_CASE("Rejects bad fogs") {
    SceneData scene = makeScene();
    scene.fogs[0].name = "fog__one";
    CHECK(scene::validateFog(scene.fogs[0]) == scene::Error::kBadFogName);

    scene = makeScene();
    scene.fogs[0].name = std::string("fog\0one", 7);
    CHECK(scene::validateFog(scene.fogs[0]) == scene::Error::kBadFogName);

    scene = makeScene();
    scene.fogs[0].position.z = std::numeric_limits<float>::infinity();
    CHECK(scene::validate(scene) == scene::Error::kBadFogPosition);

    scene = makeScene();
    scene.fogs[0].size = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene::validate(scene) == scene::Error::kBadFogEffect);

    scene = makeScene();
    scene.fogs[0].directional = false;
    scene.fogs[0].rotation.w = 2.0f;
    CHECK(scene::validate(scene) == scene::Error::kBadFogRotation);
  }

  TEST_CASE("Rejects and repairs duplicate fog names") {
    SceneData scene = makeScene();
    scene.fogs.front().name = "mist";
    scene.fogs.push_back(scene.fogs.front());

    CHECK(scene::validateFogs(scene.fogs) == scene::Error::kDuplicateFogName);
    CHECK(scene::repairFogNames(scene.fogs) == 1);
    CHECK(scene.fogs[0].name == "mist");
    CHECK(scene.fogs[1].name == "mist_1");
    CHECK(scene::validateFogs(scene.fogs) == scene::Error::kNone);

    scene = makeScene();
    scene.fogs.push_back(scene.fogs.front());
    CHECK(scene::repairFogNames(scene.fogs) == 0);
    CHECK(scene::validateFogs(scene.fogs) == scene::Error::kNone);
  }

  TEST_CASE("Rejects bad zones") {
    SceneData scene = makeScene();
    scene.zones[0].name.clear();
    CHECK(scene::validate(scene) == scene::Error::kBadZoneName);

    scene = makeScene();
    scene.zones[0].perimeter_xz[1] = scene.zones[0].perimeter_xz[0];
    CHECK(scene::validate(scene) == scene::Error::kBadZonePerimeter);

    scene = makeScene();
    scene.zones[0].height_mode = static_cast<ZoneHeightMode>(10);
    CHECK(scene::validate(scene) == scene::Error::kBadZoneHeightMode);

    scene = makeScene();
    scene.zones[0].height = 0.0f;
    CHECK(scene::validate(scene) == scene::Error::kBadZoneHeight);

    scene = makeScene();
    scene.zones[0].color->r = std::numeric_limits<float>::quiet_NaN();
    CHECK(scene::validate(scene) == scene::Error::kBadZoneColor);

    scene = makeScene();
    scene.zones[0].farclip = std::numeric_limits<float>::infinity();
    CHECK(scene::validate(scene) == scene::Error::kBadZoneFarclip);

    scene = makeScene();
    scene.zones[0].ambiance->name = "ambient_cave_a.amb";
    CHECK(scene::validate(scene) == scene::Error::kBadZoneAmbiance);

    scene = makeScene();
    scene.zones[0].name = "zone__one";
    CHECK(scene::validateZone(scene.zones[0]) == scene::Error::kBadZoneName);

    scene = makeScene();
    scene.zones[0].name = std::string("zone\0one", 8);
    CHECK(scene::validateZone(scene.zones[0]) == scene::Error::kBadZoneName);

    scene = makeScene();
    scene.zones[0].ambiance->name = "ambient__cave.v2";
    CHECK(scene::validateZone(scene.zones[0]) == scene::Error::kNone);

    scene = makeScene();
    scene.zones[0].ambiance->name = "ambient?cave";
    CHECK(scene::validateZone(scene.zones[0]) == scene::Error::kBadZoneAmbiance);
  }

  TEST_CASE("Repairs zone names to unique lowercase identifiers") {
    SceneData scene = makeScene();
    scene.zones.push_back(scene.zones.front());
    scene.zones.back().name = "ZONE";

    CHECK(scene::validateZones(scene.zones) == scene::Error::kBadZoneName);
    CHECK(scene::repairZoneNames(scene.zones) == 1);
    CHECK(scene.zones[0].name == "zone");
    CHECK(scene.zones[1].name == "zone_1");
    CHECK(scene::validateZones(scene.zones) == scene::Error::kNone);

    scene.zones[1].name = "zone";
    CHECK(scene::validateZones(scene.zones) == scene::Error::kDuplicateZoneName);
  }

  TEST_CASE("Zone and path names use independent namespaces") {
    SceneData scene = makeScene();
    scene.paths[0].name = "zone";

    CHECK(scene::validate(scene) == scene::Error::kNone);
  }

  TEST_CASE("Rejects bad paths") {
    SceneData scene = makeScene();
    scene.paths[0].name.clear();
    CHECK(scene::validate(scene) == scene::Error::kBadPathName);

    scene = makeScene();
    scene.paths[0].position.y = std::numeric_limits<float>::infinity();
    CHECK(scene::validate(scene) == scene::Error::kBadPathPosition);

    scene = makeScene();
    scene.paths[0].nodes.front().time_ms = 1;
    CHECK(scene::validate(scene) == scene::Error::kBadPathFirstNode);

    scene = makeScene();
    scene.paths[0].nodes.back().type = static_cast<PathNodeType>(2);
    CHECK(scene::validate(scene) == scene::Error::kBadPathNodeType);

    scene = makeScene();
    scene.paths[0].name = "path__one";
    CHECK(scene::validatePath(scene.paths[0]) == scene::Error::kBadPathName);

    scene = makeScene();
    scene.paths[0].name = std::string("path\0one", 8);
    CHECK(scene::validatePath(scene.paths[0]) == scene::Error::kBadPathName);

    scene = makeScene();
    scene.paths.push_back(scene.paths.front());
    CHECK(scene::validatePaths(scene.paths) == scene::Error::kDuplicatePathName);
  }

  TEST_CASE("Makes duplicate path names unique without taking existing names") {
    SceneData scene = makeScene();
    Path path = scene.paths.front();
    scene.paths.clear();
    for (std::string_view name : {"patrol_", "patrol_1", "patrol_", "guard", "guard"}) {
      path.name = name;
      scene.paths.push_back(path);
    }

    CHECK(scene::repairPathNames(scene.paths) == 3);
    REQUIRE(scene.paths.size() == 5);
    CHECK(scene.paths[0].name == "patrol");
    CHECK(scene.paths[1].name == "patrol_1");
    CHECK(scene.paths[2].name == "patrol_2");
    CHECK(scene.paths[3].name == "guard");
    CHECK(scene.paths[4].name == "guard_1");
    CHECK(scene::validatePaths(scene.paths) == scene::Error::kNone);
  }

  TEST_CASE("Repairs path names to unique lowercase identifiers") {
    SceneData scene = makeScene();
    scene.paths.push_back(scene.paths.front());
    scene.paths.back().name = "PATROL";

    CHECK(scene::validatePaths(scene.paths) == scene::Error::kBadPathName);
    CHECK(scene::repairPathNames(scene.paths) == 1);
    CHECK(scene.paths[0].name == "patrol");
    CHECK(scene.paths[1].name == "patrol_1");
    CHECK(scene::validatePaths(scene.paths) == scene::Error::kNone);

    scene.paths[1].name = "patrol";
    CHECK(scene::validatePaths(scene.paths) == scene::Error::kDuplicatePathName);
  }

  TEST_CASE("Zone and path edits publish lowercase names transactionally") {
    SceneData scene = makeScene();
    Zone zone = scene.zones.front();
    zone.name = "UPPER-ZONE";
    scene::repairZoneName(scene, zone);
    REQUIRE(scene::validateZone(zone) == scene::Error::kNone);
    const ZoneIndex zone_index = scene::addZone(scene, std::move(zone));
    REQUIRE(zone_index == 1);
    CHECK(scene.zones[zone_index].name == "upper-zone");

    Zone invalid_zone = scene.zones.front();
    invalid_zone.name = "bad__zone";
    CHECK(scene::validateZone(invalid_zone) == scene::Error::kBadZoneName);
    CHECK(scene.zones[zone_index].name == "upper-zone");

    Path path = scene.paths.front();
    path.name = "UPPER-PATH";
    scene::repairPathName(scene, path);
    REQUIRE(scene::validatePath(path) == scene::Error::kNone);
    const PathIndex path_index = scene::addPath(scene, std::move(path));
    REQUIRE(path_index == 1);
    CHECK(scene.paths[path_index].name == "upper-path");

    Path invalid_path = scene.paths.front();
    invalid_path.name = "bad__path";
    CHECK(scene::validatePath(invalid_path) == scene::Error::kBadPathName);
    CHECK(scene.paths[path_index].name == "upper-path");
  }
}
