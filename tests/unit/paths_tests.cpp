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

  TEST_CASE("Portable resource path components preserve the broader mounted-path grammar") {
    CHECK(pistoris::paths::isPortableResourcePathComponent("My texture_(stone)&[wet]__01.png"));
    CHECK_FALSE(pistoris::paths::isPortableResourcePathComponent("folder/texture.png"));
    CHECK_FALSE(pistoris::paths::isPortableResourcePathComponent("texture#detail.png"));
    CHECK_FALSE(pistoris::paths::isPortableResourcePathComponent("CON.png"));
  }

  TEST_CASE("Common resource directories use game paths") {
    CHECK(pistoris::paths::textureDirectory() == "graph/obj3d/textures");
    CHECK(pistoris::paths::soundDirectory() == "sfx");
    CHECK(pistoris::paths::ambianceSoundDirectory() == "sfx/ambiance");
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

  TEST_CASE("Level selectors identify one canonical DLF") {
    static_assert(sizeof(ArxResourceKind) == 1);
    CHECK(pistoris::paths::resourceSelectorKind("LEVEL:invalid") == ARX_RESOURCE_KIND_LEVEL);
    CHECK(pistoris::paths::resourceSelectorKind("model:npc:hero") == ARX_RESOURCE_KIND_MODEL);
    CHECK(pistoris::paths::resourceSelectorKind("ANIM:npc:walk") == ARX_RESOURCE_KIND_ANIMATION);
    CHECK(pistoris::paths::resourceSelectorKind("cinematic:intro") == ARX_RESOURCE_KIND_CINEMATIC);
    CHECK(pistoris::paths::resourceSelectorKind("ambiance:cave") == ARX_RESOURCE_KIND_AMBIANCE);
    CHECK(pistoris::paths::resourceSelectorKind("C:\\level:17") == ARX_RESOURCE_KIND_NONE);
    CHECK(pistoris::paths::resourceSelectorKind("level") == ARX_RESOURCE_KIND_NONE);

    CHECK(pistoris::paths::levelSelector(17) == "level:17");
    std::uint32_t level = 99;
    REQUIRE(pistoris::paths::levelFromSelector("LEVEL:17", level));
    CHECK(level == 17);
    CHECK_FALSE(pistoris::paths::levelFromSelector("level:*", level));
    CHECK(level == 17);
  }

  TEST_CASE("Model paths validate and normalize interactive types") {
    const std::span<const std::string_view> types = pistoris::paths::modelSelectorTypes();
    REQUIRE(types.size() == 14);
    CHECK(types.front() == "npc");
    CHECK(types[11] == "ui-runes");
    CHECK(types[12] == "ui-menus");
    CHECK(types.back() == "editor");

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
    REQUIRE(pistoris::paths::modelFtl({.type = "ui-runes", .name = "aam"}, path));
    CHECK(path == "game/graph/interface/book/runes/aam.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = "ui-menus", .name = "main"}, path));
    CHECK(path == "game/graph/interface/menus/main.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = "editor", .name = "light"}, path));
    CHECK(path == "game/editor/obj3d/light.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = "editor", .name = "My model_(stone)&[wet]__01"}, path));
    CHECK(path == "game/editor/obj3d/My model_(stone)&[wet]__01.ftl");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "ui-runes", .name = "aam", .tweak = "red"}, path));

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
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "npc", .name = "human#alt"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = "npc", .name = "CON.ftl"}, path));
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
    REQUIRE(pistoris::paths::modelFromFtl("game/graph/interface/book/runes/aam.ftl", model));
    CHECK(model.type == "ui-runes");
    CHECK(model.name == "aam");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::modelFromFtl(R"(GAME\EDITOR\OBJ3D\Light.FTL)", model));
    CHECK(model.type == "editor");
    CHECK(model.name == "Light");

    model = {"unchanged-type", "unchanged-name", "unchanged-tweak"};
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/npc/human#alt/human#alt.ftl", model));
    CHECK(model.type == "unchanged-type");
    CHECK(model.name == "unchanged-name");
    CHECK(model.tweak == "unchanged-tweak");

    std::string class_path;
    REQUIRE(pistoris::paths::entityClassFromModel({.type = "FIX_INTER", .name = "Timed_Lever.ftl"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    REQUIRE(pistoris::paths::entityClassFromModel({.type = "npc", .name = "human_base", .tweak = "human_kultar"},
                                                  class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/human_base/tweaks/human_kultar");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == "npc");
    CHECK(model.name == "human_base");
    CHECK(model.tweak == "human_kultar");
    REQUIRE(pistoris::paths::baseEntityClassFromModel({.type = "npc", .name = "human_base", .tweak = "human_kultar"},
                                                      class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/human_base/human_base");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == "npc");
    CHECK(model.name == "human_base");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::entityClassFromModel({.type = "npc", .name = "items_dummy"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/items_dummy/items_dummy");
    REQUIRE(pistoris::paths::entityClassFromModel({.type = "npc", .name = "my__npc"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/my__npc/my__npc");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == "npc");
    CHECK(model.name == "my__npc");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::entityClassFromModel({.type = "system", .name = "plain"}, class_path));
    REQUIRE(pistoris::paths::entityClassFromModel({.type = "system", .name = "camera"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/system/camera/camera");
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
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/ui-runes/foo/foo.ftl", model));
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/items/weapons/sword/other.ftl", model));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/fix_inter/custom/door/door", model));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/items/armor/chest_chain/chest_chain.teo",
                                                      model));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/items/weapon/sword/sword", model));
    CHECK_FALSE(pistoris::paths::entityClassFromModel({.type = "fix_inter", .name = "door.part"}, class_path));
    CHECK_FALSE(pistoris::paths::entityClassFromModel({.type = "npc", .name = "human", .tweak = "../red"}, class_path));
    CHECK_FALSE(
        pistoris::paths::baseEntityClassFromModel({.type = "npc", .name = "human", .tweak = "../red"}, class_path));
    CHECK_FALSE(
        pistoris::paths::baseEntityClassFromModel({.type = "ui-menus", .name = "main", .tweak = "red"}, class_path));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/ui-runes/foo/foo", model));
    CHECK_FALSE(pistoris::paths::entityClassFromModel({.type = "ui-menus", .name = "main"}, class_path));
    CHECK(class_path == "unchanged-path");
  }

  TEST_CASE("FTL and entity class helpers preserve engine resource identity") {
    std::string path;
    REQUIRE(pistoris::paths::entityClassFromFtl("game/myitemsnew/my_item.ftl", path));
    CHECK(path == "myitemsnew/my_item");
    REQUIRE(pistoris::paths::ftlFromEntityClass("MYITEMSNEW\\My_Item", path));
    CHECK(path == "game/myitemsnew/my_item.ftl");
    REQUIRE(pistoris::paths::entityClassFromFtl("game/graph/interface/menus/items_main.ftl", path));
    CHECK(path == "graph/interface/menus/items_main");

    pistoris::paths::ModelPathView model;
    REQUIRE(pistoris::paths::modelFromEntityClass("graph/interface/menus/items_main", model));
    CHECK(model.type == "ui-menus");
    CHECK(model.name == "items_main");
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/interface/menus/main", model));

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::entityClassFromFtl("game/prefix/graph/items/foo.ftl", path));
    CHECK_FALSE(pistoris::paths::entityClassFromFtl("game/custom/resource.ftl", path));
    CHECK_FALSE(pistoris::paths::ftlFromEntityClass("prefix/graph/items/foo", path));
    CHECK_FALSE(pistoris::paths::ftlFromEntityClass("graph/items/foo.ftl", path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Model selectors use flattened types and preserve optional tweaks") {
    std::string selector;
    REQUIRE(pistoris::paths::modelSelector({.type = "Armor", .name = "chest.FTL"}, selector));
    CHECK(selector == "model:armor:chest");
    REQUIRE(
        pistoris::paths::modelSelector({.type = "npc", .name = "human_base", .tweak = R"(skins\red.ftl)"}, selector));
    CHECK(selector == "model:npc:human_base:skins/red");

    pistoris::paths::ModelPathView model;
    REQUIRE(pistoris::paths::modelFromSelector("MODEL:Provisions:bread.ftl", model));
    CHECK(model.type == "provisions");
    CHECK(model.name == "bread");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::modelSelector({.type = "armor", .name = "chest.teo"}, selector));
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:armor:chest.teo", model));
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:npc:human_base:skins/red.teo", model));
    REQUIRE(pistoris::paths::modelFromSelector("model:npc:human_base:skins/red.v2", model));
    CHECK(model.tweak == "skins/red.v2");
    REQUIRE(pistoris::paths::modelSelector({.type = "ui-runes", .name = "aam.ftl"}, selector));
    CHECK(selector == "model:ui-runes:aam");
    REQUIRE(pistoris::paths::modelFromSelector("MODEL:UI-MENUS:main.ftl", model));
    CHECK(model.type == "ui-menus");
    CHECK(model.name == "main");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:editor:light:red", model));
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:items:armor:chest", model));
  }

  TEST_CASE("Animation paths map interactive types to runtime animation directories") {
    const std::span<const std::string_view> types = pistoris::paths::animationSelectorTypes();
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
    CHECK_FALSE(pistoris::paths::animationDirectory("ui-runes", path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::animationTea({"npc", "walk#fast"}, path));
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
    CHECK_FALSE(pistoris::paths::animationFromTea("graph/obj3d/anims/npc/open#fast.tea", animation));
    CHECK(animation.type == "unchanged-type");
    CHECK(animation.name == "unchanged-name");
  }

  TEST_CASE("Animation selectors identify one exact TEA") {
    std::string selector;
    REQUIRE(pistoris::paths::animationSelector({"NPC", "walk2.TEA"}, selector));
    CHECK(selector == "anim:npc:walk2");

    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromSelector("ANIM:Fix_Inter:open.tea", animation));
    CHECK(animation.type == "fix_inter");
    CHECK(animation.name == "open");
    CHECK_FALSE(pistoris::paths::animationFromSelector("anim:items:open", animation));
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
    CHECK_FALSE(pistoris::paths::ftsFromDlfScene("graph/levels/scene#alt", path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Level names construct canonical DLF scene directories") {
    std::string path;
    REQUIRE(pistoris::paths::dlfSceneFromLevelName("scene.v2", path));
    CHECK(path == "graph/levels/scene.v2");

    path = "scene.v2";
    REQUIRE(pistoris::paths::dlfSceneFromLevelName(path, path));
    CHECK(path == "graph/levels/scene.v2");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::dlfSceneFromLevelName("../scene", path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::dlfSceneFromLevelName("", path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::dlfSceneFromLevelName("scene#alt", path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Zone ambiance paths normalize resource names and construct runtime files") {
    std::string value;
    REQUIRE(pistoris::paths::normalizeZoneAmbiance(R"(Cave\Water.AMB)", value));
    CHECK(value == "cave/water");
    REQUIRE(pistoris::paths::ambFromZoneAmbiance(value, value));
    CHECK(value == "sfx/ambiance/cave/water.amb");
    REQUIRE(pistoris::paths::normalizeZoneAmbiance("cave/water__deep.amb", value));
    CHECK(value == "cave/water__deep");
    REQUIRE(pistoris::paths::normalizeZoneAmbiance("cave/water.v2", value));
    CHECK(value == "cave/water.v2");
    REQUIRE(pistoris::paths::normalizeZoneAmbiance("cave/water.v2.amb", value));
    CHECK(value == "cave/water.v2");
    REQUIRE(pistoris::paths::ambFromZoneAmbiance(value, value));
    CHECK(value == "sfx/ambiance/cave/water.v2.amb");

    value = "unchanged";
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("../water", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("/cave/water", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance(R"(C:\cave\water)", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("cave/water?.amb", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance("ambiance:cave", value));
    CHECK(value == "unchanged");
    CHECK_FALSE(pistoris::paths::normalizeZoneAmbiance(std::string("cave/water\0hidden", 17), value));
    CHECK(value == "unchanged");

    value = "unchanged";
    CHECK_FALSE(pistoris::paths::ambFromZoneAmbiance("none", value));
    CHECK(value == "unchanged");
  }

  TEST_CASE("Cinematic and ambiance resource identities roundtrip through selectors") {
    std::string value;
    REQUIRE(pistoris::paths::cinematicCin({"intro.CIN"}, value));
    CHECK(value == "graph/interface/illustrations/intro.cin");
    pistoris::paths::CinematicPathView cinematic;
    REQUIRE(pistoris::paths::cinematicFromCin(R"(GRAPH\INTERFACE\ILLUSTRATIONS\Intro.CIN)", cinematic));
    CHECK(cinematic.name == "Intro");
    REQUIRE(pistoris::paths::cinematicSelector(cinematic, value));
    CHECK(value == "cinematic:Intro");
    REQUIRE(pistoris::paths::cinematicFromSelector("CINEMATIC:intro.cin", cinematic));
    CHECK(cinematic.name == "intro");
    value = "unchanged";
    CHECK_FALSE(pistoris::paths::cinematicCin({"intro#alt"}, value));
    CHECK(value == "unchanged");
    cinematic = {"unchanged"};
    CHECK_FALSE(pistoris::paths::cinematicFromCin("graph/interface/illustrations/intro#alt.cin", cinematic));
    CHECK(cinematic.name == "unchanged");

    REQUIRE(pistoris::paths::ambianceAmb({R"(cave\water.AMB)"}, value));
    CHECK(value == "sfx/ambiance/cave/water.amb");
    pistoris::paths::AmbiancePathView ambiance;
    REQUIRE(pistoris::paths::ambianceFromAmb(R"(SFX\AMBIANCE\Cave\Water.AMB)", ambiance));
    CHECK(ambiance.name == R"(Cave\Water)");
    REQUIRE(pistoris::paths::ambianceSelector(ambiance, value));
    CHECK(value == "ambiance:Cave/Water");
    REQUIRE(pistoris::paths::ambianceFromSelector("AMBIANCE:cave/water.amb", ambiance));
    CHECK(ambiance.name == "cave/water");
    REQUIRE(pistoris::paths::ambianceFromSelector("ambiance:cave/water__deep.amb", ambiance));
    CHECK(ambiance.name == "cave/water__deep");
    REQUIRE(pistoris::paths::ambianceFromAmb("sfx/ambiance/cave/water.v2.amb", ambiance));
    REQUIRE(pistoris::paths::ambianceSelector(ambiance, value));
    CHECK(value == "ambiance:cave/water.v2");
    REQUIRE(pistoris::paths::ambianceFromSelector(value, ambiance));
    CHECK(ambiance.name == "cave/water.v2");
    CHECK_FALSE(pistoris::paths::ambianceAmb({"none"}, value));
    value = "unchanged";
    CHECK_FALSE(pistoris::paths::ambianceAmb({"cave/water#deep"}, value));
    CHECK(value == "unchanged");
    ambiance = {"unchanged"};
    CHECK_FALSE(pistoris::paths::ambianceFromAmb("sfx/ambiance/cave/water#deep.amb", ambiance));
    CHECK(ambiance.name == "unchanged");
  }

  TEST_CASE("Resource search locations expose canonical roots and bounded depths") {
    pistoris::paths::ResourceSearchLocation location;
    REQUIRE(pistoris::paths::modelSearchLocation("npc", location));
    CHECK(location.base_path == "game/graph/obj3d/interactive/npc");
    CHECK(location.max_discovery_depth == 3);
    REQUIRE(pistoris::paths::modelSearchLocation("armor", location));
    CHECK(location.base_path == "game/graph/obj3d/interactive/items/armor");
    REQUIRE(pistoris::paths::modelSearchLocation("ui-runes", location));
    CHECK(location.base_path == "game/graph/interface/book/runes");
    CHECK(location.max_discovery_depth == 1);
    REQUIRE(pistoris::paths::modelSearchLocation("ui-menus", location));
    CHECK(location.base_path == "game/graph/interface/menus");
    REQUIRE(pistoris::paths::modelSearchLocation("editor", location));
    CHECK(location.base_path == "game/editor/obj3d");
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
