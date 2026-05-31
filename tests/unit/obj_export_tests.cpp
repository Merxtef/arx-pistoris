// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "external/obj.h"
#include "helpers.h"

#include <cstring>
#include <string>

// count lines beginning with prefix
static std::size_t countPrefixLines(const std::string& s, const char* prefix) {
  std::size_t count = 0;
  std::size_t plen  = std::strlen(prefix);
  std::size_t pos   = 0;
  while (pos < s.size()) {
    std::size_t nl  = s.find('\n', pos);
    std::size_t end = (nl == std::string::npos) ? s.size() : nl;
    if (end - pos >= plen && s.compare(pos, plen, prefix) == 0) ++count;
    pos = (nl == std::string::npos) ? s.size() : nl + 1;
  }
  return count;
}

TEST_SUITE("obj") {
  // 1 vertex, no faces -> 1 v, 1 vn, 0 vt, 0 f lines
  TEST_CASE("ObjEmpty") {
    auto d = makeData(1);

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    CHECK(countPrefixLines(obj, "v ") == 1);
    CHECK(countPrefixLines(obj, "vn ") == 1);
    CHECK(countPrefixLines(obj, "vt ") == 0);
    CHECK(countPrefixLines(obj, "f ") == 0);
  }

  // 3 vertices, 1 textured face -> 3 v, 3 vn, 3 vt, 1 f lines
  TEST_CASE("ObjTriangle") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    d.faces.push_back(makeFace(0, 1, 2, 0));

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    CHECK(countPrefixLines(obj, "v ") == 3);
    CHECK(countPrefixLines(obj, "vn ") == 3);
    CHECK(countPrefixLines(obj, "vt ") == 3);
    CHECK(countPrefixLines(obj, "f ") == 1);
  }

  // FACE_BIT_TRANS face -> "usemtl STEM__TRANS" in OBJ
  TEST_CASE("ObjFlags") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.type = pistoris::kFaceBitTrans;
    d.faces.push_back(face);

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    CHECK(countPrefixLines(obj, "usemtl BODY__TRANS") == 1);
    CHECK(obj.find("usemtl BODY\n") == std::string::npos);
  }

  // No texture containers -> empty MTL string
  TEST_CASE("MtlEmpty") {
    auto d = makeData(1);

    std::string mtl;
    CHECK(pistoris::exportFtlToMtl(d, mtl) == ARX_OK);
    CHECK(mtl.empty());
  }

  // Texture with full path -> stem for newmtl, stem+ext for map_Kd, full path in arx_path comment
  TEST_CASE("MtlTexture") {
    auto d = makeData(1);
    pistoris::ftl::TextureContainer tc{};
    const char* filename = "GRAPH\\OBJ3D\\BODY.BMP";
    std::memcpy(tc.filename, filename, std::strlen(filename) + 1);
    d.texture_containers.push_back(tc);
    d.faces.push_back(makeFace(0, 0, 0, 0));

    std::string mtl;
    CHECK(pistoris::exportFtlToMtl(d, mtl) == ARX_OK);
    CHECK(countPrefixLines(mtl, "newmtl BODY") == 1);
    CHECK(countPrefixLines(mtl, "map_Kd BODY.BMP") == 1);
    CHECK(mtl.find("map_Kd GRAPH") == std::string::npos);
    CHECK(countPrefixLines(mtl, "# arx_path GRAPH") == 1);
  }

  // FACE_BIT_TRANS face -> "newmtl STEM__TRANS" in MTL
  TEST_CASE("MtlFlags") {
    auto d = makeData(1);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face = makeFace(0, 0, 0, 0);
    face.type = pistoris::kFaceBitTrans;
    d.faces.push_back(face);

    std::string mtl;
    CHECK(pistoris::exportFtlToMtl(d, mtl) == ARX_OK);
    CHECK(countPrefixLines(mtl, "newmtl BODY__TRANS") == 1);
    CHECK(countPrefixLines(mtl, "map_Kd BODY.BMP") == 1);
    CHECK(mtl.find("newmtl BODY\n") == std::string::npos);
  }

  // FACE_BIT_TRANS + known transval -> MTL has "d <val>"
  TEST_CASE("MtlTransvalExported") {
    using namespace pistoris;
    auto d = makeData(1);
    ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "TEX.BMP", 8);
    d.texture_containers.push_back(tc);
    auto face     = makeFace(0, 0, 0, 0);
    face.type     = kFaceBitTrans;
    face.transval = 0.5f;
    d.faces.push_back(face);

    std::string mtl;
    CHECK(exportFtlToMtl(d, mtl) == ARX_OK);
    CHECK(mtl.find("\nd 0.5\n") != std::string::npos);
  }

  // transval without FACE_BIT_TRANS -> no "d" line emitted
  TEST_CASE("MtlNoTransvalWithoutTransFlag") {
    using namespace pistoris;
    auto d = makeData(1);
    ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "TEX.BMP", 8);
    d.texture_containers.push_back(tc);
    auto face     = makeFace(0, 0, 0, 0);
    face.transval = 0.5f;
    d.faces.push_back(face);

    std::string mtl;
    CHECK(exportFtlToMtl(d, mtl) == ARX_OK);
    CHECK(mtl.find("\nd ") == std::string::npos);
  }

  // Two FACE_BIT_TRANS faces in same group with different transval -> MTL "d" is average
  TEST_CASE("MtlTransvalAverage") {
    using namespace pistoris;
    auto d = makeData(1);
    ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "TEX.BMP", 8);
    d.texture_containers.push_back(tc);
    for (float tv : {0.25f, 0.75f}) {
      auto face     = makeFace(0, 0, 0, 0);
      face.type     = kFaceBitTrans;
      face.transval = tv;
      d.faces.push_back(face);
    }

    std::string mtl;
    CHECK(exportFtlToMtl(d, mtl) == ARX_OK);
    CHECK(mtl.find("\nd 0.5\n") != std::string::npos);
  }

  // --- New format tests ---

  // Specific UV floats appear verbatim in "vt" lines
  TEST_CASE("ObjUVValues") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.u    = {0.25f, 0.5f, 0.75f};
    face.v    = {0.125f, 0.25f, 0.375f};
    face.norm = {0, 0, 1};
    d.faces.push_back(face);

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    // vt u v per corner: (u.x, v.x), (u.y, v.y), (u.z, v.z)
    CHECK(obj.find("vt 0.25 0.125") != std::string::npos);
    CHECK(obj.find("vt 0.5 0.25") != std::string::npos);
    CHECK(obj.find("vt 0.75 0.375") != std::string::npos);
  }

  // Specific normal floats appear verbatim in "vn" lines
  TEST_CASE("ObjNormalValues") {
    pistoris::ftl::Data d;
    d.header.origin = 0;
    d.vertices.push_back({{0, 0, 0}, {0.5f, 0.25f, 0.125f}});

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    CHECK(obj.find("vn 0.5 0.25 0.125") != std::string::npos);
  }

  // "# origin N" is written with the correct 1-based index
  TEST_CASE("ObjOriginComment") {
    auto d          = makeData(3);
    d.header.origin = 2;  // 0-based; "# origin 3" in OBJ (1-based)

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    CHECK(obj.find("# origin 3\n") != std::string::npos);
  }

  // "mtllib <stem>.mtl" is present when there are texture containers
  TEST_CASE("ObjMtllibLine") {
    auto d = makeData(1);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "mystem", obj) == ARX_OK);
    CHECK(obj.find("mtllib mystem.mtl\n") != std::string::npos);
  }

  // face with kFtlTextureNone -> "usemtl no_tex" written instead of a texture name
  TEST_CASE("ObjNoTexFace") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));  // kFtlTextureNone by default

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    CHECK(obj.find("usemtl no_tex") != std::string::npos);
  }

  // Two textures, two faces with different ids -> two usemtl lines in the correct order
  TEST_CASE("ObjMultiMaterial") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc0{}, tc1{};
    std::memcpy(tc0.filename, "TEX_A.BMP", 10);
    std::memcpy(tc1.filename, "TEX_B.BMP", 10);
    d.texture_containers.push_back(tc0);
    d.texture_containers.push_back(tc1);
    d.faces.push_back(makeFace(0, 1, 2, 0));
    d.faces.push_back(makeFace(0, 1, 2, 1));

    std::string obj;
    CHECK(pistoris::exportFtlToObj(d, "test", obj) == ARX_OK);
    auto pos0 = obj.find("usemtl TEX_A");
    auto pos1 = obj.find("usemtl TEX_B");
    CHECK(pos0 != std::string::npos);
    CHECK(pos1 != std::string::npos);
    CHECK(pos0 < pos1);
  }

}  // TEST_SUITE("obj")
