// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/paths/types.h"

#include "modules/resource.h"

#include <string>
#include <utility>

TEST_SUITE("resource module validation") {
  TEST_CASE("Repairs optional typed identities") {
    pistoris::ResourceData resource;
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_MODEL) == pistoris::resource::Error::kNone);
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_AMBIANCE) == pistoris::resource::Error::kNone);

    std::string path;
    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_MODEL, "model:npc:human_base", path) ==
            pistoris::resource::Error::kNone);
    pistoris::resource::setPath(resource, std::move(path));
    CHECK(resource.path == "game/graph/obj3d/interactive/npc/human_base/human_base.ftl");
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_MODEL) == pistoris::resource::Error::kNone);
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_AMBIANCE) == pistoris::resource::Error::kBadPath);

    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_MODEL, {}, path) == pistoris::resource::Error::kNone);
    pistoris::resource::setPath(resource, std::move(path));
    CHECK(resource.path.empty());

    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_AMBIANCE, "ambiance:cave", path) ==
            pistoris::resource::Error::kNone);
    pistoris::resource::setPath(resource, std::move(path));
    CHECK(resource.path == "sfx/ambiance/cave.amb");
  }

  TEST_CASE("Normalizes portable paths without imposing registered layouts") {
    std::string path;
    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_MODEL, R"(Graph\MY_FOLDER\My_Model.FTL)", path) ==
            pistoris::resource::Error::kNone);
    CHECK(path == "graph/my_folder/my_model.ftl");

    pistoris::ResourceData resource{path};
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_MODEL) == pistoris::resource::Error::kNone);
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_LEVEL) == pistoris::resource::Error::kBadPath);

    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_AMBIANCE, R"(custom\Caves\Deep.AMB)", path) ==
            pistoris::resource::Error::kNone);
    CHECK(path == "custom/caves/deep.amb");
    resource.path = path;
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_AMBIANCE) == pistoris::resource::Error::kNone);

    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_LEVEL, R"(Drafts\MY_LEVEL.DLF)", path) ==
            pistoris::resource::Error::kNone);
    CHECK(path == "drafts/my_level.dlf");
    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_ANIMATION, R"(Anims\MY_WALK.TEA)", path) ==
            pistoris::resource::Error::kNone);
    CHECK(path == "anims/my_walk.tea");
  }

  TEST_CASE("Rejects non-normalized and mismatched identities") {
    for (const pistoris::ResourceData& resource : {pistoris::ResourceData{"ambiance:cave"},
                                                   {"sfx/ambiance/cave"},
                                                   {R"(sfx\ambiance\cave.amb)"},
                                                   {"SFX/ambiance/cave.amb"},
                                                   {"model:npc:human_base"}})
      CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_AMBIANCE) == pistoris::resource::Error::kBadPath);

    pistoris::ResourceData resource;
    std::string path;
    REQUIRE(pistoris::resource::repairPath(ARX_RESOURCE_KIND_LEVEL, "level:1", path) ==
            pistoris::resource::Error::kNone);
    pistoris::resource::setPath(resource, std::move(path));
    CHECK(resource.path == "graph/levels/level1/level1.dlf");
    CHECK(pistoris::resource::validate(resource, ARX_RESOURCE_KIND_LEVEL) == pistoris::resource::Error::kNone);
    path = "unchanged";
    CHECK(pistoris::resource::repairPath(ARX_RESOURCE_KIND_LEVEL, "model:npc:human_base", path) ==
          pistoris::resource::Error::kBadPath);
    CHECK(path == "unchanged");
    CHECK(pistoris::resource::repairPath(ARX_RESOURCE_KIND_LEVEL, "draft/level1", path) ==
          pistoris::resource::Error::kBadPath);
    CHECK(path == "unchanged");
    CHECK(pistoris::resource::repairPath(ARX_RESOURCE_KIND_MODEL, "draft/model.teo", path) ==
          pistoris::resource::Error::kBadPath);
    CHECK(path == "unchanged");
    CHECK(pistoris::resource::repairPath(ARX_RESOURCE_KIND_CINEMATIC, "cinematic:intro", path) ==
          pistoris::resource::Error::kBadKind);
    CHECK(path == "unchanged");
  }
}
