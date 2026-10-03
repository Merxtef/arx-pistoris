// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/location.hpp"

#include <string_view>
#include <utility>
#include <variant>

namespace {

template <class Resource, class CheckNeutral, class Clear>
void checkMovedFromContract(std::string_view resource_path, CheckNeutral check_neutral, Clear clear) {
  Resource source;
  REQUIRE(source.setResourcePath(resource_path));

  Resource value(std::move(source));
  CHECK_FALSE(value.resourcePath().empty());
  check_neutral(source);  // NOLINT(bugprone-use-after-move)
  CHECK(source.validate().code() == ARX_INVALID_STATE);
  clear(source);
  check_neutral(source);

  const Resource copied(source);
  check_neutral(copied);
  CHECK(copied.validate().code() == ARX_INVALID_STATE);

  Resource assigned;
  assigned = source;
  check_neutral(assigned);
  CHECK(assigned.validate().code() == ARX_INVALID_STATE);

  Resource moved;
  moved = std::move(value);
  CHECK_FALSE(moved.resourcePath().empty());
  check_neutral(value);  // NOLINT(bugprone-use-after-move)
  CHECK(value.validate().code() == ARX_INVALID_STATE);

  REQUIRE(source.reset());
  REQUIRE(source.setResourcePath(resource_path));
  CHECK_FALSE(source.resourcePath().empty());
  CHECK(source.validate().code() != ARX_INVALID_STATE);
}

}  // namespace

TEST_SUITE("C++ resource lifetime") {
  TEST_CASE("Animation has a recoverable moved-from state") {
    checkMovedFromContract<pistoris::Animation>(
        "anim:npc:walk",
        [](const pistoris::Animation& animation) {
          CHECK(animation.resourcePath().empty());
          CHECK(animation.keyframeCount() == 0);
          CHECK(animation.keyframes().empty());
        },
        [](pistoris::Animation& animation) { animation.clearKeyframes(); });
  }

  TEST_CASE("Ambiance has a recoverable moved-from state") {
    checkMovedFromContract<pistoris::Ambiance>(
        "ambiance:cave",
        [](const pistoris::Ambiance& ambiance) {
          CHECK(ambiance.resourcePath().empty());
          CHECK(ambiance.trackCount() == 0);
          CHECK(ambiance.tracks().empty());
        },
        [](pistoris::Ambiance& ambiance) { ambiance.clearTracks(); });
  }

  TEST_CASE("Cinematic has a recoverable moved-from state") {
    checkMovedFromContract<pistoris::Cinematic>(
        "cinematic:introduction",
        [](const pistoris::Cinematic& cinematic) {
          CHECK(cinematic.resourcePath().empty());
          CHECK(cinematic.keyframeCount() == 0);
          CHECK(cinematic.keyframes().empty());
        },
        [](pistoris::Cinematic& cinematic) { cinematic.clearKeyframes(); });
  }

  TEST_CASE("Model has a recoverable moved-from state") {
    checkMovedFromContract<pistoris::Model>(
        "model:npc:human_base",
        [](const pistoris::Model& model) {
          CHECK(model.resourcePath().empty());
          CHECK(model.vertexCount() == 0);
          CHECK(model.vertices().empty());
        },
        [](pistoris::Model& model) { model.clearMesh(); });
  }

  TEST_CASE("Level has a recoverable moved-from state") {
    checkMovedFromContract<pistoris::Level>(
        "level:3",
        [](const pistoris::Level& level) {
          CHECK(level.resourcePath().empty());
          CHECK(level.entityCount() == 0);
          CHECK(level.entities().empty());
        },
        [](pistoris::Level& level) { level.clearMesh(); });
  }

  TEST_CASE("Ambiance GLB export rejects a moved-from reference Model") {
    pistoris::Model reference;
    [[maybe_unused]] pistoris::Model owner(std::move(reference));
    pistoris::Ambiance ambiance;

    auto exported = ambiance.exportGlb({}, &reference);  // NOLINT(bugprone-use-after-move)
    CHECK_FALSE(exported);
    CHECK(exported.code() == ARX_INVALID_STATE);
    REQUIRE(exported.error());
    REQUIRE(exported.error()->location());
    CHECK(std::holds_alternative<pistoris::ModelLocation>(*exported.error()->location()));
  }
}
