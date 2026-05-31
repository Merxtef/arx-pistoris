// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "helpers.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

TEST_SUITE("cpp_api") {
  auto make_tri_data = [] {
    pistoris::Ftl ftl        = makeData(3);
    ftl.vertices[0].position = {0.0f, 0.0f, 0.0f};
    ftl.vertices[1].position = {1.0f, 0.0f, 0.0f};
    ftl.vertices[2].position = {0.0f, 1.0f, 0.0f};
    ftl.faces.push_back(makeFace(0, 1, 2));
    return ftl;
  };

  TEST_CASE("Metadata") {
    CHECK(pistoris::version() != nullptr);
    CHECK(std::string_view(pistoris::version()).size() > 0);
    CHECK(pistoris::buildTimeString() != nullptr);
    CHECK(std::string_view(pistoris::buildTimeString()).size() > 0);
    CHECK(pistoris::errorString(ARX_OK) != nullptr);
    CHECK(std::string_view(pistoris::errorString(ARX_OK)) == "ok");
  }

  TEST_CASE("FtlReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeMinimalFtl();
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(fixture, ftl) == ARX_OK);

    std::vector<std::uint8_t> out;
    CHECK(pistoris::writeFtl(ftl, out) == ARX_OK);
    CHECK(!out.empty());
  }

  TEST_CASE("TeaReadWriteRoundtrip") {
    std::vector<std::uint8_t> fixture = makeKeyframeTea();
    pistoris::Tea tea;
    REQUIRE(pistoris::readTea(fixture, tea) == ARX_OK);

    std::vector<std::uint8_t> out;
    CHECK(pistoris::writeTea(tea, out) == ARX_OK);
    CHECK(!out.empty());
  }

  TEST_CASE("JsonRoundtrip") {
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(makeTriangleFtlWithTexture(), ftl) == ARX_OK);

    std::string json;
    REQUIRE(pistoris::exportJson(ftl, json, true) == ARX_OK);
    CHECK(json.find("\"vertices\"") != std::string::npos);

    pistoris::Ftl imported;
    CHECK(pistoris::importJson(json, imported) == ARX_OK);
    CHECK(!imported.vertices.empty());
  }

  TEST_CASE("TeaJsonRoundtrip") {
    pistoris::Tea tea;
    REQUIRE(pistoris::readTea(makeKeyframeTea(), tea) == ARX_OK);

    std::string json;
    REQUIRE(pistoris::exportJson(tea, json, true) == ARX_OK);
    CHECK(json.find("\"keyframes\"") != std::string::npos);
    CHECK(json.find("\"totalNumberOfFrames\"") != std::string::npos);

    pistoris::Tea imported;
    CHECK(pistoris::importJson(json, imported) == ARX_OK);
    CHECK(!imported.keyframes.empty());
  }

  TEST_CASE("ObjRoundtrip") {
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(makeTriangleFtlWithTexture(), ftl) == ARX_OK);

    pistoris::Obj obj;
    REQUIRE(pistoris::exportObj(ftl, "test", obj) == ARX_OK);
    CHECK(obj.text.find("v ") != std::string::npos);
    CHECK(obj.mtl.find("newmtl") != std::string::npos);

    pistoris::Ftl imported;
    CHECK(pistoris::importObj(obj, "test.obj", imported) == ARX_OK);
    CHECK(!imported.vertices.empty());
    CHECK(!imported.faces.empty());
  }

  TEST_CASE("GlbRoundtrip") {
    pistoris::Ftl ftl = make_tri_data();

    std::vector<std::uint8_t> glb;
    REQUIRE(pistoris::exportGlb(ftl, {}, glb) == ARX_OK);
    CHECK(glb.size() > 12);

    pistoris::Ftl imported;
    std::vector<pistoris::Tea> teas;
    CHECK(pistoris::importGlb(glb, "test.glb", imported, teas) == ARX_OK);
    CHECK(!imported.vertices.empty());
    CHECK(teas.empty());
  }

  TEST_CASE("Utilities") {
    pistoris::Ftl ftl;
    REQUIRE(pistoris::readFtl(makeMinimalFtl(), ftl) == ARX_OK);

    CHECK(pistoris::overwriteTexturePaths(ftl, "GRAPH\\NEW.BMP") == ARX_OK);

    pistoris::AffineXform identity = pistoris::makeAffineXform(0, 0, 0, 1, 1, 1, 0, 0, 0);
    CHECK(pistoris::applyTransform(ftl, identity) == ARX_OK);
    CHECK(pistoris::validate(ftl) == ARX_OK);
  }

  TEST_CASE("ReferenceRepairUtilities") {
    auto set_name = [](auto& dst, const char* name) { std::memcpy(dst.name, name, std::strlen(name) + 1); };

    pistoris::Ftl target     = makeData(4);
    pistoris::Ftl ref        = makeData(4);
    ref.vertices[1].position = {10.0f, 0.0f, 0.0f};
    ref.vertices[2].position = {20.0f, 0.0f, 0.0f};
    ref.vertices[3].position = {30.0f, 0.0f, 0.0f};

    pistoris::ftl::Group root{};
    set_name(root, "root");
    root.origin = 1;
    pistoris::ftl::Group child{};
    set_name(child, "child");
    child.origin  = 2;
    target.groups = {root, child};
    ref.groups    = {root, child};

    pistoris::ftl::Action action{};
    set_name(action, "head2chest");
    action.vertex_idx = 3;
    target.actions.push_back(action);
    ref.actions.push_back(action);

    pistoris::ftl::Selection ref_sel{};
    set_name(ref_sel, "chest");
    ref_sel.selected = {2, 3};
    ref.selections.push_back(ref_sel);

    CHECK(pistoris::snapFtlBoneOriginsToReference(target, ref) == ARX_OK);
    CHECK(target.vertices[1].position.x == 10.0f);
    CHECK(target.vertices[2].position.x == 20.0f);

    CHECK(pistoris::snapFtlActionPointsToReference(target, ref) == ARX_OK);
    CHECK(target.vertices[3].position.x == 30.0f);

    CHECK(pistoris::copyFtlSyntheticSelectionAffiliations(target, ref) == ARX_OK);
    REQUIRE(target.selections.size() == 1);
    CHECK(std::string_view(target.selections[0].name) == "chest");
    CHECK(target.selections[0].selected == std::vector<std::int32_t>{2, 3});
  }

  TEST_CASE("FailedImportsLeaveOutputsUnchanged") {
    pistoris::Ftl ftl             = makeData(2);
    std::size_t original_vertices = ftl.vertices.size();

    std::uint8_t bad_ftl[4] = {};
    CHECK(pistoris::readFtl(bad_ftl, ftl) != ARX_OK);
    CHECK(ftl.vertices.size() == original_vertices);

    CHECK(pistoris::importJson("{", ftl) != ARX_OK);
    CHECK(ftl.vertices.size() == original_vertices);
  }
}  // TEST_SUITE("cpp_api")
