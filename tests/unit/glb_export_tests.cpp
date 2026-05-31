// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "arx/ftl.h"
#include "external/glb.h"
#include "helpers.h"
#include "utils/cursor.h"
#include "utils/log.h"

#include <cstdint>
#include <cstring>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <vector>

// save/load roundtrip so Extras gets populated, matching production flow
static pistoris::ftl::Data sealFtlData(const pistoris::ftl::Data& d) {
  pistoris::WriteCursor wc;
  REQUIRE(pistoris::saveFtl(&d, wc) == ARX_OK);
  auto buf = wc.take();
  pistoris::ReadCursor rcur(buf.data(), buf.size());
  pistoris::ftl::Data sealed;
  REQUIRE(pistoris::loadFtl(&sealed, rcur) == ARX_OK);
  return sealed;
}

static constexpr uint32_t kGlbMagic      = 0x46546C67u;
static constexpr uint32_t kChunkTypeJson = 0x4E4F534Au;
static constexpr uint32_t kChunkTypeBin  = 0x004E4942u;

static uint32_t read32(const uint8_t* p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}

// Assumes a well-formed GLB
static std::string extractGlbJson(const std::vector<uint8_t>& glb) {
  REQUIRE(glb.size() >= 20);
  REQUIRE(read32(glb.data()) == kGlbMagic);
  uint32_t json_len = read32(glb.data() + 12);
  REQUIRE(read32(glb.data() + 16) == kChunkTypeJson);
  return std::string(reinterpret_cast<const char*>(glb.data() + 20), json_len);
}

static std::span<const uint8_t> extractGlbBin(const std::vector<uint8_t>& glb) {
  uint32_t json_len = read32(glb.data() + 12);
  size_t bin_off    = 20 + json_len;
  REQUIRE(glb.size() >= bin_off + 8);
  uint32_t bin_len = read32(glb.data() + bin_off);
  REQUIRE(read32(glb.data() + bin_off + 4) == kChunkTypeBin);
  return std::span<const uint8_t>(glb.data() + bin_off + 8, bin_len);
}

static bool jsonContains(const std::string& json, const char* needle) { return json.find(needle) != std::string::npos; }

struct GlbExportLogCapture {
  ArxLogLevel last_level = ARX_LOG_DEBUG;
  std::string last_msg;

  GlbExportLogCapture() {
    pistoris::log_fn = [](ArxLogLevel level, const char* msg, void* ud) {
      auto* self       = static_cast<GlbExportLogCapture*>(ud);
      self->last_level = level;
      self->last_msg   = msg;
    };
    pistoris::log_ud = this;
  }

  ~GlbExportLogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }
};

