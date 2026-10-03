// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"

#include "helpers.h"
#include "native/fixed_string.h"
#include "nlohmann/json.hpp"
#include "support/native_equivalence.h"

#include <string_view>
#include <utility>

namespace {

pistoris::Fts makeFtsWithPortal() {
  pistoris::Fts result = makeMinimalFtsData();
  result.scene.num_rooms = 1;
  result.scene.num_portals = 1;
  result.scene.sizex = 160;
  result.scene.sizez = 160;
  result.cells.resize(160U * 160U);
  result.rooms.resize(2);
  result.room_distances.resize(4);
  result.portals.resize(1);

  pistoris::fts::Portal& portal = result.portals[0];
  portal.poly.type = pistoris::kFaceBitQuad;
  portal.poly.min = {0.0f, 0.0f, 0.0f};
  portal.poly.max = {100.0f, 100.0f, 0.0f};
  portal.poly.norm = {0.0f, 0.0f, 1.0f};
  portal.poly.norm2 = portal.poly.norm;
  portal.poly.center = {50.0f, 50.0f, 0.0f};
  portal.poly.v[0].pos = {0.0f, 0.0f, 0.0f};
  portal.poly.v[1].pos = {100.0f, 0.0f, 0.0f};
  portal.poly.v[2].pos = {0.0f, 100.0f, 0.0f};
  portal.poly.v[3].pos = {100.0f, 100.0f, 0.0f};
  portal.poly.v[0].rhw = 1.0f;
  portal.poly.v[1].rhw = 2.0f;
  portal.poly.v[2].rhw = 3.0f;
  portal.poly.v[3].rhw = 4.0f;
  portal.room_1 = 0;
  portal.room_2 = 1;
  portal.useportal = 7;

  result.rooms[0].data.num_portals = 1;
  result.rooms[0].portal_ids.push_back(0);
  result.rooms[1].data.num_portals = 1;
  result.rooms[1].portal_ids.push_back(0);
  return result;
}

pistoris::Ftl makeFtlWithSemanticReferences() {
  pistoris::Ftl result = makeData(1);
  REQUIRE(pistoris::copyFixedString("test", result.header.name));

  pistoris::ftl::Face face;
  face.vertex_idx = {0, 0, 0};
  result.faces.push_back(face);

  pistoris::ftl::Group group;
  REQUIRE(pistoris::copyFixedString("root", group.name));
  group.origin = 0;
  group.indices.push_back(0);
  result.groups.push_back(std::move(group));

  pistoris::ftl::Action action;
  REQUIRE(pistoris::copyFixedString("attach", action.name));
  action.vertex_idx = 0;
  result.actions.push_back(action);

  pistoris::ftl::Selection selection;
  REQUIRE(pistoris::copyFixedString("selected", selection.name));
  selection.selected.push_back(0);
  result.selections.push_back(std::move(selection));
  return result;
}

pistoris::Fts makeFtsWithSemanticReferences() {
  pistoris::Fts result = makeFtsWithPortal();
  result.anchors.resize(1);
  result.scene.num_anchors = 1;
  result.cells[0].anchor_ids.push_back(0);

  pistoris::fts::Poly polygon;
  polygon.v[1].ssx = 1.0f;
  polygon.v[2].ssz = 1.0f;
  result.cells[0].polygons.push_back(polygon);
  result.scene.num_polys = 1;
  result.rooms[0].data.num_polys = 1;
  result.rooms[0].polygons.push_back({0, 0, 0, 0});
  return result;
}

void checkFtlJsonFailure(const nlohmann::json& json, ArxReturnCode code, std::string_view pointer) {
  const auto result = pistoris::fromFtlJson(json.dump());
  REQUIRE_FALSE(result);
  CHECK(result.code() == code);
  REQUIRE(result.error());
  REQUIRE(result.error()->location());
  CHECK(result.error()->location()->pointer == pointer);
}

void checkFtsJsonFailure(const nlohmann::json& json, ArxReturnCode code, std::string_view pointer) {
  const auto result = pistoris::fromFtsJson(json.dump());
  REQUIRE_FALSE(result);
  CHECK(result.code() == code);
  REQUIRE(result.error());
  REQUIRE(result.error()->location());
  CHECK(result.error()->location()->pointer == pointer);
}

pistoris::Dlf makeDlfWithFog() {
  pistoris::Dlf result;
  REQUIRE(pistoris::copyFixedString("graph/levels/level1", result.scene_path));
  result.player_spawn.position = {10.0f, 20.0f, 30.0f};
  result.player_spawn.angle = {5.0f, 10.0f, 15.0f};
  result.fogs.push_back({
      .position = {100.0f, 200.0f, 300.0f},
      .color = {0.2f, 0.4f, 0.8f},
      .size = 12.0f,
      .directional = true,
      .scale = 1.5f,
      .angle = {10.0f, 20.0f, 30.0f},
      .speed = 2.0f,
      .rotate_speed = 3.0f,
      .lifetime_ms = 600,
      .frequency = 7.0f,
  });
  return result;
}

}  // namespace

