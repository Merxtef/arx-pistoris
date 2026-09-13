// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/paths/types.h"

#include "base/resource_path.h"
#include "formats/format.h"
#include "resources/selector.h"

#include <string>

TEST_SUITE("CLI resource selectors") {
  TEST_CASE("Level selector constructs canonical DLF identity") {
    cli::ResourceSelector selector;
    std::string error;
    REQUIRE(cli::parseResourceSelector("level:17", selector, error) == cli::SelectorParseStatus::kValid);
    CHECK(selector.kind == ARX_RESOURCE_KIND_LEVEL);
    CHECK(selector.level == 17);
    CHECK(selector.name == "level17");
    CHECK(selector.logical_path == "graph/levels/level17/level17.dlf");
  }

  TEST_CASE("Typed selectors construct conventional resource paths") {
    cli::ResourceSelector selector;
    std::string error;
    REQUIRE(cli::parseResourceSelector("anim:NPC:walk", selector, error) == cli::SelectorParseStatus::kValid);
    CHECK(selector.type == "npc");
    CHECK(selector.name == "walk");
    CHECK(selector.logical_path == "graph/obj3d/anims/npc/walk.tea");

    REQUIRE(cli::parseResourceSelector("model:Armor:key.ftl", selector, error) == cli::SelectorParseStatus::kValid);
    CHECK(selector.type == "armor");
    CHECK(selector.name == "key");
    CHECK(selector.logical_path == "game/graph/obj3d/interactive/items/armor/key/key.ftl");

    REQUIRE(cli::parseResourceSelector("model:npc:hero", selector, error) == cli::SelectorParseStatus::kValid);
    CHECK(selector.logical_path == "game/graph/obj3d/interactive/npc/hero/hero.ftl");
    REQUIRE(cli::parseResourceSelector("model:npc:hero:skins/red", selector, error) ==
            cli::SelectorParseStatus::kValid);
    CHECK(selector.tweak == "skins/red");
    CHECK(selector.logical_path == "game/graph/obj3d/interactive/npc/hero/tweaks/skins/red.ftl");

    REQUIRE(cli::parseResourceSelector("cinematic:intro", selector, error) == cli::SelectorParseStatus::kValid);
    CHECK(selector.kind == ARX_RESOURCE_KIND_CINEMATIC);
    CHECK(selector.logical_path == "graph/interface/illustrations/intro.cin");
    REQUIRE(cli::parseResourceSelector("ambiance:cave/water", selector, error) == cli::SelectorParseStatus::kValid);
    CHECK(selector.kind == ARX_RESOURCE_KIND_AMBIANCE);
    CHECK(selector.logical_path == "sfx/ambiance/cave/water.amb");
  }

  TEST_CASE("Selector roots types and traversal are validated") {
    cli::ResourceSelector selector;
    std::string error;
    CHECK(cli::parseResourceSelector("level:", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("anim:items:walk", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("anim:npc:.tea", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("model:npc:.ftl", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("model:npc:hero.teo", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("model:items:key", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("model:weapon:sword", selector, error) == cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("model:graph/obj3d/interactive/npc/a.ftl", selector, error) ==
          cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("anim:graph/obj3d/anims/npc/../walk.tea", selector, error) ==
          cli::SelectorParseStatus::kInvalid);
    CHECK(cli::parseResourceSelector("C:\\files\\model.ftl", selector, error) ==
          cli::SelectorParseStatus::kNotSelector);
  }

  TEST_CASE("Resource stems can preserve unregistered suffixes") {
    CHECK(cli::resourceStem("folder/scene.dlf") == "scene");
    CHECK(cli::resourceStem("folder/scene.v2") == "scene");
    CHECK(cli::resourceFormatStem("folder/scene.dlf") == "scene");
    CHECK(cli::resourceFormatStem("folder/scene.DLF") == "scene");
    CHECK(cli::resourceFormatStem("folder/ambiance.AMB") == "ambiance");
    CHECK(cli::resourceFormatStem("folder/scene.v2") == "scene.v2");
    CHECK(cli::resourceFormatStem("folder/scene.dlf.bak") == "scene.dlf.bak");
  }
}
