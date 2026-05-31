// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "external/json.h"
#include "helpers.h"

#include <cstring>
#include <string>
#include <utility>

TEST_SUITE("json") {
  // export produces arx-convert-compatible key names
  TEST_CASE("JsonFieldNames") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "X.BMP", 6);
    d.texture_containers.push_back(tc);
    d.faces.push_back(makeFace(0, 1, 2, 0));
    pistoris::ftl::Group g;
    std::memcpy(g.name, "g", 2);
    g.origin = 0;
    d.groups.push_back(std::move(g));

    std::string out;
    CHECK(pistoris::exportFtlToJson(d, false, out) == ARX_OK);
    CHECK(out.find("\"vector\"") != std::string::npos);    // not "position"
    CHECK(out.find("\"norm\"") != std::string::npos);      // not "normal"
    CHECK(out.find("\"faceType\"") != std::string::npos);  // not "type"
    CHECK(out.find("\"textureContainers\"") != std::string::npos);
    CHECK(out.find("\"blobShadowSize\"") != std::string::npos);
    CHECK(out.find("\"vertexIdx\"") != std::string::npos);
    CHECK(out.find("\"textureIdx\"") != std::string::npos);
  }

  TEST_CASE("JsonPretty") {
    auto d = makeData(1);
    std::string compact, pretty;
    CHECK(pistoris::exportFtlToJson(d, false, compact) == ARX_OK);
    CHECK(pistoris::exportFtlToJson(d, true, pretty) == ARX_OK);
    CHECK(compact.find('\n') == std::string::npos);
    CHECK(pretty.find('\n') != std::string::npos);
    CHECK(pretty.size() > compact.size());
  }

  TEST_CASE("JsonRoundtripMinimal") {
    auto d = makeData(1);

    std::string s;
    CHECK(pistoris::exportFtlToJson(d, false, s) == ARX_OK);

    pistoris::ftl::Data d2;
    CHECK(pistoris::importJsonToFtl(s, &d2) == ARX_OK);
    checkEq(d, d2);
  }

  // all section types populated
  TEST_CASE("JsonRoundtripFull") {
    auto d = makeData(3);

    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    d.faces.push_back(makeFace(0, 1, 2, 0));

    pistoris::ftl::Group g;
    std::memcpy(g.name, "head", 5);
    g.origin           = 0;
    g.indices          = {0, 1};
    g.blob_shadow_size = 1.5f;
    d.groups.push_back(std::move(g));

    pistoris::ftl::Action a{};
    std::memcpy(a.name, "hit", 4);
    a.vertex_idx = 2;
    a.action     = 1;
    a.sfx        = 3;
    d.actions.push_back(a);

    pistoris::ftl::Selection sel;
    std::memcpy(sel.name, "left", 5);
    sel.selected = {0, 2};
    d.selections.push_back(std::move(sel));

    std::string s;
    CHECK(pistoris::exportFtlToJson(d, false, s) == ARX_OK);

    pistoris::ftl::Data d2;
    CHECK(pistoris::importJsonToFtl(s, &d2) == ARX_OK);
    checkEq(d, d2);
  }

  // u/v/vertexIdx must be JSON arrays, not objects
  TEST_CASE("JsonFaceArrayFields") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "X.BMP", 6);
    d.texture_containers.push_back(tc);
    d.faces.push_back(makeFace(0, 1, 2, 0));

    std::string out;
    CHECK(pistoris::exportFtlToJson(d, false, out) == ARX_OK);
    // arrays produce "[" not "{"
    CHECK(out.find("\"vertexIdx\":[") != std::string::npos);
    CHECK(out.find("\"u\":[") != std::string::npos);
    CHECK(out.find("\"v\":[") != std::string::npos);
  }

  // faceType stored as raw uint32 bitmask
  TEST_CASE("JsonFaceTypeBitmask") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "X.BMP", 6);
    d.texture_containers.push_back(tc);
    auto f = makeFace(0, 1, 2, 0);
    f.type = pistoris::kFaceBitDoublesided | pistoris::kFaceBitTrans;
    d.faces.push_back(f);

    std::string s;
    CHECK(pistoris::exportFtlToJson(d, false, s) == ARX_OK);

    pistoris::ftl::Data d2;
    CHECK(pistoris::importJsonToFtl(s, &d2) == ARX_OK);
    CHECK(d2.faces[0].type == (pistoris::kFaceBitDoublesided | pistoris::kFaceBitTrans));
  }

  TEST_CASE("JsonImportBadFormat") {
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl("not json {{{", &d) == ARX_JSON_BAD_FORMAT);
  }

  // empty object missing all required keys
  TEST_CASE("JsonImportMissingKeys") {
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl("{}", &d) == ARX_JSON_BAD_SCHEMA);
  }

  // valid JSON but fails validateFtl (0 vertices)
  TEST_CASE("JsonImportBadValidation") {
    const char* text =
        R"({"header":{"origin":0,"name":""},"vertices":[],"faces":[],"textureContainers":[],"groups":[],"actions":[],"selections":[]})";
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl(text, &d) == ARX_FTL_BAD_VERT_N);
  }

  TEST_CASE("JsonImportRejectsBadHeaderSchema") {
    const char* text =
        R"({"header":{"origin":-1,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[],"actions":[],"selections":[]})";
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
  }

  TEST_CASE("JsonImportRejectsBadVertexSchema") {
    const char* text =
        R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[],"actions":[],"selections":[]})";
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
  }

  TEST_CASE("JsonImportRejectsBadFaceSchema") {
    const char* text =
        R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[{"faceType":0,"vertexIdx":[0,0],"u":[0,0,0],"v":[0,0,0],"textureIdx":0}],"textureContainers":[],"groups":[],"actions":[],"selections":[]})";
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
  }

  TEST_CASE("JsonImportRejectsBadCollectionMemberSchemas") {
    SUBCASE("texture filename") {
      const char* text =
          R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[{"filename":7}],"groups":[],"actions":[],"selections":[]})";
      pistoris::ftl::Data d;
      CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("group indices") {
      const char* text =
          R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[{"name":"","origin":0,"indices":0,"blobShadowSize":0}],"actions":[],"selections":[]})";
      pistoris::ftl::Data d;
      CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("action sfx") {
      const char* text =
          R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[],"actions":[{"name":"","vertexIdx":0,"action":0,"sfx":"bad"}],"selections":[]})";
      pistoris::ftl::Data d;
      CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("selection selected") {
      const char* text =
          R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[],"actions":[],"selections":[{"name":"","selected":0}]})";
      pistoris::ftl::Data d;
      CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_BAD_SCHEMA);
    }
  }

  TEST_CASE("JsonImportRejectsNestedFtlLimitsBeforeValidation") {
    const char* text =
        R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[{"name":"","origin":0,"indices":[0,0],"blobShadowSize":0}],"actions":[],"selections":[]})";
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_LIMIT_EXCEEDED);
  }

  TEST_CASE("JsonImportTooManyGroups") {
    std::string text =
        R"({"header":{"origin":0,"name":""},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[)";
    for (std::size_t i = 0; i <= pistoris::kFtlMaxGroups; ++i) {
      if (i != 0) text += ',';
      text += R"({"name":"","origin":0,"indices":[],"blobShadowSize":0})";
    }
    text += R"(],"actions":[],"selections":[]})";
    pistoris::ftl::Data d;
    CHECK(pistoris::importJsonToFtl(text, &d) == ARX_JSON_LIMIT_EXCEEDED);
  }

  TEST_CASE("JsonImportFailureDoesNotMutateFtl") {
    pistoris::ftl::Data d = makeData(3);
    CHECK(pistoris::importJsonToFtl("{}", &d) == ARX_JSON_BAD_SCHEMA);
    CHECK(d.vertices.size() == 3);
  }

  TEST_CASE("TeaJsonFieldNames") {
    pistoris::tea::Data tea;
    tea.num_frames = 24;
    tea.num_groups = 1;
    std::memcpy(tea.name, "walk", 5);

    pistoris::tea::Keyframe kf;
    kf.num_frame  = 24;
    kf.flag_frame = pistoris::kTeaFlagFrameStep;
    kf.translate  = pistoris::ArxVector3{1.0f, 2.0f, 3.0f};
    kf.quat       = pistoris::ArxQuat{0.5f, 0.5f, 0.5f, 0.5f};

    pistoris::tea::GroupAnim group;
    group.key_group = 1;
    group.quat      = pistoris::ArxQuat{0.5f, 0.0f, 0.5f, 0.0f};
    group.translate = {4.0f, 5.0f, 6.0f};
    group.zoom      = {1.0f, 1.0f, 1.0f};
    kf.groups.push_back(group);
    tea.keyframes.push_back(kf);

    std::string out;
    CHECK(pistoris::exportTeaToJson(tea, false, out) == ARX_OK);
    CHECK(out.find("\"$schema\"") != std::string::npos);
    CHECK(out.find("\"totalNumberOfFrames\"") != std::string::npos);
    CHECK(out.find("\"keyframes\"") != std::string::npos);
    CHECK(out.find("\"frame\"") != std::string::npos);
    CHECK(out.find("\"flags\"") != std::string::npos);
    CHECK(out.find("\"isMasterKeyFrame\"") != std::string::npos);
    CHECK(out.find("\"isKeyFrame\"") != std::string::npos);
    CHECK(out.find("\"timeFrame\"") != std::string::npos);
    CHECK(out.find("\"isKey\"") != std::string::npos);
    CHECK(out.find("\"quaternion\"") != std::string::npos);
  }

  TEST_CASE("TeaJsonOmitsDefaults") {
    pistoris::tea::Data tea;
    tea.num_frames = 1;
    tea.num_groups = 1;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 1;
    kf.groups.push_back({});
    tea.keyframes.push_back(kf);

    std::string out;
    CHECK(pistoris::exportTeaToJson(tea, false, out) == ARX_OK);
    CHECK(out.find("\"translate\"") == std::string::npos);
    CHECK(out.find("\"quaternion\"") == std::string::npos);
    CHECK(out.find("\"zoom\"") == std::string::npos);
  }

  TEST_CASE("TeaJsonRoundtrip") {
    pistoris::tea::Data tea;
    tea.num_frames = 24;
    tea.num_groups = 1;
    std::memcpy(tea.name, "step", 5);

    pistoris::tea::Keyframe kf;
    kf.num_frame  = 24;
    kf.flag_frame = pistoris::kTeaFlagFrameStep;
    kf.translate  = pistoris::ArxVector3{1.0f, 2.0f, 3.0f};
    kf.quat       = pistoris::ArxQuat{0.5f, 0.5f, 0.5f, 0.5f};
    pistoris::tea::GroupAnim group;
    group.key_group = 1;
    group.quat      = pistoris::ArxQuat{0.5f, 0.0f, 0.5f, 0.0f};
    group.translate = {4.0f, 5.0f, 6.0f};
    group.zoom      = {1.0f, 1.0f, 1.0f};
    kf.groups.push_back(group);
    auto& sample = kf.sample.emplace();
    std::memcpy(sample.name, "foot.wav", 9);
    tea.keyframes.push_back(kf);

    std::string out;
    CHECK(pistoris::exportTeaToJson(tea, false, out) == ARX_OK);

    pistoris::tea::Data imported;
    CHECK(pistoris::importJsonToTea(out, &imported) == ARX_OK);
    checkEq(tea, imported);
  }

  TEST_CASE("TeaJsonImportArxConvertShape") {
    const char* text =
        R"({"$schema":"https://arx-tools.github.io/schemas/tea.schema.json","header":{"name":"walk","totalNumberOfFrames":24},"keyframes":[{"frame":24,"flags":9,"isMasterKeyFrame":false,"isKeyFrame":false,"timeFrame":0,"groups":[{"isKey":true,"translate":{"x":1,"y":2,"z":3}}],"sample":{"name":"foot.wav","sizeInBytes":123}}]})";
    pistoris::tea::Data tea;
    CHECK(pistoris::importJsonToTea(text, &tea) == ARX_OK);
    CHECK(std::string(tea.name) == "walk");
    CHECK(tea.num_frames == 24);
    REQUIRE(tea.keyframes.size() == 1);
    CHECK(tea.keyframes[0].num_frame == 24);
    CHECK(tea.keyframes[0].flag_frame == pistoris::kTeaFlagFrameStep);
    REQUIRE(tea.keyframes[0].groups.size() == 1);
    CHECK(tea.keyframes[0].groups[0].key_group == 1);
    checkEq(tea.keyframes[0].groups[0].translate, pistoris::ArxVector3{1.0f, 2.0f, 3.0f});
    REQUIRE(tea.keyframes[0].sample.has_value());
    CHECK(std::string(tea.keyframes[0].sample->name) == "foot.wav");
  }

  TEST_CASE("TeaJsonImportBadFormat") {
    pistoris::tea::Data tea;
    CHECK(pistoris::importJsonToTea("not json {{{", &tea) == ARX_JSON_BAD_FORMAT);
  }

  TEST_CASE("TeaJsonImportBadValidation") {
    const char* text = R"({"header":{"name":"","totalNumberOfFrames":0},"keyframes":[]})";
    pistoris::tea::Data tea;
    CHECK(pistoris::importJsonToTea(text, &tea) == ARX_TEA_BAD_KEYFRAMES_N);
  }

  TEST_CASE("TeaJsonImportRejectsBadHeaderSchema") {
    const char* text = R"({"header":{"name":"","totalNumberOfFrames":2147483648},"keyframes":[{"frame":0,"groups":[]}]})";
    pistoris::tea::Data tea;
    CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
  }

  TEST_CASE("TeaJsonImportRejectsBadKeyframeSchemas") {
    SUBCASE("frame") {
      const char* text = R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":2147483648,"groups":[]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("flags") {
      const char* text = R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"flags":4294967296,"groups":[]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("master key flag") {
      const char* text =
          R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"isMasterKeyFrame":0,"groups":[]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("time frame") {
      const char* text = R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"timeFrame":"0","groups":[]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("translate") {
      const char* text = R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"translate":{"x":0},"groups":[]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("quaternion") {
      const char* text = R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"quaternion":{"w":1},"groups":[]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }
  }

  TEST_CASE("TeaJsonImportRejectsBadGroupAndSampleSchemas") {
    SUBCASE("group key flag") {
      const char* text =
          R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"groups":[{"isKey":0}]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("group quaternion") {
      const char* text =
          R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"groups":[{"isKey":false,"quaternion":{"w":1}}]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("group translate") {
      const char* text =
          R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"groups":[{"isKey":false,"translate":{"x":0}}]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("group zoom") {
      const char* text =
          R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"groups":[{"isKey":false,"zoom":{"x":1}}]}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }

    SUBCASE("sample name") {
      const char* text =
          R"({"header":{"name":"","totalNumberOfFrames":1},"keyframes":[{"frame":0,"groups":[],"sample":{"name":7}}]})";
      pistoris::tea::Data tea;
      CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_BAD_SCHEMA);
    }
  }

  TEST_CASE("TeaJsonImportTooManyGroups") {
    std::string text = R"({"header":{"name":"","totalNumberOfFrames":0},"keyframes":[{"frame":0,"groups":[)";
    for (std::size_t i = 0; i <= pistoris::kTeaMaxGroups; ++i) {
      if (i != 0) text += ',';
      text += R"({"isKey":false})";
    }
    text += R"(]}]})";
    pistoris::tea::Data tea;
    CHECK(pistoris::importJsonToTea(text, &tea) == ARX_JSON_LIMIT_EXCEEDED);
  }

  TEST_CASE("TeaJsonImportFailureDoesNotMutateTea") {
    pistoris::tea::Data tea;
    tea.num_frames = 1;
    tea.num_groups = 1;
    tea.keyframes.push_back({});
    CHECK(pistoris::importJsonToTea("{}", &tea) == ARX_JSON_BAD_SCHEMA);
    CHECK(tea.keyframes.size() == 1);
  }
}
