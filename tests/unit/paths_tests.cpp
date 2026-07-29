// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include <cstdint>
#include <limits>
#include <ostream>  // IWYU pragma: keep
#include <span>
#include <string>

TEST_SUITE("paths") {
  TEST_CASE("Portable emitted filenames validate and sanitize through the public path API") {
    CHECK(pistoris::paths::isPortableFilename("my-texture.png"));
    CHECK(pistoris::paths::isPortableFilename("wall_[metal].png"));
    CHECK_FALSE(pistoris::paths::isPortableFilename("my__texture.png"));
    CHECK_FALSE(pistoris::paths::isPortableFilename("folder/texture.png"));
    CHECK_FALSE(pistoris::paths::isPortableFilename("NUL.png"));

    CHECK(pistoris::paths::sanitizePortableFilename("wall_[metal].png") == "wall_[metal].png");
    CHECK(pistoris::paths::sanitizePortableFilename("my??__texture.png") == "my_texture.png");
    CHECK(pistoris::paths::sanitizePortableFilename("NUL.png") == "NUL_1.png");
    const std::string sanitized = pistoris::paths::sanitizePortableFilename("folder/texture.png");
    CHECK(sanitized == "folder_texture.png");
    CHECK(pistoris::paths::isPortableFilename(sanitized));
    CHECK(pistoris::paths::sanitizePortableFilename(sanitized) == sanitized);
  }

  TEST_CASE("Known game texture paths map to library-safe identities") {
    CHECK(pistoris::paths::textureFromGame(R"(GRAPH\OBJ3D\TEXTURES\L4_DWARF_[STONE]__WALL01.BMP)") ==
          "graph/obj3d/textures/l4_dwarf_[stone]_wall01.BMP");
    CHECK(pistoris::paths::textureFromGame("graph/obj3d/textures/l4_dwarf_[stone]__wall24.jpg") ==
          "graph/obj3d/textures/l4_dwarf_[stone]_wall24.jpg");
    CHECK(pistoris::paths::textureFromGame("graph/obj3d/textures/npc_human__base_hero_head") ==
          "graph/obj3d/textures/npc_human_base_hero_head_1");
  }

  TEST_CASE("Library-safe texture identities map back to game paths") {
    CHECK(pistoris::paths::textureToGame("GRAPH/OBJ3D/TEXTURES/L4_DWARF_[STONE]_WALL01.PNG") ==
          "graph/obj3d/textures/l4_dwarf_[stone]__wall01.PNG");
    CHECK(pistoris::paths::textureToGame("graph/obj3d/textures/l4_dwarf_[stone]_wall24.tga") ==
          "graph/obj3d/textures/l4_dwarf_[stone]__wall24.tga");
    CHECK(pistoris::paths::textureToGame("graph/obj3d/textures/npc_human_base_hero_head_1.bmp") ==
          "graph/obj3d/textures/npc_human__base_hero_head.bmp");
  }

  TEST_CASE("Texture aliases are scoped to complete canonical resource paths") {
    CHECK(pistoris::paths::textureFromGame("custom/l4_dwarf_[stone]__wall01.jpg") ==
          "custom/l4_dwarf_[stone]__wall01.jpg");
    CHECK(pistoris::paths::textureToGame("custom/l4_dwarf_[stone]_wall01.jpg") == "custom/l4_dwarf_[stone]_wall01.jpg");
    CHECK(pistoris::paths::textureFromGame("graph/obj3d/textures/unknown__texture.bmp") ==
          "graph/obj3d/textures/unknown__texture.bmp");
  }

  TEST_CASE("Level resource paths use the conventional runtime identities") {
    CHECK(pistoris::paths::levelDlf(17) == "graph/levels/level17/level17.dlf");
    CHECK(pistoris::paths::levelLlf(17) == "graph/levels/level17/level17.llf");
    CHECK(pistoris::paths::levelFts(17) == "game/graph/levels/level17/fast.fts");
    CHECK(pistoris::paths::levelDlf(std::numeric_limits<std::uint32_t>::max()) ==
          "graph/levels/level4294967295/level4294967295.dlf");
  }

  TEST_CASE("Canonical level paths recover their level number") {
    std::uint32_t level = 0;
    REQUIRE(pistoris::paths::levelFromDlf(R"(GRAPH\LEVELS\LEVEL17\LEVEL17.DLF)", level));
    CHECK(level == 17);
    REQUIRE(pistoris::paths::levelFromDlf("graph/levels/level17/level17", level));
    CHECK(level == 17);
    REQUIRE(pistoris::paths::levelFromLlf("graph/levels/level23/level23.llf", level));
    CHECK(level == 23);
    REQUIRE(pistoris::paths::levelFromLlf("graph/levels/level23/level23", level));
    CHECK(level == 23);
    REQUIRE(pistoris::paths::levelFromFts("game/graph/levels/level42/fast.FTS", level));
    CHECK(level == 42);
    REQUIRE(pistoris::paths::levelFromFts("game/graph/levels/level42/fast", level));
    CHECK(level == 42);

    level = 99;
    CHECK_FALSE(pistoris::paths::levelFromDlf("graph/levels/level17/level18.dlf", level));
    CHECK(level == 99);
    CHECK_FALSE(pistoris::paths::levelFromDlf("graph/levels/level017/level017.dlf", level));
    CHECK(level == 99);
    CHECK_FALSE(pistoris::paths::levelFromLlf("graph/levels/level17/level17.dlf", level));
    CHECK(level == 99);
    CHECK_FALSE(pistoris::paths::levelFromFts("game/graph/levels/level17/fast.bin", level));
    CHECK(level == 99);
  }

  TEST_CASE("Level shorthands identify one canonical DLF") {
    static_assert(sizeof(ArxResourceKind) == 1);
    CHECK(pistoris::paths::resourceShorthandKind("LEVEL:invalid") == ARX_RESOURCE_KIND_LEVEL);
    CHECK(pistoris::paths::resourceShorthandKind("model:npc:hero") == ARX_RESOURCE_KIND_MODEL);
    CHECK(pistoris::paths::resourceShorthandKind("ANIM:npc:walk") == ARX_RESOURCE_KIND_ANIMATION);
    CHECK(pistoris::paths::resourceShorthandKind("cinematic:intro") == ARX_RESOURCE_KIND_CINEMATIC);
    CHECK(pistoris::paths::resourceShorthandKind("ambiance:cave") == ARX_RESOURCE_KIND_AMBIANCE);
    CHECK(pistoris::paths::resourceShorthandKind("C:\\level:17") == ARX_RESOURCE_KIND_NONE);
    CHECK(pistoris::paths::resourceShorthandKind("level") == ARX_RESOURCE_KIND_NONE);

    CHECK(pistoris::paths::levelShorthand(17) == "level:17");
    std::uint32_t level = 99;
    REQUIRE(pistoris::paths::levelFromShorthand("LEVEL:17", level));
    CHECK(level == 17);
    CHECK_FALSE(pistoris::paths::levelFromShorthand("level:*", level));
    CHECK(level == 17);
  }

  TEST_CASE("Model paths validate and normalize interactive types") {
    const std::span<const std::string_view> types = pistoris::paths::modelTypes();
    REQUIRE(types.size() == 11);
    CHECK(types.front() == "npc");
    CHECK(types.back() == "weapons");

    std::string path;
    REQUIRE(pistoris::paths::modelFtl({.type = "NPC", .name = "my_npc"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/my_npc/my_npc.ftl");

    for (std::string_view type :
         {"armor", "jewelry", "magic", "movable", "provisions", "quest_item", "special", "weapons"}) {
      REQUIRE(pistoris::paths::modelFtl({.type = type, .name = "key.ftl"}, path));
      CHECK(path == "game/graph/obj3d/interactive/items/" + std::string(type) + "/key/key.ftl");
    }
    REQUIRE(pistoris::paths::modelFtl({.type = "system", .name = "camera"}, path));
    REQUIRE(pistoris::paths::modelFtl({.type = "fix_inter", .name = "door"}, path));
    REQUIRE(pistoris::paths::modelFtl({.type = "npc", .name = "human_base", .tweak = "skin/red.ftl"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = "npc", .name = "human_base", .tweak = "skin/red.v2"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.v2.ftl");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "invalid", .name = "model"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "items", .name = "key"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "weapon", .name = "sword"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "npc", .name = "folder/model"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "npc", .name = ".ftl"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "npc", .name = "human.TEO"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "npc", .name = "human", .tweak = "skins/red.teo"}, path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Canonical model paths and entity classes recover model components") {
    pistoris::paths::ModelPathView model;
    REQUIRE(
        pistoris::paths::modelFromFtl(R"(GAME\GRAPH\OBJ3D\INTERACTIVE\Fix_Inter\Timed_Lever\Timed_Lever.FTL)", model));
    CHECK(model.type == "fix_inter");
    CHECK(model.name == "Timed_Lever");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/items/weapons/long_sword/long_sword", model));
    CHECK(model.type == "weapons");
    CHECK(model.name == "long_sword");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.ftl", model));
    CHECK(model.type == "npc");
    CHECK(model.name == "human_base");
    CHECK(model.tweak == "skin/red");

    std::string class_path;
    REQUIRE(pistoris::paths::entityClassFromModel({.type = "FIX_INTER", .name = "Timed_Lever.ftl"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == "fix_inter");
    CHECK(model.name == "timed_lever");
    model = {"unchanged-type", "unchanged-name", "unchanged-tweak"};
    class_path = "unchanged-path";
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/fix_inter/door/other.ftl", model));
    CHECK(model.type == "unchanged-type");
    CHECK(model.name == "unchanged-name");
    CHECK(model.tweak == "unchanged-tweak");
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/fix_inter/door/door.bin", model));
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/fix_inter/door/door.teo", model));
    CHECK_FALSE(
        pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.teo", model));
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/items/key/key.ftl", model));
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/items/weapons/sword/other.ftl", model));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/fix_inter/custom/door/door", model));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/items/armor/chest_chain/chest_chain.teo",
                                                      model));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/items/weapon/sword/sword", model));
    CHECK_FALSE(pistoris::paths::entityClassFromModel({.type = "fix_inter", .name = "door.part"}, class_path));
    CHECK_FALSE(pistoris::paths::entityClassFromModel({.type = "npc", .name = "human", .tweak = "red"}, class_path));
    CHECK(class_path == "unchanged-path");
  }

  TEST_CASE("Model shorthands use flattened types and preserve optional tweaks") {
    std::string shorthand;
    REQUIRE(pistoris::paths::modelShorthand({.type = "Armor", .name = "chest.FTL"}, shorthand));
    CHECK(shorthand == "model:armor:chest");
    REQUIRE(
        pistoris::paths::modelShorthand({.type = "npc", .name = "human_base", .tweak = R"(skins\red.ftl)"}, shorthand));
    CHECK(shorthand == "model:npc:human_base:skins/red");

    pistoris::paths::ModelPathView model;
    REQUIRE(pistoris::paths::modelFromShorthand("MODEL:Provisions:bread.ftl", model));
    CHECK(model.type == "provisions");
    CHECK(model.name == "bread");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::modelShorthand({.type = "armor", .name = "chest.teo"}, shorthand));
    CHECK_FALSE(pistoris::paths::modelFromShorthand("model:armor:chest.teo", model));
    CHECK_FALSE(pistoris::paths::modelFromShorthand("model:npc:human_base:skins/red.teo", model));
    REQUIRE(pistoris::paths::modelFromShorthand("model:npc:human_base:skins/red.v2", model));
    CHECK(model.tweak == "skins/red.v2");
    CHECK_FALSE(pistoris::paths::modelFromShorthand("model:items:armor:chest", model));
  }

  TEST_CASE("Animation paths map interactive types to runtime animation directories") {
    const std::span<const std::string_view> types = pistoris::paths::animationTypes();
    REQUIRE(types.size() == 2);
    CHECK(types[0] == "npc");
    CHECK(types[1] == "fix_inter");

    std::string path;
    REQUIRE(pistoris::paths::animationTea({"NPC", "walk"}, path));
    CHECK(path == "graph/obj3d/anims/npc/walk.tea");

    REQUIRE(pistoris::paths::animationTea({"fix_inter", "open.TEA"}, path));
    CHECK(path == "graph/obj3d/anims/fix_inter/open.tea");

    REQUIRE(pistoris::paths::animationDirectory("armor", path));
    CHECK(path == "graph/obj3d/anims/fix_inter");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::animationTea({"armor", "open"}, path));
    CHECK_FALSE(pistoris::paths::animationTea({"invalid", "open"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::animationDirectory("invalid", path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Canonical animation paths recover animation components") {
    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromTea(R"(GRAPH\OBJ3D\ANIMS\NPC\Walk.TEA)", animation));
    CHECK(animation.type == "npc");
    CHECK(animation.name == "Walk");
    REQUIRE(pistoris::paths::animationFromTea("graph/obj3d/anims/fix_inter/open", animation));
    CHECK(animation.type == "fix_inter");
    CHECK(animation.name == "open");

    animation = {"unchanged-type", "unchanged-name"};
    CHECK_FALSE(pistoris::paths::animationFromTea("graph/obj3d/anims/items/open.tea", animation));
    CHECK(animation.type == "unchanged-type");
    CHECK(animation.name == "unchanged-name");
    CHECK_FALSE(pistoris::paths::animationFromTea("graph/obj3d/anims/npc/open.bin", animation));
    CHECK(animation.type == "unchanged-type");
    CHECK(animation.name == "unchanged-name");
  }

  TEST_CASE("Animation shorthands identify one exact TEA") {
    std::string shorthand;
    REQUIRE(pistoris::paths::animationShorthand({"NPC", "walk2.TEA"}, shorthand));
    CHECK(shorthand == "anim:npc:walk2");

    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromShorthand("ANIM:Fix_Inter:open.tea", animation));
    CHECK(animation.type == "fix_inter");
    CHECK(animation.name == "open");
    CHECK_FALSE(pistoris::paths::animationFromShorthand("anim:items:open", animation));
  }

  TEST_CASE("DLF scene paths map to the engine FTS identity") {
    std::string path;
    REQUIRE(pistoris::paths::ftsFromDlfScene(R"(Graph\Levels\Level1\)", path));
    CHECK(path == "game/Graph/Levels/Level1/fast.fts");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::ftsFromDlfScene("graph/levels/../level1", path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::ftsFromDlfScene("/graph/levels/level1", path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Level names construct canonical DLF scene directories") {
    std::string path;
    REQUIRE(pistoris::paths::dlfSceneFromLevelName("scene.v2", path));
    CHECK(path == "graph/levels/scene.v2");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::dlfSceneFromLevelName("../scene", path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::dlfSceneFromLevelName("", path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Zone ambiance paths normalize resource names and construct runtime files") {
    std::string value;
    REQUIRE(pistoris::paths::normalizeZoneAmbiance(R"(Cave\Water.AMB)", value));
    CHECK(value == "cave/water");
    REQUIRE(pistoris::paths::zoneAmbianceFile(value, value));
    CHECK(value == "sfx/ambiance/cave/water.amb");

    value = "unchanged";
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("../water", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("/cave/water", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance(R"(C:\cave\water)", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("cave/water.old.amb", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance(std::string("cave/water\0hidden", 17), value));
    CHECK(value == "unchanged");

    value = "unchanged";
    CHECK_FALSE(pistoris::paths::zoneAmbianceFile("none", value));
    CHECK(value.empty());
  }

  TEST_CASE("Cinematic and ambiance resource identities roundtrip through shorthands") {
    std::string value;
    REQUIRE(pistoris::paths::cinematicFile({"intro.CIN"}, value));
    CHECK(value == "graph/interface/illustrations/intro.cin");
    pistoris::paths::CinematicPathView cinematic;
    REQUIRE(pistoris::paths::cinematicFromFile(R"(GRAPH\INTERFACE\ILLUSTRATIONS\Intro.CIN)", cinematic));
    CHECK(cinematic.name == "Intro");
    REQUIRE(pistoris::paths::cinematicShorthand(cinematic, value));
    CHECK(value == "cinematic:Intro");
    REQUIRE(pistoris::paths::cinematicFromShorthand("CINEMATIC:intro.cin", cinematic));
    CHECK(cinematic.name == "intro");

    REQUIRE(pistoris::paths::ambianceFile({R"(cave\water.AMB)"}, value));
    CHECK(value == "sfx/ambiance/cave/water.amb");
    pistoris::paths::AmbiancePathView ambiance;
    REQUIRE(pistoris::paths::ambianceFromFile(R"(SFX\AMBIANCE\Cave\Water.AMB)", ambiance));
    CHECK(ambiance.name == R"(Cave\Water)");
    REQUIRE(pistoris::paths::ambianceShorthand(ambiance, value));
    CHECK(value == "ambiance:Cave/Water");
    REQUIRE(pistoris::paths::ambianceFromShorthand("AMBIANCE:cave/water.amb", ambiance));
    CHECK(ambiance.name == "cave/water");
    REQUIRE(pistoris::paths::ambianceFromFile("sfx/ambiance/cave/water.v2.amb", ambiance));
    REQUIRE(pistoris::paths::ambianceShorthand(ambiance, value));
    CHECK(value == "ambiance:cave/water.v2");
    REQUIRE(pistoris::paths::ambianceFromShorthand(value, ambiance));
    CHECK(ambiance.name == "cave/water.v2");
    CHECK_FALSE(pistoris::paths::ambianceFile({"none"}, value));
  }

  TEST_CASE("Resource search locations expose canonical roots and bounded depths") {
    pistoris::paths::ResourceSearchLocation location;
    REQUIRE(pistoris::paths::modelSearchLocation("npc", location));
    CHECK(location.base_path == "game/graph/obj3d/interactive/npc");
    CHECK(location.max_discovery_depth == 3);
    REQUIRE(pistoris::paths::modelSearchLocation("armor", location));
    CHECK(location.base_path == "game/graph/obj3d/interactive/items/armor");
    CHECK_FALSE(pistoris::paths::modelSearchLocation("items", location));

    REQUIRE(pistoris::paths::animationSearchLocation("fix_inter", location));
    CHECK(location.base_path == "graph/obj3d/anims/fix_inter");
    CHECK(location.max_discovery_depth == 1);
    CHECK_FALSE(pistoris::paths::animationSearchLocation("armor", location));

    CHECK(pistoris::paths::levelSearchLocation().base_path == "graph/levels");
    CHECK(pistoris::paths::levelSearchLocation().max_discovery_depth == 2);
    CHECK(pistoris::paths::cinematicSearchLocation().base_path == "graph/interface/illustrations");
    CHECK(pistoris::paths::ambianceSearchLocation().max_discovery_depth == 8);
  }
}
