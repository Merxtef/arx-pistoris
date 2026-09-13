// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"

#include "helpers.h"
#include "nlohmann/json.hpp"
#include "support/native_equivalence.h"

#include <cstring>
#include <string>

namespace {

pistoris::Fts makeFtsWithPortal() {
  pistoris::Fts result = makeMinimalFtsData();
  const std::string path = pistoris::paths::levelFts(1);
  std::memcpy(result.header.path, path.c_str(), path.size() + 1U);

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

pistoris::Dlf makeDlfWithFog() {
  pistoris::Dlf result;
  result.scene_path = "graph/levels/level1/";
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
    REQUIRE(pistoris::validate(source) == ARX_OK);

    std::string encoded;
    REQUIRE(pistoris::toJson(source, encoded) == ARX_OK);

    pistoris::Fts roundtrip;
    REQUIRE(pistoris::fromJson(encoded, roundtrip) == ARX_OK);
    test_support::checkEquivalent(source, roundtrip);

    nlohmann::json malformed = nlohmann::json::parse(encoded);
    malformed["portals"][0]["room1"] = "invalid";
    pistoris::Fts unchanged = makeTriangleFtsData();
    const pistoris::Fts expected = unchanged;
    CHECK(pistoris::fromJson(malformed.dump(), unchanged) == ARX_JSON_BAD_SCHEMA);
    test_support::checkEquivalent(expected, unchanged);
  }

  TEST_CASE("DLF fog survives JSON roundtrip") {
    const pistoris::Dlf source = makeDlfWithFog();
    REQUIRE(pistoris::validate(source) == ARX_OK);

    std::string encoded;
    REQUIRE(pistoris::toJson(source, encoded) == ARX_OK);

    pistoris::Dlf roundtrip;
    REQUIRE(pistoris::fromJson(encoded, roundtrip) == ARX_OK);
    test_support::checkEquivalent(source, roundtrip);

    nlohmann::json malformed = nlohmann::json::parse(encoded);
    malformed["fogs"][0]["size"] = "invalid";
    pistoris::Dlf unchanged;
    unchanged.scene_path = "graph/levels/level2/";
    const pistoris::Dlf expected = unchanged;
    CHECK(pistoris::fromJson(malformed.dump(), unchanged) == ARX_JSON_BAD_SCHEMA);
    test_support::checkEquivalent(expected, unchanged);
  }
}
