// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/paths/types.h"

#include "formats/format.h"
#include "io/path_location.h"
#include "resources/animation_output.h"
#include "resources/layout.h"
#include "resources/selector.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool buildTeaTargets(std::span<const cli::AnimationOutputIdentity> animations, const cli::OutputTarget& base,
                     std::vector<cli::OutputTarget>& targets, std::string& error) {
  return cli::buildAnimationTargets(animations, base, {}, cli::Format::kTea, {}, targets, error);
}

}  // namespace

TEST_SUITE("CLI animation output") {
  TEST_CASE("Animation output identities own semantic strings") {
    const std::string resource_path = "graph/obj3d/anims/npc/walk.tea";

    const cli::AnimationOutputIdentity identity{"walk", resource_path};
    CHECK(identity.name.compare("walk") == 0);
    CHECK(identity.resource_path.compare(resource_path) == 0);
    CHECK(identity.resource_path.data() != resource_path.data());
  }

  TEST_CASE("Unnamed animations derive numbered fallback paths") {
    const std::vector<cli::AnimationOutputIdentity> animations(3);
    cli::OutputTarget base;
    base.path = "exports/provided.tea";
    base.layout = cli::ResourceLayout::kLoose;

    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(buildTeaTargets(animations, base, targets, error));
    REQUIRE(targets.size() == 3);
    CHECK(targets[0].path == "exports/provided.tea");
    CHECK(targets[1].path == "exports/provided2.tea");
    CHECK(targets[2].path == "exports/provided3.tea");
    CHECK(targets[0].layout == cli::ResourceLayout::kLoose);
  }

  TEST_CASE("Animation targets disambiguate duplicate internal names case insensitively") {
    const std::vector<cli::AnimationOutputIdentity> animations = {{"Idle", {}}, {"idle", {}}};

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(buildTeaTargets(animations, base, targets, error));
    REQUIRE(targets.size() == 2);
    CHECK(targets[0].path == "Idle.tea");
    CHECK(targets[1].path == "idle2.tea");
    CHECK(error.empty());
  }

  TEST_CASE("Animation target disambiguation reserves every natural name") {
    const std::vector<cli::AnimationOutputIdentity> animations = {{"my anim", {}}, {"my?anim", {}}, {"my_anim2", {}}};

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(buildTeaTargets(animations, base, targets, error));
    REQUIRE(targets.size() == 3);
    CHECK(targets[0].path == "my_anim.tea");
    CHECK(targets[1].path == "my_anim3.tea");
    CHECK(targets[2].path == "my_anim2.tea");
  }

  TEST_CASE("Animation targets sanitize portable reserved names") {
    const std::vector<cli::AnimationOutputIdentity> animations = {{"NUL", {}}};

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(buildTeaTargets(animations, base, targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "NUL_1.tea");
  }

  TEST_CASE("Animation targets preserve portable square brackets") {
    const std::vector<cli::AnimationOutputIdentity> animations = {{"step_[metal]", {}}};

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(buildTeaTargets(animations, base, targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "step_[metal].tea");
  }

  TEST_CASE("Animation targets reserve the primary JSON output") {
    const std::vector<cli::AnimationOutputIdentity> animations = {{"model", {}}};

    cli::OutputTarget base;
    base.path = "exports/model.json";
    base.format = cli::Format::kJson;
    std::vector<cli::OutputTarget> targets;
    std::string error;
    const std::string_view reserved_path = base.path;
    REQUIRE(cli::buildAnimationTargets(
        animations, base, {}, cli::Format::kJson, std::span(&reserved_path, 1), targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "exports/model2.json");
  }

  TEST_CASE("Resource animation targets keep the mounted output directory") {
    const std::vector<cli::AnimationOutputIdentity> animations = {{"Walk", {}}};

    cli::OutputTarget base;
    base.path = "graph/obj3d/anims/npc/provided.tea";
    base.layout = cli::ResourceLayout::kGame;
    base.selector.kind = ARX_RESOURCE_KIND_ANIMATION;
    base.selector.name = "provided";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(buildTeaTargets(animations, base, targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "graph/obj3d/anims/npc/Walk.tea");
    CHECK(targets[0].address == cli::OutputAddress::kMountRelative);
    CHECK(targets[0].layout == cli::ResourceLayout::kGame);
  }

  TEST_CASE("Game animation targets preserve explicit resource paths") {
    const std::vector<cli::AnimationOutputIdentity> animations = {
        {"generated", {}},
        {"explicit", "GRAPH/OBJ3D/ANIMS/NPC/Walk.TEA"},
    };
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildGameAnimationTargets(animations, "npc", {}, targets, error));
    REQUIRE(targets.size() == 2);
    CHECK(targets[0].path == "graph/obj3d/anims/npc/generated.tea");
    CHECK(targets[1].path == "graph/obj3d/anims/npc/Walk.tea");
    CHECK(targets[0].address == cli::OutputAddress::kMountRelative);
    CHECK(targets[1].address == cli::OutputAddress::kMountRelative);
    CHECK(targets[0].layout == cli::ResourceLayout::kGame);
    CHECK(targets[1].layout == cli::ResourceLayout::kGame);
  }

  TEST_CASE("Explicit game animation identities win collisions with inferred paths") {
    const std::vector<cli::AnimationOutputIdentity> animations = {
        {"walk", {}},
        {"different internal name", "graph/obj3d/anims/npc/walk.tea"},
    };
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildGameAnimationTargets(animations, "npc", {}, targets, error));
    REQUIRE(targets.size() == 2);
    CHECK(targets[0].path == "graph/obj3d/anims/npc/walk2.tea");
    CHECK(targets[1].path == "graph/obj3d/anims/npc/walk.tea");
  }
}