TEST_SUITE("glb") {
  TEST_CASE("GlbHeader") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    // GLB header: magic, version 2, total length
    REQUIRE(glb.size() >= 12);
    CHECK(read32(glb.data() + 0) == kGlbMagic);
    CHECK(read32(glb.data() + 4) == 2u);
    CHECK(read32(glb.data() + 8) == glb.size());

    // JSON chunk header
    REQUIRE(glb.size() >= 20);
    uint32_t json_len = read32(glb.data() + 12);
    CHECK(read32(glb.data() + 16) == kChunkTypeJson);
    CHECK(json_len % 4 == 0);

    // BIN chunk header (if present)
    size_t bin_off = 20 + json_len;
    if (bin_off < glb.size()) {
      REQUIRE(glb.size() >= bin_off + 8);
      uint32_t bin_len = read32(glb.data() + bin_off);
      CHECK(read32(glb.data() + bin_off + 4) == kChunkTypeBin);
      CHECK(bin_len % 4 == 0);
      CHECK(bin_off + 8 + bin_len == glb.size());
    }
  }

  TEST_CASE("GlbEmptyMesh") {
    auto d = makeData(1);  // 1 vertex, no faces

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);
    CHECK(jsonContains(json, "\"meshes\""));
    CHECK(jsonContains(json, "\"asset\""));
  }

  TEST_CASE("GlbTriangleWithTexture") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "GRAPH/OBJ3D/BODY.BMP", 21);
    d.texture_containers.push_back(tc);
    d.faces.push_back(makeFace(0, 1, 2, 0));

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    // Material name uses stem
    CHECK(jsonContains(json, "BODY"));
    // Texture URI present
    CHECK(jsonContains(json, "GRAPH/OBJ3D/BODY.BMP"));
    // Primitive attributes
    CHECK(jsonContains(json, "POSITION"));
    CHECK(jsonContains(json, "NORMAL"));
    CHECK(jsonContains(json, "TEXCOORD_0"));
  }

  TEST_CASE("GlbFlags") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face     = makeFace(0, 1, 2, 0);
    face.type     = pistoris::kFaceBitGlow | pistoris::kFaceBitTrans;
    face.transval = 0.5f;
    d.faces.push_back(face);

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    CHECK(jsonContains(json, "BODY__TRANS__GLOW"));
    CHECK(jsonContains(json, "BLEND"));
  }

  TEST_CASE("GlbDoubleSided") {
    auto d = makeData(3);
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.type = pistoris::kFaceBitDoublesided;
    d.faces.push_back(face);

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    CHECK(jsonContains(json, "\"doubleSided\":true"));
  }

  TEST_CASE("GlbWithGroups") {
    auto d = makeData(4);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "chest", 6);
    g0.origin           = 0;
    g0.indices          = {0, 1, 2, 3};
    g0.blob_shadow_size = 2.5f;
    d.groups.push_back(g0);

    auto sealed = sealFtlData(d);
    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(sealed, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    CHECK(jsonContains(json, "\"skins\""));
    CHECK(jsonContains(json, "\"000__chest\""));
    CHECK(jsonContains(json, "JOINTS_0"));
    CHECK(jsonContains(json, "WEIGHTS_0"));
    CHECK(jsonContains(json, "inverseBindMatrices"));
    CHECK(jsonContains(json, "arx_blob_shadow_size"));
  }

  TEST_CASE("GlbNoSkinWithoutGroups") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    CHECK_FALSE(jsonContains(json, "\"skins\""));
    CHECK_FALSE(jsonContains(json, "JOINTS_0"));
  }

  TEST_CASE("GlbActions") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Action act{};
    std::memcpy(act.name, "weapon", 7);
    act.vertex_idx = 1;
    d.actions.push_back(act);

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    CHECK(jsonContains(json, "arx_action__weapon"));
    CHECK(jsonContains(json, "translation"));
  }

  TEST_CASE("GlbSelectionsExported") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Selection sel{};
    std::memcpy(sel.name, "head", 5);
    sel.selected = {0, 2};
    d.selections.push_back(sel);

    pistoris::ftl::Selection sel2{};
    std::memcpy(sel2.name, "left arm", 9);
    sel2.selected = {1};
    d.selections.push_back(sel2);

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);
    auto json_str = extractGlbJson(glb);
    auto bin      = extractGlbBin(glb);

    CHECK(jsonContains(json_str, "_HEAD"));
    CHECK(jsonContains(json_str, "_LEFT_ARM"));

    auto gltf       = nlohmann::json::parse(json_str);
    const auto& acc = gltf["accessors"];
    const auto& bvs = gltf["bufferViews"];
    const auto& pa  = gltf["meshes"][0]["primitives"][0]["attributes"];

    auto read_mask = [&](const char* key) -> std::vector<float> {
      REQUIRE(pa.contains(key));
      size_t ai     = pa[key].get<size_t>();
      const auto& a = acc[ai];
      CHECK(a["componentType"] == 5126);
      CHECK(a["type"] == "VEC4");
      size_t count = a["count"].get<size_t>();
      size_t bv    = a["bufferView"].get<size_t>();
      size_t off   = bvs[bv]["byteOffset"].get<size_t>();
      std::vector<float> rgba(count * 4);
      std::memcpy(rgba.data(), bin.data() + off, sizeof(float) * count * 4);
      std::vector<float> out(count);
      for (size_t i = 0; i < count; ++i) {
        out[i] = rgba[i * 4 + 0];
        CHECK(rgba[i * 4 + 1] == out[i]);
        CHECK(rgba[i * 4 + 2] == out[i]);
        CHECK(rgba[i * 4 + 3] == 1.0f);
      }
      return out;
    };

    auto head = read_mask("_HEAD");
    REQUIRE(head.size() == 3);
    CHECK(head[0] == 1.0f);
    CHECK(head[1] == 0.0f);
    CHECK(head[2] == 1.0f);

    auto larm = read_mask("_LEFT_ARM");
    REQUIRE(larm.size() == 3);
    CHECK(larm[0] == 0.0f);
    CHECK(larm[1] == 1.0f);
    CHECK(larm[2] == 0.0f);

    const auto& extras = gltf["meshes"][0]["extras"];
    REQUIRE(extras.contains("arx_selection_names"));
    const auto& arr = extras["arx_selection_names"];
    REQUIRE(arr.is_array());
    REQUIRE(arr.size() == 2);
    CHECK(arr[0].get<std::string>() == "head");
    CHECK(arr[1].get<std::string>() == "left arm");
  }

  TEST_CASE("GlbExportSummaryReportsEmittedSelectionColorAttributes") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Selection sel{};
    std::memcpy(sel.name, "head", 5);
    sel.selected = {0, 2};
    d.selections.push_back(sel);

    GlbExportLogCapture logs;
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    CHECK(logs.last_level == ARX_LOG_INFO);
    CHECK(logs.last_msg.find("1 primitives") != std::string::npos);
    CHECK(logs.last_msg.find("1 materials") != std::string::npos);
    CHECK(logs.last_msg.find("0 bones") != std::string::npos);
    CHECK(logs.last_msg.find("0 action points") != std::string::npos);
    CHECK(logs.last_msg.find("1 selection VEC4 attributes") != std::string::npos);
  }

  TEST_CASE("GlbBoneNamesCarryOrdinalPrefixes") {
    auto d                 = makeData(4);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group root{};
    std::memcpy(root.name, "root", 5);
    root.origin  = 0;
    root.indices = {0, 1, 2, 3};
    d.groups.push_back(root);

    pistoris::ftl::Data sealed = sealFtlData(d);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(sealed, {}, glb) == ARX_OK);
    auto gltf = nlohmann::json::parse(extractGlbJson(glb));

    REQUIRE(gltf["nodes"].size() >= 3);
    CHECK(gltf["nodes"][2]["name"].get<std::string>() == "000__root");
  }

  TEST_CASE("GlbExtrasPassthrough") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Selection sel{};
    std::memcpy(sel.name, "head", 5);
    sel.selected = {0};
    d.selections.push_back(sel);

    std::vector<std::pair<std::string, std::string>> extras = {
        {"arx_selection_names_sidecar", "model.glb.selections.txt"},
        {"my_custom_tag", "v1"},
    };

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb, std::span(extras)) == ARX_OK);
    auto gltf               = nlohmann::json::parse(extractGlbJson(glb));
    const auto& mesh_extras = gltf["meshes"][0]["extras"];
    REQUIRE(mesh_extras.contains("arx_selection_names"));
    CHECK(mesh_extras["arx_selection_names"][0].get<std::string>() == "head");
    REQUIRE(mesh_extras.contains("arx_selection_names_sidecar"));
    CHECK(mesh_extras["arx_selection_names_sidecar"].get<std::string>() == "model.glb.selections.txt");
    REQUIRE(mesh_extras.contains("my_custom_tag"));
    CHECK(mesh_extras["my_custom_tag"].get<std::string>() == "v1");

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> imp_teas;
    std::vector<std::pair<std::string, std::string>> imp_extras;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, imp_teas, &imp_extras) == ARX_OK);
    REQUIRE(imp_extras.size() == 2);
    bool saw_sidecar = false, saw_tag = false;
    for (const auto& [k, v] : imp_extras) {
      if (k == "arx_selection_names_sidecar" && v == "model.glb.selections.txt") saw_sidecar = true;
      if (k == "my_custom_tag" && v == "v1") saw_tag = true;
      CHECK(k != "arx_selection_names");
    }
    CHECK(saw_sidecar);
    CHECK(saw_tag);
  }

  TEST_CASE("GlbWithAnimation") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "root_bone", 10);
    g0.origin  = 0;
    g0.indices = {0, 1, 2};
    d.groups.push_back(g0);

    d = sealFtlData(d);

    pistoris::tea::Data tea;
    tea.num_frames = 24;
    tea.num_groups = 1;

    pistoris::tea::Keyframe kf;
    kf.num_frame  = 24;
    kf.flag_frame = pistoris::kTeaFlagFrameStep;
    kf.translate  = pistoris::ArxVector3{1.0f, 2.0f, 3.0f};
    kf.quat       = pistoris::ArxQuat{0.5f, 0.5f, 0.5f, 0.5f};

    pistoris::tea::GroupAnim ga;
    ga.quat      = {1.0f, 0.0f, 0.0f, 0.0f};
    ga.translate = {4.0f, 5.0f, 6.0f};
    ga.zoom      = {1.0f, 1.0f, 1.0f};
    kf.groups.push_back(ga);

    tea.keyframes.push_back(kf);

    const pistoris::tea::Data* tea_ptr = &tea;
    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {&tea_ptr, 1}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);

    CHECK(jsonContains(json, "\"animations\""));
    CHECK(jsonContains(json, "animation_0"));
    CHECK(jsonContains(json, "\"translation\""));
    CHECK(jsonContains(json, "\"rotation\""));
    CHECK(jsonContains(json, "\"scale\""));
    CHECK(jsonContains(json, "arx_footstep_frames"));
  }

  TEST_CASE("GlbTeaGroupMismatch") {
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));
    // no groups in FTL

    pistoris::tea::Data tea;
    tea.num_frames = 24;
    tea.num_groups = 1;  // mismatch: FTL has 0 groups
    pistoris::tea::Keyframe kf;
    kf.num_frame = 0;
    kf.groups.emplace_back();  // one group per keyframe to match num_groups
    tea.keyframes.push_back(kf);

    const pistoris::tea::Data* tea_ptr = &tea;
    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {&tea_ptr, 1}, glb) == ARX_GLB_TEA_GROUP_MISMATCH);
  }

  TEST_CASE("GlbNoGroupsForTea") {
    // FTL has no groups; TEA provided with num_groups=0 and no bones -> reject as malformed
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::tea::Data tea;
    tea.num_frames = 0;
    tea.num_groups = 0;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 0;
    tea.keyframes.push_back(kf);

    const pistoris::tea::Data* tea_ptr = &tea;
    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {&tea_ptr, 1}, glb) == ARX_GLB_NO_GROUPS_FOR_TEA);
  }

  TEST_CASE("GlbHoldSuffix") {
    // last keyframe (num_frame=10) falls short of anim_end (num_frames=24)
    // -> synthetic hold frame appended, animation name suffixed with "__h"
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "root", 5);
    g0.origin  = 0;
    g0.indices = {0, 1, 2};
    d.groups.push_back(g0);
    d = sealFtlData(d);

    pistoris::tea::Data tea;
    tea.num_frames = 24;
    tea.num_groups = 1;
    std::memcpy(tea.name, "walk", 5);
    pistoris::tea::Keyframe kf;
    kf.num_frame  = 10;
    kf.flag_frame = pistoris::kTeaFlagFrameNone;
    kf.groups.emplace_back();
    tea.keyframes.push_back(kf);

    const pistoris::tea::Data* tea_ptr = &tea;
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {&tea_ptr, 1}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);
    CHECK(jsonContains(json, "\"walk__h\""));
  }

  TEST_CASE("GlbNoHoldSuffixWhenExact") {
    // last keyframe aligns with num_frames -> no hold, no "__h" suffix
    auto d = makeData(3);
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "root", 5);
    g0.origin  = 0;
    g0.indices = {0, 1, 2};
    d.groups.push_back(g0);
    d = sealFtlData(d);

    pistoris::tea::Data tea;
    tea.num_frames = 24;
    tea.num_groups = 1;
    std::memcpy(tea.name, "attack", 7);
    pistoris::tea::Keyframe kf;
    kf.num_frame = 24;  // matches num_frames exactly
    kf.groups.emplace_back();
    tea.keyframes.push_back(kf);

    const pistoris::tea::Data* tea_ptr = &tea;
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {&tea_ptr, 1}, glb) == ARX_OK);
    auto json = extractGlbJson(glb);
    CHECK(jsonContains(json, "\"attack\""));
    CHECK_FALSE(jsonContains(json, "attack__h"));
  }

  // 3 verts x 21846 faces, unique UV per corner -> 65538 distinct ExpandKeys, exceeds uint16 max
  TEST_CASE("GlbExportTooManyVertices") {
    auto d                 = makeData(3);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};

    constexpr size_t kFaces = 21846;
    d.faces.reserve(kFaces);
    for (size_t i = 0; i < kFaces; ++i) {
      auto f = makeFace(0, 1, 2);
      // distinct u per corner -> distinct ExpandKey {vi, u_bits, v_bits} per corner
      float fi = static_cast<float>(i);
      f.u      = {fi * 3.0f, fi * 3.0f + 1.0f, fi * 3.0f + 2.0f};
      d.faces.push_back(f);
    }

    std::vector<uint8_t> glb;
    CHECK(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_GLB_TOO_MANY_VERTICES);
  }

}  // TEST_SUITE
