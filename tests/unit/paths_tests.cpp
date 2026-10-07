// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include <array>
#include <cstdint>
#include <limits>
#include <ostream>  // IWYU pragma: keep
#include <span>
#include <string>
#include <string_view>

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

  TEST_CASE("Model paths validate typed resource families") {
    const std::span<const pistoris::paths::ModelPathType> types = pistoris::paths::modelPathTypes();
    REQUIRE(types.size() == 14);
    CHECK(types.front() == pistoris::paths::ModelPathType::kNpc);
    CHECK(types[11] == pistoris::paths::ModelPathType::kUiRunes);
    CHECK(types[12] == pistoris::paths::ModelPathType::kUiMenus);
    CHECK(types.back() == pistoris::paths::ModelPathType::kEditor);
    CHECK(pistoris::paths::modelPathTypeName(types.front()) == "npc");
    CHECK(pistoris::paths::modelPathTypeName(types[11]) == "ui-runes");
    pistoris::paths::ModelPathType parsed_type = pistoris::paths::ModelPathType::kNone;
    REQUIRE(pistoris::paths::modelPathTypeFromName("NPC", parsed_type));
    CHECK(parsed_type == pistoris::paths::ModelPathType::kNpc);
    CHECK_FALSE(pistoris::paths::modelPathTypeFromName("items", parsed_type));

    std::string path;
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = "my_npc"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/my_npc/my_npc.ftl");

    for (pistoris::paths::ModelPathType type : {pistoris::paths::ModelPathType::kArmor,
                                                pistoris::paths::ModelPathType::kJewelry,
                                                pistoris::paths::ModelPathType::kMagic,
                                                pistoris::paths::ModelPathType::kMovable,
                                                pistoris::paths::ModelPathType::kProvisions,
                                                pistoris::paths::ModelPathType::kQuestItem,
                                                pistoris::paths::ModelPathType::kSpecial,
                                                pistoris::paths::ModelPathType::kWeapons}) {
      REQUIRE(pistoris::paths::modelFtl({.type = type, .name = "key.ftl"}, path));
      CHECK(path == "game/graph/obj3d/interactive/items/" + std::string(pistoris::paths::modelPathTypeName(type)) +
                        "/key/key.ftl");
    }
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kSystem, .name = "camera"}, path));
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kFixInter, .name = "door"}, path));
    REQUIRE(pistoris::paths::modelFtl(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human_base", .tweak = "skin/red.ftl"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.ftl");
    REQUIRE(pistoris::paths::modelFtl(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human_base", .tweak = "skin/red.v2"}, path));
    CHECK(path == "game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.v2.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kUiRunes, .name = "aam"}, path));
    CHECK(path == "game/graph/interface/book/runes/aam.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kUiMenus, .name = "main"}, path));
    CHECK(path == "game/graph/interface/menus/main.ftl");
    REQUIRE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kEditor, .name = "light"}, path));
    CHECK(path == "game/editor/obj3d/light.ftl");
    REQUIRE(pistoris::paths::modelFtl(
        {.type = pistoris::paths::ModelPathType::kEditor, .name = "My model_(stone)&[wet]__01"}, path));
    CHECK(path == "game/editor/obj3d/My model_(stone)&[wet]__01.ftl");
    CHECK_FALSE(pistoris::paths::modelFtl(
        {.type = pistoris::paths::ModelPathType::kUiRunes, .name = "aam", .tweak = "red"}, path));

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNone, .name = "model"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(
        pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = "folder/model"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = ".ftl"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = "human.TEO"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human", .tweak = "skins/red.teo"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = "human#alt"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::modelFtl({.type = pistoris::paths::ModelPathType::kNpc, .name = "CON.ftl"}, path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Canonical model paths and entity classes recover model components") {
    pistoris::paths::ModelPathView model;
    REQUIRE(
        pistoris::paths::modelFromFtl(R"(GAME\GRAPH\OBJ3D\INTERACTIVE\Fix_Inter\Timed_Lever\Timed_Lever.FTL)", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kFixInter);
    CHECK(model.name == "Timed_Lever");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/items/weapons/long_sword/long_sword", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kWeapons);
    CHECK(model.name == "long_sword");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/npc/human_base/tweaks/skin/red.ftl", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kNpc);
    CHECK(model.name == "human_base");
    CHECK(model.tweak == "skin/red");
    REQUIRE(pistoris::paths::modelFromFtl("game/graph/interface/book/runes/aam.ftl", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kUiRunes);
    CHECK(model.name == "aam");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::modelFromFtl(R"(GAME\EDITOR\OBJ3D\Light.FTL)", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kEditor);
    CHECK(model.name == "Light");

    model = {pistoris::paths::ModelPathType::kEditor, "unchanged-name", "unchanged-tweak"};
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/npc/human#alt/human#alt.ftl", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kEditor);
    CHECK(model.name == "unchanged-name");
    CHECK(model.tweak == "unchanged-tweak");

    std::string class_path;
    REQUIRE(pistoris::paths::entityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kFixInter, .name = "Timed_Lever.ftl"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    REQUIRE(pistoris::paths::entityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human_base", .tweak = "human_kultar"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/human_base/tweaks/human_kultar");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == pistoris::paths::ModelPathType::kNpc);
    CHECK(model.name == "human_base");
    CHECK(model.tweak == "human_kultar");
    REQUIRE(pistoris::paths::baseEntityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human_base", .tweak = "human_kultar"}, class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/human_base/human_base");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == pistoris::paths::ModelPathType::kNpc);
    CHECK(model.name == "human_base");
    CHECK(model.tweak.empty());
    REQUIRE(pistoris::paths::entityClassFromModel({.type = pistoris::paths::ModelPathType::kNpc, .name = "items_dummy"},
                                                  class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/items_dummy/items_dummy");
    REQUIRE(pistoris::paths::entityClassFromModel({.type = pistoris::paths::ModelPathType::kNpc, .name = "my__npc"},
                                                  class_path));
    CHECK(class_path == "graph/obj3d/interactive/npc/my__npc/my__npc");
    REQUIRE(pistoris::paths::modelFromEntityClass(class_path, model));
    CHECK(model.type == pistoris::paths::ModelPathType::kNpc);
    CHECK(model.name == "my__npc");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::entityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kSystem, .name = "plain"}, class_path));
    REQUIRE(pistoris::paths::entityClassFromModel({.type = pistoris::paths::ModelPathType::kSystem, .name = "camera"},
                                                  class_path));
    CHECK(class_path == "graph/obj3d/interactive/system/camera/camera");
    model = {pistoris::paths::ModelPathType::kEditor, "unchanged-name", "unchanged-tweak"};
    class_path = "unchanged-path";
    CHECK_FALSE(pistoris::paths::modelFromFtl("game/graph/obj3d/interactive/fix_inter/door/other.ftl", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kEditor);
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
    CHECK_FALSE(pistoris::paths::entityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kFixInter, .name = "door.part"}, class_path));
    CHECK_FALSE(pistoris::paths::entityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human", .tweak = "../red"}, class_path));
    CHECK_FALSE(pistoris::paths::baseEntityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human", .tweak = "../red"}, class_path));
    CHECK_FALSE(pistoris::paths::baseEntityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kUiMenus, .name = "main", .tweak = "red"}, class_path));
    CHECK_FALSE(pistoris::paths::modelFromEntityClass("graph/obj3d/interactive/ui-runes/foo/foo", model));
    CHECK_FALSE(pistoris::paths::entityClassFromModel(
        {.type = pistoris::paths::ModelPathType::kUiMenus, .name = "main"}, class_path));
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
    CHECK(model.type == pistoris::paths::ModelPathType::kUiMenus);
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
    REQUIRE(pistoris::paths::modelSelector({.type = pistoris::paths::ModelPathType::kArmor, .name = "chest.FTL"},
                                           selector));
    CHECK(selector == "model:armor:chest");
    REQUIRE(pistoris::paths::modelSelector(
        {.type = pistoris::paths::ModelPathType::kNpc, .name = "human_base", .tweak = R"(skins\red.ftl)"}, selector));
    CHECK(selector == "model:npc:human_base:skins/red");

    pistoris::paths::ModelPathView model;
    REQUIRE(pistoris::paths::modelFromSelector("MODEL:Provisions:bread.ftl", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kProvisions);
    CHECK(model.name == "bread");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::modelSelector({.type = pistoris::paths::ModelPathType::kArmor, .name = "chest.teo"},
                                               selector));
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:armor:chest.teo", model));
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:npc:human_base:skins/red.teo", model));
    REQUIRE(pistoris::paths::modelFromSelector("model:npc:human_base:skins/red.v2", model));
    CHECK(model.tweak == "skins/red.v2");
    REQUIRE(pistoris::paths::modelSelector({.type = pistoris::paths::ModelPathType::kUiRunes, .name = "aam.ftl"},
                                           selector));
    CHECK(selector == "model:ui-runes:aam");
    REQUIRE(pistoris::paths::modelFromSelector("MODEL:UI-MENUS:main.ftl", model));
    CHECK(model.type == pistoris::paths::ModelPathType::kUiMenus);
    CHECK(model.name == "main");
    CHECK(model.tweak.empty());
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:editor:light:red", model));
    CHECK_FALSE(pistoris::paths::modelFromSelector("model:items:armor:chest", model));
  }

  TEST_CASE("Animation paths map interactive types to runtime animation directories") {
    const std::span<const pistoris::paths::AnimationPathType> types = pistoris::paths::animationPathTypes();
    REQUIRE(types.size() == 2);
    CHECK(types[0] == pistoris::paths::AnimationPathType::kNpc);
    CHECK(types[1] == pistoris::paths::AnimationPathType::kFixInter);
    CHECK(pistoris::paths::animationPathTypeName(types[0]) == "npc");
    pistoris::paths::AnimationPathType parsed_type = pistoris::paths::AnimationPathType::kNone;
    REQUIRE(pistoris::paths::animationPathTypeFromName("FIX_INTER", parsed_type));
    CHECK(parsed_type == pistoris::paths::AnimationPathType::kFixInter);
    CHECK_FALSE(pistoris::paths::animationPathTypeFromName("armor", parsed_type));

    std::string path;
    REQUIRE(pistoris::paths::animationTea({pistoris::paths::AnimationPathType::kNpc, "walk"}, path));
    CHECK(path == "graph/obj3d/anims/npc/walk.tea");

    REQUIRE(pistoris::paths::animationTea({pistoris::paths::AnimationPathType::kFixInter, "open.TEA"}, path));
    CHECK(path == "graph/obj3d/anims/fix_inter/open.tea");

    REQUIRE(pistoris::paths::animationDirectory(pistoris::paths::ModelPathType::kArmor, path));
    CHECK(path == "graph/obj3d/anims/fix_inter");
    REQUIRE(pistoris::paths::animationDirectory(pistoris::paths::AnimationPathType::kNpc, path));
    CHECK(path == "graph/obj3d/anims/npc");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::animationTea({pistoris::paths::AnimationPathType::kNone, "open"}, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::animationDirectory(pistoris::paths::ModelPathType::kNone, path));
    CHECK_FALSE(pistoris::paths::animationDirectory(pistoris::paths::ModelPathType::kUiRunes, path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::animationTea({pistoris::paths::AnimationPathType::kNpc, "walk#fast"}, path));
    CHECK(path == "unchanged");
  }

  TEST_CASE("Canonical animation paths recover animation components") {
    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromTea(R"(GRAPH\OBJ3D\ANIMS\NPC\Walk.TEA)", animation));
    CHECK(animation.type == pistoris::paths::AnimationPathType::kNpc);
    CHECK(animation.name == "Walk");
    REQUIRE(pistoris::paths::animationFromTea("graph/obj3d/anims/fix_inter/open", animation));
    CHECK(animation.type == pistoris::paths::AnimationPathType::kFixInter);
    CHECK(animation.name == "open");

    animation = {pistoris::paths::AnimationPathType::kFixInter, "unchanged-name"};
    CHECK_FALSE(pistoris::paths::animationFromTea("graph/obj3d/anims/items/open.tea", animation));
    CHECK(animation.type == pistoris::paths::AnimationPathType::kFixInter);
    CHECK(animation.name == "unchanged-name");
    CHECK_FALSE(pistoris::paths::animationFromTea("graph/obj3d/anims/npc/open.bin", animation));
    CHECK(animation.type == pistoris::paths::AnimationPathType::kFixInter);
    CHECK(animation.name == "unchanged-name");
    CHECK_FALSE(pistoris::paths::animationFromTea("graph/obj3d/anims/npc/open#fast.tea", animation));
    CHECK(animation.type == pistoris::paths::AnimationPathType::kFixInter);
    CHECK(animation.name == "unchanged-name");
  }

  TEST_CASE("Animation selectors identify one exact TEA") {
    std::string selector;
    REQUIRE(pistoris::paths::animationSelector({pistoris::paths::AnimationPathType::kNpc, "walk2.TEA"}, selector));
    CHECK(selector == "anim:npc:walk2");

    pistoris::paths::AnimationPathView animation;
    REQUIRE(pistoris::paths::animationFromSelector("ANIM:Fix_Inter:open.tea", animation));
    CHECK(animation.type == pistoris::paths::AnimationPathType::kFixInter);
    CHECK(animation.name == "open");
    CHECK_FALSE(pistoris::paths::animationFromSelector("anim:items:open", animation));
  }

  TEST_CASE("DLF scene paths map to the engine FTS identity") {
    std::string path;
    REQUIRE(pistoris::paths::ftsFromDlfScene(R"(Graph\Levels\Level1\)", path));
    CHECK(path == "game/Graph/Levels/Level1/fast.fts");
    REQUIRE(pistoris::paths::ftsFromDlfScene("my_folder", path));
    CHECK(path == "game/my_folder/fast.fts");
    REQUIRE(pistoris::paths::ftsFromDlfScene("prefix/graph/levels/level1", path));
    CHECK(path == "game/prefix/graph/levels/level1/fast.fts");
    REQUIRE(pistoris::paths::ftsFromDlfScene("../custom", path));
    CHECK(path == "custom/fast.fts");
    REQUIRE(pistoris::paths::ftsFromDlfScene("..", path));
    CHECK(path == "fast.fts");
    REQUIRE(pistoris::paths::ftsFromDlfScene("graph/levels/../level1", path));
    CHECK(path == "game/graph/level1/fast.fts");

    path = "unchanged";
    CHECK_FALSE(pistoris::paths::ftsFromDlfScene("../../custom", path));
    CHECK(path == "unchanged");
    CHECK_FALSE(pistoris::paths::ftsFromDlfScene("../..", path));
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

  TEST_CASE("Owning resource selectors parse and rebuild every resource kind") {
    constexpr std::array<std::string_view, 5> kSelectors = {"level:17",
                                                            "model:npc:human_base:tweaks/red",
                                                            "anim:fix_inter:lever",
                                                            "cinematic:intro",
                                                            "ambiance:cave/water"};
    for (std::string_view selector : kSelectors) {
      pistoris::paths::ResourceSelector resource;
      REQUIRE(pistoris::paths::parseResourceSelector(selector, resource));
      std::string rebuilt;
      REQUIRE(pistoris::paths::resourceSelector(resource, rebuilt));
      CHECK(rebuilt == selector);
    }

    pistoris::paths::ResourceSelector unchanged;
    unchanged.kind = ARX_RESOURCE_KIND_CINEMATIC;
    unchanged.name = "unchanged";
    CHECK_FALSE(pistoris::paths::parseResourceSelector("level:not-a-number", unchanged));
    CHECK(unchanged.kind == ARX_RESOURCE_KIND_CINEMATIC);
    CHECK(unchanged.name == "unchanged");
    CHECK_FALSE(pistoris::paths::parseResourceSelector("texture:stone", unchanged));
    CHECK(unchanged.kind == ARX_RESOURCE_KIND_CINEMATIC);
    CHECK(unchanged.name == "unchanged");

    pistoris::paths::ResourceSelector invalid;
    std::string unchanged_text = "unchanged";
    CHECK_FALSE(pistoris::paths::resourceSelector(invalid, unchanged_text));
    CHECK(unchanged_text == "unchanged");

    const auto check_incoherent = [](const pistoris::paths::ResourceSelector& resource) {
      std::string text = "unchanged";
      CHECK_FALSE(pistoris::paths::resourceSelector(resource, text));
      CHECK(text == "unchanged");
    };
    REQUIRE(pistoris::paths::parseResourceSelector("level:17", invalid));
    invalid.name = "ignored";
    check_incoherent(invalid);
    REQUIRE(pistoris::paths::parseResourceSelector("model:npc:human_base", invalid));
    invalid.animation_type = pistoris::paths::AnimationPathType::kNpc;
    check_incoherent(invalid);
    REQUIRE(pistoris::paths::parseResourceSelector("anim:npc:human_male_wait", invalid));
    invalid.tweak = "ignored";
    check_incoherent(invalid);
    REQUIRE(pistoris::paths::parseResourceSelector("cinematic:intro", invalid));
    invalid.level = 1;
    check_incoherent(invalid);
    REQUIRE(pistoris::paths::parseResourceSelector("ambiance:cave", invalid));
    invalid.model_type = pistoris::paths::ModelPathType::kNpc;
    check_incoherent(invalid);
  }

  TEST_CASE("Resource search locations expose canonical roots and bounded depths") {
    pistoris::paths::ResourceSearchLocation location;
    REQUIRE(pistoris::paths::modelSearchLocation(pistoris::paths::ModelPathType::kNpc, location));
    CHECK(location.base_path == "game/graph/obj3d/interactive/npc");
    CHECK(location.max_discovery_depth == 3);
    REQUIRE(pistoris::paths::modelSearchLocation(pistoris::paths::ModelPathType::kArmor, location));
    CHECK(location.base_path == "game/graph/obj3d/interactive/items/armor");
    REQUIRE(pistoris::paths::modelSearchLocation(pistoris::paths::ModelPathType::kUiRunes, location));
    CHECK(location.base_path == "game/graph/interface/book/runes");
    CHECK(location.max_discovery_depth == 1);
    REQUIRE(pistoris::paths::modelSearchLocation(pistoris::paths::ModelPathType::kUiMenus, location));
    CHECK(location.base_path == "game/graph/interface/menus");
    REQUIRE(pistoris::paths::modelSearchLocation(pistoris::paths::ModelPathType::kEditor, location));
    CHECK(location.base_path == "game/editor/obj3d");
    CHECK_FALSE(pistoris::paths::modelSearchLocation(pistoris::paths::ModelPathType::kNone, location));

    REQUIRE(pistoris::paths::animationSearchLocation(pistoris::paths::AnimationPathType::kFixInter, location));
    CHECK(location.base_path == "graph/obj3d/anims/fix_inter");
    CHECK(location.max_discovery_depth == 1);
    CHECK_FALSE(pistoris::paths::animationSearchLocation(pistoris::paths::AnimationPathType::kNone, location));

    CHECK(pistoris::paths::levelSearchLocation().base_path == "graph/levels");
    CHECK(pistoris::paths::levelSearchLocation().max_discovery_depth == 2);
    CHECK(pistoris::paths::cinematicIllustrationDirectory() == "graph/interface/illustrations");
    CHECK(pistoris::paths::cinematicSearchLocation().base_path == "graph/interface/illustrations");
    CHECK(pistoris::paths::ambianceSearchLocation().max_discovery_depth == 8);
  }
}
