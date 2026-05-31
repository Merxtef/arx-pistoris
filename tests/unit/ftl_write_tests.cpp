// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/ftl.h"
#include "helpers.h"
#include "utils/cursor.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static pistoris::ftl::Data parseFixture(const std::vector<uint8_t>& fixture) {
  pistoris::ftl::Data d;
  pistoris::ReadCursor c(fixture.data(), fixture.size());
  REQUIRE(pistoris::loadFtl(&d, c) == ARX_OK);
  return d;
}

TEST_SUITE("ftl") {
  TEST_CASE("WriteExactMinimal") {
    auto fixture = makeMinimalFtl();
    auto d       = parseFixture(fixture);
    pistoris::WriteCursor wc;
    CHECK(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto bytes = wc.take();
    CHECK(bytes.size() == fixture.size());
    CHECK(std::memcmp(bytes.data(), fixture.data(), fixture.size()) == 0);
  }

  TEST_CASE("WriteExactTriangle") {
    auto fixture = makeTriangleFtlWithFlags(pistoris::kFaceBitTrans);
    auto d       = parseFixture(fixture);
    pistoris::WriteCursor wc;
    CHECK(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto bytes = wc.take();
    CHECK(bytes.size() == fixture.size());
    CHECK(std::memcmp(bytes.data(), fixture.data(), fixture.size()) == 0);
  }

  // exercises group/action/selection writer paths that fixture-based tests miss
  TEST_CASE("WriteFromInMemoryDataFull") {
    auto d1 = makeData(4);

    pistoris::ftl::Face f;
    f.vertex_idx = {0, 1, 2};
    f.texture_id = 0;
    d1.faces.push_back(f);

    pistoris::ftl::TextureContainer tc{};
    std::strcpy(tc.filename, "GRAPH\\OBJ3D\\BODY.BMP");
    d1.texture_containers.push_back(tc);

    pistoris::ftl::Group g{};
    std::strcpy(g.name, "root");
    g.origin           = 0;
    g.indices          = {0, 1, 2, 3};
    g.blob_shadow_size = 1.5f;
    d1.groups.push_back(g);

    pistoris::ftl::Action a{};
    std::strcpy(a.name, "action_pt");
    a.vertex_idx = 0;
    d1.actions.push_back(a);

    pistoris::ftl::Selection s;
    std::strcpy(s.name, "head");
    s.selected = {0, 1};
    d1.selections.push_back(s);

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d1, wc) == ARX_OK);
    auto bytes = wc.take();

    pistoris::ftl::Data d2;
    pistoris::ReadCursor rc(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadFtl(&d2, rc) == ARX_OK);

    checkEq(d1, d2);
  }

  TEST_CASE("WriteRoundtrip") {
    auto fixture = makeTriangleFtlWithFlags(pistoris::kFaceBitTrans);
    auto d1      = parseFixture(fixture);

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d1, wc) == ARX_OK);
    auto bytes = wc.take();

    pistoris::ftl::Data d2;
    {
      pistoris::ReadCursor c(bytes.data(), bytes.size());
      REQUIRE(pistoris::loadFtl(&d2, c) == ARX_OK);
    }

    CHECK(d1.vertices.size() == d2.vertices.size());
    CHECK(d1.faces.size() == d2.faces.size());
    CHECK(d1.faces[0].type == d2.faces[0].type);
    CHECK(d1.faces[0].texture_id == d2.faces[0].texture_id);
    CHECK(d1.texture_containers.size() == d2.texture_containers.size());
    CHECK(std::string(d1.texture_containers[0].filename) == std::string(d2.texture_containers[0].filename));
    CHECK(d1.header.origin == d2.header.origin);
  }

}  // TEST_SUITE("ftl")
