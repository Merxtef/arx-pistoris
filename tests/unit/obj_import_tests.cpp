// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/flags.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "external/obj.h"
#include "helpers.h"

#include <cstring>
#include <ostream>  // IWYU pragma: keep (needed by doctest to stringify string_view)
#include <string>
#include <string_view>

TEST_SUITE("obj") {
  // --- Geometry roundtrip ---

  TEST_CASE("ImportRoundtripCounts") {
    auto src = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    const char* fn = "GRAPH\\OBJ3D\\BODY.BMP";
    std::memcpy(tc.filename, fn, std::strlen(fn) + 1);
    src.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.u = {0.0f, 1.0f, 0.0f};
    face.v = {0.0f, 0.0f, 1.0f};
    face.norm = {0, 0, 1};
    src.faces.push_back(face);

    std::string obj, mtl;
    CHECK(pistoris::exportFtlToObj(src, "test", obj) == ARX_OK);
    CHECK(pistoris::exportFtlToMtl(src, mtl) == ARX_OK);

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "test.obj", &d) == ARX_OK);
    CHECK(d.vertices.size() == 3);
    CHECK(d.faces.size() == 1);
    CHECK(d.texture_containers.size() == 1);
    CHECK(d.faces[0].texture_id == 0);
  }

  TEST_CASE("ImportArxPathPreserved") {
    auto src = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    const char* fn = "GRAPH\\OBJ3D\\BODY.BMP";
    std::memcpy(tc.filename, fn, std::strlen(fn) + 1);
    src.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.u = {0.0f, 1.0f, 0.0f};
    face.v = {0.0f, 0.0f, 1.0f};
    face.norm = {0, 0, 1};
    src.faces.push_back(face);

    std::string obj, mtl;
    CHECK(pistoris::exportFtlToObj(src, "test", obj) == ARX_OK);
    CHECK(pistoris::exportFtlToMtl(src, mtl) == ARX_OK);

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "test.obj", &d) == ARX_OK);
    CHECK(d.texture_containers.size() == 1);
    CHECK(std::string_view(d.texture_containers[0].filename) == "GRAPH\\OBJ3D\\BODY.BMP");
  }

  TEST_CASE("ImportFlagsRoundtrip") {
    auto src = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    src.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.type = pistoris::kFaceBitTrans;
    face.u = {0.0f, 1.0f, 0.0f};
    face.v = {0.0f, 0.0f, 1.0f};
    face.norm = {0, 0, 1};
    src.faces.push_back(face);

    std::string obj, mtl;
    CHECK(pistoris::exportFtlToObj(src, "test", obj) == ARX_OK);
    CHECK(pistoris::exportFtlToMtl(src, mtl) == ARX_OK);

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "test.obj", &d) == ARX_OK);
    CHECK(d.faces.size() == 1);
    CHECK(d.faces[0].type == pistoris::kFaceBitTrans);
  }

  // --- header fields ---

  TEST_CASE("ImportHeaderName") {
    const char* obj =
        "v 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    pistoris::ftl::Data d;
    pistoris::importObjToFtl(obj, {}, "path/to/eyeball.obj", &d);
    CHECK(std::string_view(d.header.name) == "arx_pistoris\\eyeball.obj");
  }

  TEST_CASE("ImportHeaderNameEmpty") {
    const char* obj =
        "v 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    pistoris::ftl::Data d;
    pistoris::importObjToFtl(obj, {}, "", &d);
    CHECK(d.header.name[0] == '\0');
  }

  TEST_CASE("ImportOriginResolvesUnreferencedPosition") {
    const char* obj =
        "v 1 0 0\nv 0 1 0\nv 0 0 1\n"  // positions 1,2,3 (1-based); origin = position 2
        "vn 1 0 0\nvn 0 1 0\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "# origin 2\n"
        "f 1/1/1 3/3/3 1/2/2\n";  // only positions 1 and 3 referenced, not 2
    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OK);
    // position[1] (0-based) = (0,1,0) -- unreferenced, appended last
    auto ftl_origin = d.header.origin;
    CHECK(ftl_origin < d.vertices.size());
    CHECK(d.vertices[ftl_origin].position.x == 0.0f);
    CHECK(d.vertices[ftl_origin].position.y == 1.0f);
    CHECK(d.vertices[ftl_origin].position.z == 0.0f);
  }

  TEST_CASE("ImportRejectsNoGeometry") {
    pistoris::ftl::Data d;
    ArxReturnCode rc = pistoris::importObjToFtl("# no origin here\n", {}, "", &d);
    CHECK(rc == ARX_OBJ_NO_GEOMETRY);
  }

  TEST_CASE("ImportOriginDefaultWithGeometry") {
    const char* obj =
        "v 1 0 0\nv 0 1 0\nv 0 0 1\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "f 1/1/1 2/2/2 3/3/3\n";  // no # origin
    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OK);
    CHECK(d.vertices.size() == 4);  // 3 geometry + 1 appended origin
    auto orig = d.header.origin;
    CHECK(orig == 3);
    CHECK(d.vertices[orig].position.x == 0.0f);
    CHECK(d.vertices[orig].position.y == 0.0f);
    CHECK(d.vertices[orig].position.z == 0.0f);
  }

  TEST_CASE("ImportOriginRoundtrip") {
    pistoris::ftl::Data src;
    src.header.origin = 0;
    src.vertices.push_back({{0, 0, 0}, {0, 0, 1}});
    src.vertices.push_back({{1, 0, 0}, {0, 0, 1}});
    src.vertices.push_back({{0, 1, 0}, {0, 0, 1}});
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    src.texture_containers.push_back(tc);
    pistoris::ftl::Face face{};
    face.vertex_idx = {0, 1, 2};
    face.texture_id = 0;
    face.u = {0.0f, 1.0f, 0.0f};
    face.v = {0.0f, 0.0f, 1.0f};
    face.norm = {0, 0, 1};
    src.faces.push_back(face);

    std::string obj, mtl;
    CHECK(pistoris::exportFtlToObj(src, "test", obj) == ARX_OK);
    CHECK(pistoris::exportFtlToMtl(src, mtl) == ARX_OK);

    CHECK(obj.find("# origin 1") != std::string::npos);  // origin 0 (0-based) -> 1 (1-based)

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "", &d) == ARX_OK);
    CHECK(d.header.origin == 0);
  }

  // --- Untextured faces ---

  TEST_CASE("ImportNoTex") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl no_tex\n"
        "f 1/1/1 2/2/2 3/3/3\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "test.obj", &d) == ARX_OK);
    CHECK(d.faces.size() == 1);
    CHECK(d.faces[0].texture_id == pistoris::kFtlTextureNone);
    CHECK(d.texture_containers.empty());
  }

  TEST_CASE("ImportQuadTriangulation") {
    const char* obj =
        "# origin 1\n"
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "f 1/1/1 2/2/2 3/3/3 4/4/4\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, "", "", &d) == ARX_OK);
    CHECK(d.faces.size() == 2);
    CHECK(d.vertices.size() == 4);
  }

  // --- MTL fallback tests ---

  TEST_CASE("ImportNoArxPathFallback") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl BODY\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    const char* mtl =
        "newmtl BODY\n"
        "map_Kd BODY.BMP\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "", &d) == ARX_OK);
    CHECK(d.texture_containers.size() == 1);
    CHECK(std::string_view(d.texture_containers[0].filename) == "BODY.BMP");
  }

  TEST_CASE("ImportMapKdWithSpaces") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl BODY\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    const char* mtl =
        "newmtl BODY\n"
        "map_Kd folder\\my texture.bmp\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "", &d) == ARX_OK);
    CHECK(d.texture_containers.size() == 1);
    CHECK(std::string_view(d.texture_containers[0].filename) == "folder\\my texture.bmp");
  }

  TEST_CASE("ImportArxPathSpaceInDirectory") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl BODY\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    const char* mtl =
        "newmtl BODY\n"
        "# arx_path folder\\my texture.bmp\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "", &d) == ARX_OK);
    CHECK(d.texture_containers.size() == 1);
    CHECK(std::string_view(d.texture_containers[0].filename) == "folder\\my texture.bmp");
  }

  TEST_CASE("ImportNoMtlFallback") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl BODY\n"
        "f 1/1/1 2/2/2 3/3/3\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OK);
    CHECK(d.texture_containers.size() == 1);
    CHECK(std::string_view(d.texture_containers[0].filename) == "BODY");
  }

  // --- Transval tests ---

  TEST_CASE("ImportTransvalFromMtl") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "usemtl TEX__TRANS\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    const char* mtl =
        "newmtl TEX__TRANS\n"
        "map_Kd TEX.BMP\n"
        "d 0.5\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, mtl, "", &d) == ARX_OK);
    CHECK(d.faces.size() == 1);
    CHECK(d.faces[0].transval == 0.5f);
  }

  TEST_CASE("ImportTransvalRoundtrip") {
    using namespace pistoris;
    auto d1 = makeData(3);
    ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "TEX.BMP", 8);
    d1.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.type = kFaceBitTrans;
    face.transval = 0.5f;
    face.norm = {0, 0, 1};
    d1.faces.push_back(face);

    std::string obj, mtl;
    CHECK(exportFtlToObj(d1, "test", obj) == ARX_OK);
    CHECK(exportFtlToMtl(d1, mtl) == ARX_OK);

    ftl::Data d2;
    CHECK(importObjToFtl(obj, mtl, "", &d2) == ARX_OK);
    CHECK(d2.faces.size() == 1);
    CHECK(d2.faces[0].transval == 0.5f);
  }

  TEST_CASE("ImportTransvalAverage") {
    using namespace pistoris;
    auto d1 = makeData(3);
    ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "TEX.BMP", 8);
    d1.texture_containers.push_back(tc);
    for (float tv : {0.25f, 0.75f}) {
      auto f = makeFace(0, 1, 2, 0);
      f.type = kFaceBitTrans;
      f.transval = tv;
      f.norm = {0, 0, 1};
      d1.faces.push_back(f);
    }

    std::string obj, mtl;
    CHECK(exportFtlToObj(d1, "test", obj) == ARX_OK);
    CHECK(exportFtlToMtl(d1, mtl) == ARX_OK);
    CHECK(mtl.find("\nd 0.5\n") != std::string::npos);

    ftl::Data d2;
    CHECK(importObjToFtl(obj, mtl, "", &d2) == ARX_OK);
    CHECK(d2.faces.size() == 2);
    CHECK(d2.faces[0].transval == 0.5f);
    CHECK(d2.faces[1].transval == 0.5f);
  }

  TEST_CASE("ImportRejectsMalformedVtSecondComponent") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0.5 garbage\n"
        "vt 1 0\nvt 0 1\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OBJ_BAD_FORMAT);
  }

  TEST_CASE("VtAbsentVDefaultsToZero") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0.5\nvt 1\nvt 0\n"
        "f 1/1/1 2/2/2 3/3/3\n";
    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OK);
    CHECK(d.faces[0].v.x == 0.0f);
    CHECK(d.faces[0].v.y == 0.0f);
    CHECK(d.faces[0].v.z == 0.0f);
  }

  TEST_CASE("ImportBadVertexIndexx") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "f 1/1/1 2/2/2 999/3/3\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OBJ_BAD_VERTEX_IDX);
  }

  TEST_CASE("ImportBadNormalIdx") {
    const char* obj =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vn 0 0 1\nvn 0 0 1\nvn 0 0 1\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "f 1/1/1 2/2/2 3/3/999\n";

    pistoris::ftl::Data d;
    CHECK(pistoris::importObjToFtl(obj, {}, "", &d) == ARX_OBJ_BAD_VERTEX_IDX);
  }

}  // TEST_SUITE("obj")