TEST_SUITE("json::feature_roundtrip") {
  TEST_CASE("FTS portal survives JSON roundtrip") {
    const pistoris::Fts source = makeFtsWithPortal();
    REQUIRE(pistoris::validate(source));

    auto encoded = pistoris::toFtsJson(source, 1);
    REQUIRE(encoded);

    auto roundtrip = pistoris::fromFtsJson(*encoded);
    REQUIRE(roundtrip);
    CHECK(roundtrip->level == 1);
    test_support::checkEquivalent(source, roundtrip->fts);

    nlohmann::json malformed = nlohmann::json::parse(*encoded);
    malformed["portals"][0]["room1"] = "invalid";
    auto rejected = pistoris::fromFtsJson(malformed.dump());
    CHECK(rejected.code() == ARX_JSON_BAD_SCHEMA);
  }

  TEST_CASE("Native semantic locations use carrier member names") {
    const pistoris::Ftl ftl = makeFtlWithSemanticReferences();
    REQUIRE(pistoris::validate(ftl));

    SUBCASE("FTL nested members") {
      pistoris::Ftl invalid = ftl;
      invalid.faces[0].vertex_idx.x = 1;
      auto result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "vertex_idx");
      CHECK(result.error()->location()->subindex == 0);

      invalid = ftl;
      invalid.groups[0].indices[0] = 1;
      result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "indices");
      CHECK(result.error()->location()->subindex == 0);

      invalid = ftl;
      invalid.actions[0].vertex_idx = 1;
      result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "vertex_idx");

      invalid = ftl;
      invalid.selections[0].selected[0] = 1;
      result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "selected");
      CHECK(result.error()->location()->subindex == 0);
    }

    SUBCASE("FTS nested members") {
      const pistoris::Fts fts = makeFtsWithSemanticReferences();
      REQUIRE(pistoris::validate(fts));

      pistoris::Fts invalid = fts;
      invalid.cells[0].anchor_ids[0] = 1;
      auto result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "anchor_ids");
      CHECK(result.error()->location()->subindex == 0);

      invalid = fts;
      invalid.cells[0].polygons[0].v[2].ssx = 16001.0f;
      result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "v");
      CHECK(result.error()->location()->subindex == 2);

      invalid = fts;
      invalid.portals[0].poly.v[2].pos.x = 16001.0f;
      result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "poly.v");
      CHECK(result.error()->location()->subindex == 2);

      invalid = fts;
      invalid.rooms[0].portal_ids[0] = 1;
      result = pistoris::validate(invalid);
      REQUIRE_FALSE(result);
      REQUIRE(result.error());
      REQUIRE(result.error()->location());
      CHECK(result.error()->location()->field == "portal_ids");
      CHECK(result.error()->location()->subindex == 0);
    }
  }

  TEST_CASE("FTL semantic failures map carrier members to JSON properties") {
    const auto encoded = pistoris::toFtlJson(makeFtlWithSemanticReferences());
    REQUIRE(encoded);
    const nlohmann::json source = nlohmann::json::parse(*encoded);

    nlohmann::json invalid = source;
    invalid["groups"][0]["indices"][0] = 1;
    checkFtlJsonFailure(invalid, ARX_FTL_BAD_GROUP_IDX, "/groups/0/indices/0");

    invalid = source;
    invalid["actions"][0]["vertexIdx"] = 1;
    checkFtlJsonFailure(invalid, ARX_FTL_BAD_ACTION_VERT_IDX, "/actions/0/vertexIdx");

    invalid = source;
    invalid["selections"][0]["selected"][0] = 1;
    checkFtlJsonFailure(invalid, ARX_FTL_BAD_SEL_IDX, "/selections/0/selected/0");
  }

  TEST_CASE("FTS semantic failures map carrier members to JSON properties") {
    const auto encoded = pistoris::toFtsJson(makeFtsWithSemanticReferences(), 1);
    REQUIRE(encoded);
    const nlohmann::json source = nlohmann::json::parse(*encoded);

    nlohmann::json invalid = source;
    invalid["cells"][0]["anchors"][0] = 1;
    checkFtsJsonFailure(invalid, ARX_FTS_BAD_ANCHOR_INDEX, "/cells/0/anchors/0");

    invalid = source;
    invalid["polygons"][0]["vertices"][2]["x"] = 16001.0f;
    checkFtsJsonFailure(invalid, ARX_FTS_BAD_POLYGON_POSITION, "/polygons/0/vertices/2");

    invalid = source;
    invalid["portals"][0]["polygon"]["vertices"][2]["position"]["x"] = 16001.0f;
    checkFtsJsonFailure(invalid, ARX_FTS_BAD_PORTAL_POSITION, "/portals/0/polygon/vertices/2");

    invalid = source;
    invalid["rooms"][0]["portals"][0] = 1;
    checkFtsJsonFailure(invalid, ARX_FTS_BAD_ROOM_PORTAL_INDEX, "/rooms/0/portals/0");

    invalid = source;
    invalid["rooms"][0]["polygons"][0]["polygonIdx"] = 1;
    checkFtsJsonFailure(invalid, ARX_JSON_BAD_SCHEMA, "/rooms/0/polygons/0");
  }

  TEST_CASE("DLF fog survives JSON roundtrip") {
    const pistoris::Dlf source = makeDlfWithFog();
    REQUIRE(pistoris::validate(source));

    auto encoded = pistoris::toDlfJson(source);
    REQUIRE(encoded);

    auto roundtrip = pistoris::fromDlfJson(*encoded);
    REQUIRE(roundtrip);
    test_support::checkEquivalent(source, *roundtrip);

    nlohmann::json malformed = nlohmann::json::parse(*encoded);
    malformed["fogs"][0]["size"] = "invalid";
    auto rejected = pistoris::fromDlfJson(malformed.dump());
    CHECK(rejected.code() == ARX_JSON_BAD_SCHEMA);
  }
}
