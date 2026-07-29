// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/paths/types.h"

#include "formats/format.h"
#include "io/path_location.h"
#include "resources/animation_output.h"
#include "resources/selector.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

void setName(pistoris::tea::Data& tea, const char* name) {
  const std::string_view value(name);
  REQUIRE(value.size() < std::size(tea.name));
  std::copy(value.begin(), value.end(), std::begin(tea.name));
  tea.name[value.size()] = '\0';
}

}  // namespace

TEST_SUITE("CLI animation output") {
  TEST_CASE("Unnamed native animations derive numbered fallback paths") {
    const std::vector<pistoris::tea::Data> teas(3);
    cli::OutputTarget base;
    base.path = "exports/provided.tea";

    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildNativeAnimationTargets(teas, base, {}, targets, error));
    REQUIRE(targets.size() == 3);
    CHECK(targets[0].path == "exports/provided.tea");
    CHECK(targets[1].path == "exports/provided2.tea");
    CHECK(targets[2].path == "exports/provided3.tea");
  }

  TEST_CASE("Native animation targets disambiguate duplicate internal names case insensitively") {
    std::vector<pistoris::tea::Data> teas(2);
    setName(teas[0], "Idle");
    setName(teas[1], "idle");

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildNativeAnimationTargets(teas, base, {}, targets, error));
    REQUIRE(targets.size() == 2);
    CHECK(targets[0].path == "Idle.tea");
    CHECK(targets[1].path == "idle2.tea");
    CHECK(error.empty());
  }

  TEST_CASE("Animation target disambiguation reserves every natural name") {
    std::vector<pistoris::tea::Data> teas(3);
    setName(teas[0], "my anim");
    setName(teas[1], "my?anim");
    setName(teas[2], "my_anim2");

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildNativeAnimationTargets(teas, base, {}, targets, error));
    REQUIRE(targets.size() == 3);
    CHECK(targets[0].path == "my_anim.tea");
    CHECK(targets[1].path == "my_anim3.tea");
    CHECK(targets[2].path == "my_anim2.tea");
  }

  TEST_CASE("Animation targets sanitize portable reserved names") {
    std::vector<pistoris::tea::Data> teas(1);
    setName(teas[0], "NUL");

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildNativeAnimationTargets(teas, base, {}, targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "NUL_1.tea");
  }

  TEST_CASE("Animation targets preserve portable square brackets") {
    std::vector<pistoris::tea::Data> teas(1);
    setName(teas[0], "step_[metal]");

    cli::OutputTarget base;
    base.path = "provided.tea";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildNativeAnimationTargets(teas, base, {}, targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "step_[metal].tea");
  }

  TEST_CASE("Animation targets reserve the primary JSON output") {
    std::vector<pistoris::tea::Data> teas(1);
    setName(teas[0], "model");

    cli::OutputTarget base;
    base.path = "exports/model.json";
    base.format = cli::Format::kJson;
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildAnimationTargets(teas, base, {}, cli::Format::kJson, std::span(&base, 1), targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "exports/model2.json");
  }

  TEST_CASE("Resource animation targets keep the mounted output directory") {
    std::vector<pistoris::tea::Data> teas(1);
    setName(teas[0], "Walk");

    cli::OutputTarget base;
    base.path = "graph/obj3d/anims/npc/provided.tea";
    base.selector.kind = ARX_RESOURCE_KIND_ANIMATION;
    base.selector.name = "provided";
    std::vector<cli::OutputTarget> targets;
    std::string error;
    REQUIRE(cli::buildNativeAnimationTargets(teas, base, {}, targets, error));
    REQUIRE(targets.size() == 1);
    CHECK(targets[0].path == "graph/obj3d/anims/npc/Walk.tea");
    CHECK(targets[0].address == cli::OutputAddress::kMountRelative);
  }
}
