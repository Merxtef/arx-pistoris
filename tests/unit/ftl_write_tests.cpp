// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/ftl.hpp"

#include "helpers.h"
#include "native/ftl.h"
#include "support/native_equivalence.h"
#include "utils/cursor.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
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
    auto d = parseFixture(fixture);
    pistoris::WriteCursor wc;
    CHECK(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto bytes = wc.take();
    CHECK(bytes.size() == fixture.size());
    CHECK(std::memcmp(bytes.data(), fixture.data(), fixture.size()) == 0);
  }

  TEST_CASE("WriteExactTriangle") {
    auto fixture = makeTriangleFtlWithFlags(pistoris::kFaceBitTrans);
    auto d = parseFixture(fixture);
    pistoris::WriteCursor wc;
    CHECK(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto bytes = wc.take();
    const std::size_t texture = kFtlDataOff + 3 * kFtlVertexSize + kFtlFaceSize;
    std::memset(fixture.data() + texture, 0, kFtlTextureSize);
    constexpr std::string_view kCanonicalTexture = "graph/obj3d/body.";
    std::memcpy(fixture.data() + texture, kCanonicalTexture.data(), kCanonicalTexture.size());
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
    std::strcpy(tc.filename, "graph/obj3d/body");
    d1.texture_containers.push_back(tc);

    pistoris::ftl::Group g{};
    std::strcpy(g.name, "root");
    g.origin = 0;
    g.indices = {0, 1, 2, 3};
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

    test_support::checkEquivalent(d1, d2);
  }

  TEST_CASE("Preserves nonfinite native normals exactly") {
    pistoris::ftl::Data source = makeData(3);
    const float nan = std::bit_cast<float>(UINT32_C(0xffc00000));
    source.vertices[0].normal = {nan, nan, nan};
    pistoris::ftl::Face face = makeFace(0, 1, 2);
    face.norm = {nan, nan, nan};
    source.faces.push_back(face);

    pistoris::WriteCursor writer;
    REQUIRE(pistoris::saveFtl(&source, writer) == ARX_OK);
    const std::vector<std::uint8_t> bytes = writer.take();

    pistoris::ftl::Data roundtrip;
    pistoris::ReadCursor reader(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadFtl(&roundtrip, reader) == ARX_OK);
    test_support::checkEquivalent(source, roundtrip);
  }

  TEST_CASE("WriteRoundtrip") {
    auto fixture = makeTriangleFtlWithFlags(pistoris::kFaceBitTrans);
    auto d1 = parseFixture(fixture);

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

  TEST_CASE("FtlValidationRequiresBoundedNativeStrings") {
    pistoris::ftl::Data data = makeData();
    std::memset(data.header.name, 'a', sizeof(data.header.name));
    CHECK(pistoris::validateFtl(&data) == ARX_FTL_BAD_SOURCE_PATH);

    data = makeData();
    data.texture_containers.emplace_back();
    std::memset(data.texture_containers.front().filename, 'a', sizeof(data.texture_containers.front().filename));
    CHECK(pistoris::validateFtl(&data) == ARX_FTL_BAD_TEXTURE_PATH);

    data = makeData();
    data.groups.emplace_back();
    data.groups.front().origin = 0;
    std::memset(data.groups.front().name, 'a', sizeof(data.groups.front().name));
    CHECK(pistoris::validateFtl(&data) == ARX_FTL_BAD_GROUP_NAME);

    data = makeData();
    data.actions.emplace_back();
    data.actions.front().vertex_idx = 0;
    std::memset(data.actions.front().name, 'a', sizeof(data.actions.front().name));
    CHECK(pistoris::validateFtl(&data) == ARX_FTL_BAD_ACTION_NAME);

    data = makeData();
    data.selections.emplace_back();
    data.selections.front().selected.push_back(0);
    std::memset(data.selections.front().name, 'a', sizeof(data.selections.front().name));
    CHECK(pistoris::validateFtl(&data) == ARX_FTL_BAD_SELECTION_NAME);
  }

  TEST_CASE("FtlValidationAllowsBytesAfterNativeTerminator") {
    pistoris::ftl::Data data = makeData();
    std::memset(data.header.name, 'x', sizeof(data.header.name));
    data.header.name[0] = 'a';
    data.header.name[1] = '\0';
    CHECK(pistoris::validateFtl(&data) == ARX_OK);
  }

}  // TEST_SUITE("ftl")
