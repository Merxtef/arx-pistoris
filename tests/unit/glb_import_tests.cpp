// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "arx/ftl.h"
#include "arx/tea.h"
#include "external/glb.h"
#include "helpers.h"
#include "utils/cursor.h"
#include "utils/log.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr uint32_t kGlbMagic      = 0x46546C67u;
constexpr uint32_t kChunkTypeJson = 0x4E4F534Au;
constexpr uint32_t kChunkTypeBin  = 0x004E4942u;

struct GlbImportLogCapture {
  ArxLogLevel last_level = ARX_LOG_DEBUG;
  std::string last_msg;
  std::vector<std::string> messages;

  GlbImportLogCapture() {
    pistoris::log_fn = [](ArxLogLevel level, const char* msg, void* ud) {
      auto* self       = static_cast<GlbImportLogCapture*>(ud);
      self->last_level = level;
      self->last_msg   = msg;
      self->messages.emplace_back(msg);
    };
    pistoris::log_ud = this;
  }

  ~GlbImportLogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }
};

// hand-assemble a tiny GLB from explicit JSON + BIN
std::vector<uint8_t> assembleGlb(const std::string& gltf_json, const std::vector<uint8_t>& bin) {
  std::string json_padded = gltf_json;
  while (json_padded.size() % 4 != 0) json_padded.push_back(' ');
  std::vector<uint8_t> bin_padded = bin;
  while (bin_padded.size() % 4 != 0) bin_padded.push_back(0);

  uint32_t total = 12 + 8 + static_cast<uint32_t>(json_padded.size());
  if (!bin_padded.empty()) total += 8 + static_cast<uint32_t>(bin_padded.size());

  std::vector<uint8_t> out;
  out.reserve(total);
  auto push32 = [&](uint32_t v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  push32(kGlbMagic);
  push32(2);
  push32(total);
  push32(static_cast<uint32_t>(json_padded.size()));
  push32(kChunkTypeJson);
  out.insert(out.end(), json_padded.begin(), json_padded.end());
  if (!bin_padded.empty()) {
    push32(static_cast<uint32_t>(bin_padded.size()));
    push32(kChunkTypeBin);
    out.insert(out.end(), bin_padded.begin(), bin_padded.end());
  }
  return out;
}

std::string extractGlbJson(const std::vector<uint8_t>& glb) {
  REQUIRE(glb.size() >= 20);
  uint32_t json_len = 0;
  std::memcpy(&json_len, glb.data() + 12, 4);
  REQUIRE(json_len <= glb.size() - 20);
  return std::string(reinterpret_cast<const char*>(glb.data() + 20), json_len);
}

std::vector<uint8_t> extractGlbBin(const std::vector<uint8_t>& glb) {
  uint32_t json_len = 0;
  std::memcpy(&json_len, glb.data() + 12, 4);
  size_t bin_off = 20 + json_len;
  if (bin_off >= glb.size()) return {};
  REQUIRE(glb.size() >= bin_off + 8);
  uint32_t bin_len = 0;
  std::memcpy(&bin_len, glb.data() + bin_off, 4);
  REQUIRE(bin_off + 8 + bin_len <= glb.size());
  return std::vector<uint8_t>(glb.data() + bin_off + 8, glb.data() + bin_off + 8 + bin_len);
}

// append little-endian to BIN buffer
void appendF32(std::vector<uint8_t>& bin, float v) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
  bin.insert(bin.end(), p, p + 4);
}
void appendU16(std::vector<uint8_t>& bin, uint16_t v) {
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
  bin.insert(bin.end(), p, p + 2);
}

// makeData(3) is collinear; override positions so per-face normal recompute doesn't skip
inline pistoris::ftl::Data makeTriData() {
  auto d                 = makeData(3);
  d.vertices[0].position = {0.0f, 0.0f, 0.0f};
  d.vertices[1].position = {1.0f, 0.0f, 0.0f};
  d.vertices[2].position = {0.0f, 1.0f, 0.0f};
  return d;
}

}  // namespace

TEST_SUITE("glb_import") {
  TEST_CASE("ImportRoundtripStaticTriangle") {
    auto d = makeTriData();
    d.faces.push_back(makeFace(0, 1, 2));

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    GlbImportLogCapture logs;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "model.glb", imp, teas) == ARX_OK);

    // 3 mesh verts + 1 synthetic origin
    CHECK(imp.vertices.size() == 4);
    CHECK(imp.faces.size() == 1);
    CHECK(std::string(imp.header.name) == "arx_pistoris\\model.glb");
    // synthetic origin at (0,0,0) is the last vertex
    CHECK(imp.header.origin == 3);
    CHECK(imp.vertices[3].position.x == 0.0f);
    CHECK(imp.vertices[3].position.y == 0.0f);
    CHECK(imp.vertices[3].position.z == 0.0f);
    bool warned = false;
    for (const auto& msg : logs.messages)
      if (msg.find("synthetic origin/action vertices are not in any selection") != std::string::npos) warned = true;
    CHECK(warned);
  }

  TEST_CASE("ImportShiftsByHeaderOrigin") {
    // export shifts world by -origin; import synthesizes origin at (0,0,0). Relative geometry preserved.
    auto d                 = makeTriData();
    d.vertices[0].position = {1.0f, 2.0f, 3.0f};  // header.origin = 0 -> shift (1,2,3)
    d.vertices[1].position = {4.0f, -1.0f, 0.0f};
    d.vertices[2].position = {0.0f, 0.0f, -5.0f};
    d.faces.push_back(makeFace(0, 1, 2));

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    // v0 at pivot -> (0,0,0); others shifted by -(1,2,3)
    bool found_p0 = false, found_p1 = false, found_p2 = false;
    for (size_t i = 0; i < 3; ++i) {
      auto& p = imp.vertices[i].position;
      if (p.x == 0.0f && p.y == 0.0f && p.z == 0.0f) found_p0 = true;
      if (p.x == 3.0f && p.y == -3.0f && p.z == -3.0f) found_p1 = true;
      if (p.x == -1.0f && p.y == -2.0f && p.z == -8.0f) found_p2 = true;
    }
    CHECK(found_p0);
    CHECK(found_p1);
    CHECK(found_p2);
  }

  TEST_CASE("ImportPreservesMaterialAndTransparency") {
    auto d = makeTriData();
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "BODY.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face     = makeFace(0, 1, 2, 0);
    face.type     = pistoris::kFaceBitTrans;
    face.transval = 0.25f;
    d.faces.push_back(face);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    REQUIRE(imp.faces.size() == 1);
    CHECK((imp.faces[0].type & pistoris::kFaceBitTrans) != 0);
    CHECK(imp.faces[0].transval == doctest::Approx(0.25f).epsilon(0.01));
    REQUIRE_FALSE(imp.texture_containers.empty());
    // material name is GLB material name: "BODY" (pathStem of "BODY.BMP")
    CHECK(std::string_view(imp.texture_containers[0].filename).find("BODY") != std::string_view::npos);
  }

  TEST_CASE("ImportDedupsFlaggedGlbMaterialsToOneTexture") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "materials":[
        {"name":"my_tex__NO_SHADOW","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
        {"name":"my_tex__DOUBLESIDED","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
        {"name":"my_tex__NO_SHADOW__DOUBLESIDED","pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}
      ],
      "textures":[{"source":0}],
      "images":[{"uri":"GRAPH/OBJ3D/MY_TEX.BMP"}],
      "meshes":[{"primitives":[
        {"attributes":{"POSITION":0},"indices":1,"material":0,"mode":4},
        {"attributes":{"POSITION":0},"indices":1,"material":1,"mode":4},
        {"attributes":{"POSITION":0},"indices":1,"material":2,"mode":4}
      ]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.texture_containers.size() == 1);
    CHECK(std::string(imp.texture_containers[0].filename) == "GRAPH\\OBJ3D\\MY_TEX.BMP");

    REQUIRE(imp.faces.size() == 3);
    CHECK(imp.faces[0].texture_id == 0);
    CHECK(imp.faces[1].texture_id == 0);
    CHECK(imp.faces[2].texture_id == 0);
    CHECK((imp.faces[0].type & pistoris::kFaceBitNoShadow) != 0);
    CHECK((imp.faces[0].type & pistoris::kFaceBitDoublesided) == 0);
    CHECK((imp.faces[1].type & pistoris::kFaceBitNoShadow) == 0);
    CHECK((imp.faces[1].type & pistoris::kFaceBitDoublesided) != 0);
    CHECK((imp.faces[2].type & pistoris::kFaceBitNoShadow) != 0);
    CHECK((imp.faces[2].type & pistoris::kFaceBitDoublesided) != 0);
  }

  TEST_CASE("ImportPreservesDoubleSided") {
    auto d = makeTriData();
    pistoris::ftl::TextureContainer tc{};
    std::memcpy(tc.filename, "WALL.BMP", 9);
    d.texture_containers.push_back(tc);
    auto face = makeFace(0, 1, 2, 0);
    face.type = pistoris::kFaceBitDoublesided;
    d.faces.push_back(face);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    REQUIRE(imp.faces.size() == 1);
    CHECK((imp.faces[0].type & pistoris::kFaceBitDoublesided) != 0);
  }

  TEST_CASE("ImportRejectsBadMagic") {
    std::vector<uint8_t> bad(64, 0);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(bad), "", imp, teas) == ARX_INVALID_IDENTIFIER);
  }

  TEST_CASE("ImportRejectsTruncated") {
    std::vector<uint8_t> truncated(8, 0);
    std::memcpy(truncated.data(), &kGlbMagic, 4);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(truncated), "", imp, teas) == ARX_UNEXPECTED_EOF);
  }

  TEST_CASE("ImportRejectsBadVersion") {
    std::vector<uint8_t> glb(12, 0);
    uint32_t magic = kGlbMagic, ver = 1, total = 12;
    std::memcpy(glb.data() + 0, &magic, 4);
    std::memcpy(glb.data() + 4, &ver, 4);
    std::memcpy(glb.data() + 8, &total, 4);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsDeclaredLengthExceedsBuffer") {
    // Use a minimal valid GLB and overwrite total_len with a value beyond the buffer
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);

    uint32_t inflated = static_cast<uint32_t>(glb.size()) + 100;
    std::memcpy(glb.data() + 8, &inflated, 4);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsDeclaredLengthShorterThanBuffer") {
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);

    uint32_t shortened = static_cast<uint32_t>(glb.size()) - 4;
    std::memcpy(glb.data() + 8, &shortened, 4);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsFirstChunkNotJson") {
    // Same baseline as above; overwrite the first chunk's type field
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);

    // 12-byte header + 4-byte json_len = 16: chunk type field
    std::memcpy(glb.data() + 16, &kChunkTypeBin, 4);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsAccessorOffsetOverflow") {
    std::vector<uint8_t> bin(36, 0);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":4}]}],
      "buffers":[{"byteLength":36}],
      "bufferViews":[
        {"buffer":0,"byteOffset":18446744073709551615,"byteLength":36}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportCatchesJsonShapeExceptions") {
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"translation":[1]}]
    })";
    auto glb      = assembleGlb(j, {});
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsMissingAccessor") {
    // POSITION references accessor index 99 but only 2 accessors exist
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":99},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsFloatIndices") {
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 2.0f);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":48}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":12}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5126,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsBadComponentType") {
    // POSITION accessor componentType 9999 -> compSize() returns 0
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":9999,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("ImportRejectsAccessorPastBuffer") {
    // bufferView fits in BIN (36 bytes) but accessor count=10 of VEC3 needs 120 bytes
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":10,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_BAD_FORMAT);
  }

  // 4-vertex quad with mode=5 (TRIANGLE_STRIP); triangulator emits 2 triangles with alternating winding
  TEST_CASE("ImportTriangulatesStrip") {
    std::vector<uint8_t> bin;
    // 4 vec3 positions = 48 bytes
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    // 4 u16 indices = 8 bytes
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    appendU16(bin, 3);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":5}]}],
      "buffers":[{"byteLength":56}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":48},
        {"buffer":0,"byteOffset":48,"byteLength":8}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":4,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.faces.size() == 2);
  }

  // 4-vertex fan with mode=6 (TRIANGLE_FAN); triangulator emits 2 triangles sharing vertex 0
  TEST_CASE("ImportTriangulatesFan") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    appendU16(bin, 3);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":6}]}],
      "buffers":[{"byteLength":56}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":48},
        {"buffer":0,"byteOffset":48,"byteLength":8}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":4,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":4,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.faces.size() == 2);
  }

  // Indices accessor with componentType=5121 (UBYTE); covers decodeIndex byte arm + compSize byte arm
  TEST_CASE("ImportUByteIndices") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    // 3 u8 indices + 1 byte padding to keep bufferView 4-byte aligned
    bin.push_back(0);
    bin.push_back(1);
    bin.push_back(2);
    bin.push_back(0);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":40}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":4}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5121,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.faces.size() == 1);
  }

  // TEXCOORD_0 with normalized UBYTE; covers decodeFloats UBYTE-normalized arm
  TEST_CASE("ImportTexcoordNormalizedUByte") {
    std::vector<uint8_t> bin;
    // 3 vec3 positions = 36 bytes
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    // 3 u16 indices + 2 bytes padding = 8 bytes
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);

    bin.push_back(0);
    bin.push_back(0);

    // 3 u8 vec2 UVs + 2 bytes padding = 8 bytes
    bin.push_back(255);
    bin.push_back(0);  // UV 0: (1.0, 0.0)

    bin.push_back(0);
    bin.push_back(0);  // UV 1: (0.0, 0.0)

    bin.push_back(128);
    bin.push_back(128);  // UV 2: (~0.502, ~0.502)

    bin.push_back(0);
    bin.push_back(0);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0,"TEXCOORD_0":2},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":52}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6},
        {"buffer":0,"byteOffset":44,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":2,"componentType":5121,"count":3,"type":"VEC2","normalized":true}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    REQUIRE(imp.faces.size() == 1);
    const auto& f = imp.faces[0];
    CHECK(f.u.x == doctest::Approx(1.0f));
    CHECK(f.v.x == doctest::Approx(0.0f));
    CHECK(f.u.y == doctest::Approx(0.0f));
    CHECK(f.v.y == doctest::Approx(0.0f));
    CHECK(f.u.z == doctest::Approx(128.0f / 255.0f));
    CHECK(f.v.z == doctest::Approx(128.0f / 255.0f));
  }

  TEST_CASE("ImportRejectsDisconnectedMultipleSkins") {
    // two skins under different scene roots -> merged skeleton has 2 roots -> MULTIPLE_SKINS
    // Layout:
    //   IBMs (skin 0): 64 bytes (1 identity mat4)
    //   IBMs (skin 1): 64 bytes (1 identity mat4)
    //   POSITION (mesh 0): 3 vec3 = 36 bytes  [128..163]
    //   indices (mesh 0): 3 u16 = 6 bytes     [164..169]
    //   JOINTS_0 (mesh 0): 3 vec4 u16 = 24 bytes [170..193]
    //   WEIGHTS_0 (mesh 0): 3 vec4 float = 48 bytes [194..241]
    //   POSITION (mesh 1): 3 vec3 = 36 bytes  [242..277]
    //   indices (mesh 1): 3 u16 = 6 bytes     [278..283]
    //   JOINTS_0 (mesh 1): 3 vec4 u16 = 24 bytes [284..307]
    //   WEIGHTS_0 (mesh 1): 3 vec4 float = 48 bytes [308..355]
    std::vector<uint8_t> bin;
    auto mat4_identity = [&]() {
      float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      for (int i = 0; i < 16; ++i) appendF32(bin, m[i]);
    };
    mat4_identity();  // skin 0 IBM
    mat4_identity();  // skin 1 IBM
    auto tri_attrs = [&]() {
      // Non-collinear positions
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 1);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 1);
      appendF32(bin, 0);
      // 3 indices
      appendU16(bin, 0);
      appendU16(bin, 1);
      appendU16(bin, 2);
      // 3 joint vec4 (only first component used, rest 0)
      for (int i = 0; i < 12; ++i) appendU16(bin, 0);
      // 3 weight vec4 ([0]=1.0)
      for (int v = 0; v < 3; ++v) {
        appendF32(bin, 1.0f);
        appendF32(bin, 0);
        appendF32(bin, 0);
        appendF32(bin, 0);
      }
    };
    tri_attrs();  // mesh 0
    tri_attrs();  // mesh 1

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1,2,3]}],
      "nodes":[
        {"mesh":0,"skin":0},
        {"mesh":1,"skin":1},
        {"name":"jointA"},
        {"name":"jointB"}
      ],
      "meshes":[
        {"primitives":[{"attributes":{"POSITION":1,"JOINTS_0":3,"WEIGHTS_0":4},"indices":2,"mode":4}]},
        {"primitives":[{"attributes":{"POSITION":6,"JOINTS_0":8,"WEIGHTS_0":9},"indices":7,"mode":4}]}
      ],
      "skins":[
        {"joints":[2],"inverseBindMatrices":0},
        {"joints":[3],"inverseBindMatrices":5}
      ],
      "buffers":[{"byteLength":356}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":64},
        {"buffer":0,"byteOffset":128,"byteLength":36},
        {"buffer":0,"byteOffset":164,"byteLength":6},
        {"buffer":0,"byteOffset":170,"byteLength":24},
        {"buffer":0,"byteOffset":194,"byteLength":48},
        {"buffer":0,"byteOffset":64,"byteLength":64},
        {"buffer":0,"byteOffset":242,"byteLength":36},
        {"buffer":0,"byteOffset":278,"byteLength":6},
        {"buffer":0,"byteOffset":284,"byteLength":24},
        {"buffer":0,"byteOffset":308,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"},
        {"bufferView":5,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":6,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":7,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":8,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":9,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_MULTIPLE_SKINS);
  }

  TEST_CASE("ImportMergesConnectedMultipleSkins") {
    // two skins sharing a common parent merge into one armature.
    // tree: root(2) -> child1(3), child2(4); skin0=[root,child1], skin1=[root,child2]
    // after merge: 3 joints, single root
    std::vector<uint8_t> bin;
    auto mat4_identity = [&]() {
      float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      for (int i = 0; i < 16; ++i) appendF32(bin, m[i]);
    };
    // Skin 0 IBMs: 2 identity mat4
    mat4_identity();
    mat4_identity();
    // Skin 1 IBMs: 2 identity mat4
    mat4_identity();
    mat4_identity();
    // Mesh 0 attribs (skin 0)
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    for (int i = 0; i < 12; ++i) appendU16(bin, 0);
    for (int v = 0; v < 3; ++v) {
      appendF32(bin, 1.0f);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
    }
    // Mesh 1 attribs (skin 1) - identical layout
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    for (int i = 0; i < 12; ++i) appendU16(bin, 0);
    for (int v = 0; v < 3; ++v) {
      appendF32(bin, 1.0f);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
    }
    // Layout offsets:
    //   Skin 0 IBMs: 0..127
    //   Skin 1 IBMs: 128..255
    //   Mesh 0 POS: 256..291  IDX: 292..297  JOINTS: 298..321  WEIGHTS: 322..369
    //   Mesh 1 POS: 370..405  IDX: 406..411  JOINTS: 412..435  WEIGHTS: 436..483

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1,2]}],
      "nodes":[
        {"mesh":0,"skin":0},
        {"mesh":1,"skin":1},
        {"name":"root","children":[3,4]},
        {"name":"child1"},
        {"name":"child2"}
      ],
      "meshes":[
        {"primitives":[{"attributes":{"POSITION":2,"JOINTS_0":4,"WEIGHTS_0":5},"indices":3,"mode":4}]},
        {"primitives":[{"attributes":{"POSITION":7,"JOINTS_0":9,"WEIGHTS_0":10},"indices":8,"mode":4}]}
      ],
      "skins":[
        {"joints":[2,3],"inverseBindMatrices":0},
        {"joints":[2,4],"inverseBindMatrices":1}
      ],
      "buffers":[{"byteLength":484}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":128},
        {"buffer":0,"byteOffset":128,"byteLength":128},
        {"buffer":0,"byteOffset":256,"byteLength":36},
        {"buffer":0,"byteOffset":292,"byteLength":6},
        {"buffer":0,"byteOffset":298,"byteLength":24},
        {"buffer":0,"byteOffset":322,"byteLength":48},
        {"buffer":0,"byteOffset":256,"byteLength":36},
        {"buffer":0,"byteOffset":370,"byteLength":36},
        {"buffer":0,"byteOffset":406,"byteLength":6},
        {"buffer":0,"byteOffset":412,"byteLength":24},
        {"buffer":0,"byteOffset":436,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":2,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":2,"type":"MAT4"},
        {"bufferView":2,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":4,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":5,"componentType":5126,"count":3,"type":"VEC4"},
        {"bufferView":6,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":7,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":8,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":9,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":10,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    // Unified joint set: [root, child1, child2] -> 3 groups
    CHECK(imp.groups.size() == 3);
  }

  TEST_CASE("ImportAcceptsIbmDisagreement") {
    // bind position comes from joint worldTransform, not IBMs, so per-skin IBM disagreement
    // no longer blocks import (DEBUG diagnostic only)
    std::vector<uint8_t> bin;
    auto mat4_identity = [&]() {
      float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      for (int i = 0; i < 16; ++i) appendF32(bin, m[i]);
    };
    auto mat4_translated = [&]() {
      // Identity but translation = (5,0,0) -> last column [12]=5
      float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 0, 0, 1};
      for (int i = 0; i < 16; ++i) appendF32(bin, m[i]);
    };
    mat4_identity();    // skin 0 IBM (joint 2)
    mat4_translated();  // skin 1 IBM (joint 2) - disagrees
    auto tri = [&]() {
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 1);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 1);
      appendF32(bin, 0);
      appendU16(bin, 0);
      appendU16(bin, 1);
      appendU16(bin, 2);
      for (int i = 0; i < 12; ++i) appendU16(bin, 0);
      for (int v = 0; v < 3; ++v) {
        appendF32(bin, 1.0f);
        appendF32(bin, 0);
        appendF32(bin, 0);
        appendF32(bin, 0);
      }
    };
    tri();
    tri();
    // Layout: IBM0 0..63, IBM1 64..127, mesh0 attribs 128..241, mesh1 attribs 242..355

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1,2]}],
      "nodes":[
        {"mesh":0,"skin":0},
        {"mesh":1,"skin":1},
        {"name":"shared"}
      ],
      "meshes":[
        {"primitives":[{"attributes":{"POSITION":2,"JOINTS_0":4,"WEIGHTS_0":5},"indices":3,"mode":4}]},
        {"primitives":[{"attributes":{"POSITION":7,"JOINTS_0":9,"WEIGHTS_0":10},"indices":8,"mode":4}]}
      ],
      "skins":[
        {"joints":[2],"inverseBindMatrices":0},
        {"joints":[2],"inverseBindMatrices":1}
      ],
      "buffers":[{"byteLength":356}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":64},
        {"buffer":0,"byteOffset":64,"byteLength":64},
        {"buffer":0,"byteOffset":128,"byteLength":36},
        {"buffer":0,"byteOffset":164,"byteLength":6},
        {"buffer":0,"byteOffset":170,"byteLength":24},
        {"buffer":0,"byteOffset":194,"byteLength":48},
        {"buffer":0,"byteOffset":128,"byteLength":36},
        {"buffer":0,"byteOffset":242,"byteLength":36},
        {"buffer":0,"byteOffset":278,"byteLength":6},
        {"buffer":0,"byteOffset":284,"byteLength":24},
        {"buffer":0,"byteOffset":308,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":2,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":4,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":5,"componentType":5126,"count":3,"type":"VEC4"},
        {"bufferView":6,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":7,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":8,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":9,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":10,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.groups.size() == 1);
  }

  TEST_CASE("ImportAcceptsSkinnedMeshWithTransform") {
    // per glTF spec, a skinned mesh node's transform is ignored (skin owns vertex placement)
    std::vector<uint8_t> bin;
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; ++i) appendF32(bin, m[i]);  // 1 identity IBM
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    for (int i = 0; i < 12; ++i) appendU16(bin, 0);
    for (int v = 0; v < 3; ++v) {
      appendF32(bin, 1.0f);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
    }
    // Layout: IBM 0..63, POS 64..99, IDX 100..105, JOINTS 106..129, WEIGHTS 130..177

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1]}],
      "nodes":[
        {"mesh":0,"skin":0,"translation":[1,2,3]},
        {"name":"joint"}
      ],
      "meshes":[
        {"primitives":[{"attributes":{"POSITION":1,"JOINTS_0":3,"WEIGHTS_0":4},"indices":2,"mode":4}]}
      ],
      "skins":[{"joints":[1],"inverseBindMatrices":0}],
      "buffers":[{"byteLength":178}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":64},
        {"buffer":0,"byteOffset":64,"byteLength":36},
        {"buffer":0,"byteOffset":100,"byteLength":6},
        {"buffer":0,"byteOffset":106,"byteLength":24},
        {"buffer":0,"byteOffset":130,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.groups.size() == 1);
  }

  TEST_CASE("ImportActionPointFromArxActionEmpty") {
    // child empty "arx_action__weapon" -> FTL Action "weapon" attached to its joint
    std::vector<uint8_t> bin;
    float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; ++i) appendF32(bin, ident[i]);  // 1 IBM (identity)
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    for (int i = 0; i < 12; ++i) appendU16(bin, 0);
    for (int v = 0; v < 3; ++v) {
      appendF32(bin, 1.0f);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
    }
    // Layout: IBM 0..63, POS 64..99, IDX 100..105, JOINTS 106..129, WEIGHTS 130..177

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1]}],
      "nodes":[
        {"mesh":0,"skin":0},
        {"name":"joint","children":[2]},
        {"name":"arx_action__weapon","translation":[1.5,0,0]}
      ],
      "meshes":[
        {"primitives":[{"attributes":{"POSITION":1,"JOINTS_0":3,"WEIGHTS_0":4},"indices":2,"mode":4}]}
      ],
      "skins":[{"joints":[1],"inverseBindMatrices":0}],
      "buffers":[{"byteLength":178}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":64},
        {"buffer":0,"byteOffset":64,"byteLength":36},
        {"buffer":0,"byteOffset":100,"byteLength":6},
        {"buffer":0,"byteOffset":106,"byteLength":24},
        {"buffer":0,"byteOffset":130,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    REQUIRE(imp.actions.size() == 1);
    CHECK(std::string(imp.actions[0].name) == "weapon");
    // action vertex position is the empty's world translation Y-flipped: (1.5,0,0) -> (1.5,0,0)
    auto vi = imp.actions[0].vertex_idx;
    CHECK(vi >= 0);
    CHECK(imp.vertices[vi].position.x == doctest::Approx(1.5f));
    CHECK(imp.vertices[vi].position.y == doctest::Approx(0.0f));
    // action vertex must land in the joint's group so vtb resolves on roundtrip
    REQUIRE(imp.groups.size() == 1);
    bool in_group = false;
    for (auto idx : imp.groups[0].indices)
      if (idx == vi) {
        in_group = true;
        break;
      }
    CHECK(in_group);
  }

  TEST_CASE("ImportIgnoresUnnamedEmpty") {
    // empty without arx_action__ prefix is discarded silently
    std::vector<uint8_t> bin;
    float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (int i = 0; i < 16; ++i) appendF32(bin, ident[i]);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 0);
    appendF32(bin, 1);
    appendF32(bin, 0);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    for (int i = 0; i < 12; ++i) appendU16(bin, 0);
    for (int v = 0; v < 3; ++v) {
      appendF32(bin, 1.0f);
      appendF32(bin, 0);
      appendF32(bin, 0);
      appendF32(bin, 0);
    }

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1]}],
      "nodes":[
        {"mesh":0,"skin":0},
        {"name":"joint","children":[2]},
        {"name":"bone_end","translation":[0.5,0,0]}
      ],
      "meshes":[
        {"primitives":[{"attributes":{"POSITION":1,"JOINTS_0":3,"WEIGHTS_0":4},"indices":2,"mode":4}]}
      ],
      "skins":[{"joints":[1],"inverseBindMatrices":0}],
      "buffers":[{"byteLength":178}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":64},
        {"buffer":0,"byteOffset":64,"byteLength":36},
        {"buffer":0,"byteOffset":100,"byteLength":6},
        {"buffer":0,"byteOffset":106,"byteLength":24},
        {"buffer":0,"byteOffset":130,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":1,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.actions.empty());
  }

  TEST_CASE("ImportRejectsSparseAccessor") {
    // Build a minimal GLB whose POSITION accessor declares "sparse"
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);  // 3 vec3 positions
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3",
         "sparse":{"count":1,"indices":{"bufferView":0,"componentType":5123},"values":{"bufferView":0}}},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_UNSUPPORTED_FEATURE);
  }

  TEST_CASE("ImportRejectsExternalBuffer") {
    // Single accessor referencing a buffer with a uri (external)
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":4}]}],
      "buffers":[{"byteLength":36,"uri":"out.bin"}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_UNSUPPORTED_FEATURE);
  }

  TEST_CASE("ImportRejectsNonUniformScale") {
    // Mesh node with non-uniform scale
    std::vector<uint8_t> bin;
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);

    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0,"scale":[1.0,2.0,1.0]}],
      "meshes":[{"primitives":[{"attributes":{"POSITION":0},"indices":1,"mode":4}]}],
      "buffers":[{"byteLength":42}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":36},
        {"buffer":0,"byteOffset":36,"byteLength":6}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_GLB_NON_UNIFORM_SCALE);
  }

  TEST_CASE("ImportRejectsMultipleSkeletonRoots") {
    // Two unparented joints under a skin -> ARX_FTL_MULTIPLE_ROOTS.
    // 16 IBM floats = 64 bytes for one joint (identity matrix). Two joints = 128 bytes.
    std::vector<uint8_t> bin;
    auto append_identity_mat4 = [&](std::vector<uint8_t>& b) {
      float ident[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
      for (int i = 0; i < 16; ++i) appendF32(b, ident[i]);
    };
    append_identity_mat4(bin);  // joint 0 IBM
    append_identity_mat4(bin);  // joint 1 IBM
    // 3 vec3 positions
    for (int i = 0; i < 9; ++i) appendF32(bin, 0.0f);
    appendU16(bin, 0);
    appendU16(bin, 1);
    appendU16(bin, 2);
    // 3 vec4 joints (uint16)
    for (int i = 0; i < 12; ++i) appendU16(bin, 0);
    // 3 vec4 weights (float)
    for (int i = 0; i < 12; ++i) appendF32(bin, 0.0f);

    // Layout offsets:
    //   IBMs: 0..127 (128 bytes)
    //   POSITION: 128..163 (36 bytes)
    //   indices: 164..169 (6 bytes)
    //   JOINTS_0: 170..193 (24 bytes)  // 3 * vec4 u16
    //   WEIGHTS_0: 194..241 (48 bytes) // 3 * vec4 float
    std::string j = R"({
      "asset":{"version":"2.0"},
      "scene":0,
      "scenes":[{"nodes":[0,1,2]}],
      "nodes":[
        {"mesh":0,"skin":0},
        {"name":"rootA"},
        {"name":"rootB"}
      ],
      "meshes":[{"primitives":[{"attributes":{"POSITION":1,"JOINTS_0":3,"WEIGHTS_0":4},"indices":2,"mode":4}]}],
      "skins":[{"joints":[1,2],"inverseBindMatrices":0}],
      "buffers":[{"byteLength":242}],
      "bufferViews":[
        {"buffer":0,"byteOffset":0,"byteLength":128},
        {"buffer":0,"byteOffset":128,"byteLength":36},
        {"buffer":0,"byteOffset":164,"byteLength":6},
        {"buffer":0,"byteOffset":170,"byteLength":24},
        {"buffer":0,"byteOffset":194,"byteLength":48}
      ],
      "accessors":[
        {"bufferView":0,"componentType":5126,"count":2,"type":"MAT4"},
        {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
        {"bufferView":2,"componentType":5123,"count":3,"type":"SCALAR"},
        {"bufferView":3,"componentType":5123,"count":3,"type":"VEC4"},
        {"bufferView":4,"componentType":5126,"count":3,"type":"VEC4"}
      ]
    })";
    auto glb      = assembleGlb(j, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    CHECK(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_FTL_MULTIPLE_ROOTS);
  }

  TEST_CASE("ImportSkinnedRoundtripGroups") {
    auto d                 = makeData(4);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};
    d.faces.push_back(makeFace(0, 1, 2));
    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "chest", 6);
    g0.origin           = 0;
    g0.indices          = {0, 1, 2, 3};
    g0.blob_shadow_size = 0.0f;
    d.groups.push_back(g0);
    // seal so extras gets populated, matching production flow
    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto buf = wc.take();
    pistoris::ReadCursor rcur(buf.data(), buf.size());
    pistoris::ftl::Data sealed;
    REQUIRE(pistoris::loadFtl(&sealed, rcur) == ARX_OK);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(sealed, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.groups.size() == 1);
    CHECK(std::string(imp.groups[0].name) == "chest");
    // BuildPrim defaults ungrouped verts to bone 0, so all 4 come back in this group
    CHECK(imp.groups[0].indices.size() >= 4);
  }

  TEST_CASE("ImportPreservesBoneOrder") {
    // 4-bone tree root -> A -> A1, B as sibling of A. DFS order [root,A,A1,B] differs from
    // BFS [root,A,B,A1]; import must preserve DFS so TEA keyframe indices stay aligned.
    auto d                 = makeData(8);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};
    d.vertices[3].position = {2.0f, 0.0f, 0.0f};  // A origin
    d.vertices[4].position = {3.0f, 0.0f, 0.0f};  // A1 origin
    d.vertices[5].position = {0.0f, 2.0f, 0.0f};  // B origin
    d.faces.push_back(makeFace(0, 1, 2));

    // root includes A.origin (=3) and B.origin (=5) so BuildFtlExtras finds root as parent
    pistoris::ftl::Group root{}, a{}, a1{}, b{};
    std::memcpy(root.name, "root", 4);
    root.origin  = 0;
    root.indices = {0, 1, 2, 3, 5, 6, 7};
    std::memcpy(a.name, "a", 1);
    a.origin = 3;
    // A includes A1.origin (=4) so A is A1's parent
    a.indices = {3, 4};
    std::memcpy(a1.name, "a1", 2);
    a1.origin  = 4;
    a1.indices = {4};
    std::memcpy(b.name, "b", 1);
    b.origin  = 5;
    b.indices = {5};
    d.groups.push_back(root);
    d.groups.push_back(a);
    d.groups.push_back(a1);
    d.groups.push_back(b);

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto buf = wc.take();
    pistoris::ReadCursor rc(buf.data(), buf.size());
    pistoris::ftl::Data sealed;
    REQUIRE(pistoris::loadFtl(&sealed, rc) == ARX_OK);

    // Confirm the sealed FTL is in DFS order (not BFS) so the test actually exercises the fix.
    REQUIRE(sealed.groups.size() == 4);
    CHECK(std::string(sealed.groups[0].name) == "root");
    CHECK(std::string(sealed.groups[1].name) == "a");
    CHECK(std::string(sealed.groups[2].name) == "a1");
    CHECK(std::string(sealed.groups[3].name) == "b");

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(sealed, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.groups.size() == 4);
    CHECK(std::string(imp.groups[0].name) == "root");
    CHECK(std::string(imp.groups[1].name) == "a");
    CHECK(std::string(imp.groups[2].name) == "a1");
    CHECK(std::string(imp.groups[3].name) == "b");
  }

  TEST_CASE("ImportUsesBoneOrdinalPrefixesWhenJointOrderChanges") {
    auto d                 = makeData(8);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};
    d.vertices[3].position = {2.0f, 0.0f, 0.0f};
    d.vertices[4].position = {3.0f, 0.0f, 0.0f};
    d.vertices[5].position = {0.0f, 2.0f, 0.0f};
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group root{}, a{}, a1{}, b{};
    std::memcpy(root.name, "root", 5);
    root.origin  = 0;
    root.indices = {0, 1, 2, 3, 5, 6, 7};
    std::memcpy(a.name, "a", 2);
    a.origin  = 3;
    a.indices = {3, 4};
    std::memcpy(a1.name, "a1", 3);
    a1.origin  = 4;
    a1.indices = {4};
    std::memcpy(b.name, "b", 2);
    b.origin  = 5;
    b.indices = {5};
    d.groups.push_back(root);
    d.groups.push_back(a);
    d.groups.push_back(a1);
    d.groups.push_back(b);

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto buf = wc.take();
    pistoris::ReadCursor rc(buf.data(), buf.size());
    pistoris::ftl::Data sealed;
    REQUIRE(pistoris::loadFtl(&sealed, rc) == ARX_OK);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(sealed, {}, glb) == ARX_OK);

    auto gltf = nlohmann::json::parse(extractGlbJson(glb));
    REQUIRE(gltf["skins"].size() == 1);
    // Simulate a DCC changing skin joint order from [root,a,a1,b] to [root,a,b,a1].
    gltf["skins"][0]["joints"] = nlohmann::json::array({2, 3, 5, 4});
    auto bin                   = extractGlbBin(glb);
    auto reordered_glb         = assembleGlb(gltf.dump(), bin);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(reordered_glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.groups.size() == 4);
    CHECK(std::string(imp.groups[0].name) == "root");
    CHECK(std::string(imp.groups[1].name) == "a");
    CHECK(std::string(imp.groups[2].name) == "a1");
    CHECK(std::string(imp.groups[3].name) == "b");
  }

  // --- Animation import (linear) ---

  // single-bone FTL via save/load round so extras gets populated, matching production flow
  inline pistoris::ftl::Data makeSealedSingleBoneFtl() {
    auto d                 = makeData(4);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};
    d.faces.push_back(makeFace(0, 1, 2));
    pistoris::ftl::Group g0{};
    std::memcpy(g0.name, "chest", 6);
    g0.origin           = 0;
    g0.indices          = {0, 1, 2, 3};
    g0.blob_shadow_size = 0.0f;
    d.groups.push_back(g0);

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto buf = wc.take();
    pistoris::ReadCursor rc(buf.data(), buf.size());
    pistoris::ftl::Data sealed;
    REQUIRE(pistoris::loadFtl(&sealed, rc) == ARX_OK);
    return sealed;
  }

  // 1-group TEA with N keyframes carrying non-trivial bone TRS
  inline pistoris::tea::Data makeAnimTea(int32_t nkf, int32_t num_frames) {
    pistoris::tea::Data t;
    t.num_groups = 1;
    t.num_frames = num_frames;
    std::memcpy(t.name, "test_walk", 9);
    for (int32_t i = 0; i < nkf; ++i) {
      pistoris::tea::Keyframe kf;
      kf.num_frame  = (i + 1) * (num_frames / nkf);
      kf.flag_frame = pistoris::kTeaFlagFrameNone;
      pistoris::tea::GroupAnim ga;
      ga.translate = {0.1f * i, 0.0f, 0.0f};
      ga.quat      = {1.0f, 0.0f, 0.0f, 0.0f};  // identity (w,x,y,z)
      ga.zoom      = {0.0f, 0.0f, 0.0f};
      kf.groups.push_back(ga);
      t.keyframes.push_back(std::move(kf));
    }
    return t;
  }

  TEST_CASE("ImportAnimationRoundtripBoneTRS") {
    auto ftl = makeSealedSingleBoneFtl();
    auto tea = makeAnimTea(3, 24);
    REQUIRE(pistoris::validateTea(&tea) == ARX_OK);

    const pistoris::tea::Data* tea_ptrs[1] = {&tea};
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(ftl, std::span(tea_ptrs, 1), glb) == ARX_OK);

    pistoris::ftl::Data imp_ftl;
    std::vector<pistoris::tea::Data> imp_teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp_ftl, imp_teas) == ARX_OK);

    REQUIRE(imp_teas.size() == 1);
    const auto& it = imp_teas[0];
    CHECK(it.num_groups == 1);
    REQUIRE(it.keyframes.size() == 3);
    for (size_t k = 0; k < 3; ++k) {
      CHECK(it.keyframes[k].num_frame == tea.keyframes[k].num_frame);
      const auto& orig = tea.keyframes[k].groups[0];
      const auto& back = it.keyframes[k].groups[0];
      CHECK(back.translate.x == doctest::Approx(orig.translate.x).epsilon(1e-4f));
      CHECK(back.translate.y == doctest::Approx(orig.translate.y).epsilon(1e-4f));
      CHECK(back.translate.z == doctest::Approx(orig.translate.z).epsilon(1e-4f));
      CHECK(back.quat.w == doctest::Approx(orig.quat.w).epsilon(1e-4f));
      CHECK(back.zoom.x == doctest::Approx(orig.zoom.x).epsilon(1e-4f));
    }
  }

  TEST_CASE("ImportAnimationStripsHoldSuffix") {
    auto ftl = makeSealedSingleBoneFtl();
    auto tea = makeAnimTea(2, 24);
    // force a hold: last keyframe at 8 but num_frames = 24
    tea.keyframes[0].num_frame = 4;
    tea.keyframes[1].num_frame = 8;
    tea.num_frames             = 24;
    REQUIRE(pistoris::validateTea(&tea) == ARX_OK);

    const pistoris::tea::Data* tea_ptrs[1] = {&tea};
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(ftl, std::span(tea_ptrs, 1), glb) == ARX_OK);

    pistoris::ftl::Data imp_ftl;
    std::vector<pistoris::tea::Data> imp_teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp_ftl, imp_teas) == ARX_OK);

    REQUIRE(imp_teas.size() == 1);
    // hold marker stripped: no "__h" suffix, num_frames recovered, hold keyframe popped
    CHECK(std::string(imp_teas[0].name) == "test_walk");
    CHECK(imp_teas[0].num_frames == 24);
    REQUIRE(imp_teas[0].keyframes.size() == 2);
    CHECK(imp_teas[0].keyframes.back().num_frame == 8);
  }

  TEST_CASE("ImportAnimationRecoversFootstepAndAudio") {
    auto ftl                    = makeSealedSingleBoneFtl();
    auto tea                    = makeAnimTea(2, 24);
    tea.keyframes[0].flag_frame = pistoris::kTeaFlagFrameStep;
    pistoris::tea::Sample s{};
    std::memcpy(s.name, "step.wav", 8);
    tea.keyframes[1].sample = s;
    REQUIRE(pistoris::validateTea(&tea) == ARX_OK);

    const pistoris::tea::Data* tea_ptrs[1] = {&tea};
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(ftl, std::span(tea_ptrs, 1), glb) == ARX_OK);

    pistoris::ftl::Data imp_ftl;
    std::vector<pistoris::tea::Data> imp_teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp_ftl, imp_teas) == ARX_OK);

    REQUIRE(imp_teas.size() == 1);
    REQUIRE(imp_teas[0].keyframes.size() >= 2);
    CHECK(imp_teas[0].keyframes[0].flag_frame == pistoris::kTeaFlagFrameStep);
    REQUIRE(imp_teas[0].keyframes[1].sample.has_value());
    CHECK(std::string(imp_teas[0].keyframes[1].sample->name) == "step.wav");
  }

  TEST_CASE("ImportAnimationSamplesMovingRootTranslationOnEveryKeyframe") {
    auto ftl = makeSealedSingleBoneFtl();
    auto tea = makeAnimTea(3, 24);
    // Sparse authored root translation becomes a sampled key_move on every imported keyframe.
    tea.keyframes[0].translate = pistoris::ArxVector3{0.0f, 0.0f, 0.0f};
    tea.keyframes[2].translate = pistoris::ArxVector3{24.0f, 0.0f, 0.0f};
    REQUIRE(pistoris::validateTea(&tea) == ARX_OK);

    const pistoris::tea::Data* tea_ptrs[1] = {&tea};
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(ftl, std::span(tea_ptrs, 1), glb) == ARX_OK);

    pistoris::ftl::Data imp_ftl;
    std::vector<pistoris::tea::Data> imp_teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp_ftl, imp_teas) == ARX_OK);

    REQUIRE(imp_teas.size() == 1);
    REQUIRE(imp_teas[0].keyframes.size() == 3);
    for (const auto& kf : imp_teas[0].keyframes) {
      REQUIRE(kf.translate.has_value());
      CHECK(kf.translate->y == doctest::Approx(0.0f));
      CHECK(kf.translate->z == doctest::Approx(0.0f));
    }
    CHECK(imp_teas[0].keyframes[0].translate->x == doctest::Approx(0.0f));
    CHECK(imp_teas[0].keyframes[1].translate->x == doctest::Approx(12.0f));
    CHECK(imp_teas[0].keyframes[2].translate->x == doctest::Approx(24.0f));
  }

  TEST_CASE("ImportAnimationUnanimatedBoneIsIdentity") {
    // 2-bone FTL with only bone 0 animated -> bone 1 yields identity GroupAnims
    auto d                 = makeData(5);
    d.vertices[0].position = {0.0f, 0.0f, 0.0f};
    d.vertices[1].position = {1.0f, 0.0f, 0.0f};
    d.vertices[2].position = {0.0f, 1.0f, 0.0f};
    d.vertices[3].position = {2.0f, 0.0f, 0.0f};  // bone 1 origin
    d.faces.push_back(makeFace(0, 1, 2));
    pistoris::ftl::Group g0{}, g1{};
    std::memcpy(g0.name, "root", 4);
    g0.origin = 0;
    // include vertex 3 so g1 (origin=3) finds g0 as its parent via FTL extras rule
    g0.indices = {0, 1, 2, 3, 4};
    std::memcpy(g1.name, "child", 5);
    g1.origin  = 3;
    g1.indices = {3};
    d.groups.push_back(g0);
    d.groups.push_back(g1);

    pistoris::WriteCursor wc;
    REQUIRE(pistoris::saveFtl(&d, wc) == ARX_OK);
    auto buf = wc.take();
    pistoris::ReadCursor rc(buf.data(), buf.size());
    pistoris::ftl::Data sealed;
    REQUIRE(pistoris::loadFtl(&sealed, rc) == ARX_OK);

    pistoris::tea::Data tea;
    tea.num_groups = 2;
    tea.num_frames = 12;
    pistoris::tea::Keyframe kf;
    kf.num_frame = 12;
    pistoris::tea::GroupAnim ga0;
    ga0.translate = {0.5f, 0.0f, 0.0f};
    ga0.quat      = {1.0f, 0.0f, 0.0f, 0.0f};
    ga0.zoom      = {0.0f, 0.0f, 0.0f};
    pistoris::tea::GroupAnim ga1;  // identity
    ga1.translate = {0.0f, 0.0f, 0.0f};
    ga1.quat      = {1.0f, 0.0f, 0.0f, 0.0f};
    ga1.zoom      = {0.0f, 0.0f, 0.0f};
    kf.groups.push_back(ga0);
    kf.groups.push_back(ga1);
    tea.keyframes.push_back(std::move(kf));
    REQUIRE(pistoris::validateTea(&tea) == ARX_OK);

    const pistoris::tea::Data* tea_ptrs[1] = {&tea};
    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(sealed, std::span(tea_ptrs, 1), glb) == ARX_OK);

    pistoris::ftl::Data imp_ftl;
    std::vector<pistoris::tea::Data> imp_teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp_ftl, imp_teas) == ARX_OK);

    REQUIRE(imp_teas.size() == 1);
    REQUIRE(imp_teas[0].keyframes.size() == 1);
    REQUIRE(imp_teas[0].keyframes[0].groups.size() == 2);
    const auto& g1back = imp_teas[0].keyframes[0].groups[1];
    CHECK(g1back.translate.x == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(g1back.translate.y == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(g1back.translate.z == doctest::Approx(0.0f).epsilon(1e-4f));
    CHECK(g1back.quat.w == doctest::Approx(1.0f).epsilon(1e-4f));
    CHECK(g1back.zoom.x == doctest::Approx(0.0f).epsilon(1e-4f));
  }

  TEST_CASE("ImportRoundtripSelection") {
    auto d = makeTriData();
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Selection sel{};
    std::memcpy(sel.name, "chest", 6);
    sel.selected = {0, 2};
    d.selections.push_back(sel);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    GlbImportLogCapture logs;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.selections.size() == 1);
    CHECK(std::string(imp.selections[0].name) == "chest");
    REQUIRE(imp.selections[0].selected.size() == 2);
    CHECK(imp.selections[0].selected[0] == 0);
    CHECK(imp.selections[0].selected[1] == 2);
    CHECK(logs.last_level == ARX_LOG_INFO);
    CHECK(logs.last_msg.find("texture containers") != std::string::npos);
    CHECK(logs.last_msg.find("0 bones") != std::string::npos);
    CHECK(logs.last_msg.find("0 action points") != std::string::npos);
    CHECK(logs.last_msg.find("1 selection VEC4 attributes") != std::string::npos);
    CHECK(logs.last_msg.find("0 animations") != std::string::npos);
  }

  TEST_CASE("ImportAppliesExtrasNamesAutomatically") {
    auto d = makeTriData();
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Selection sel{};
    std::memcpy(sel.name, "cut_torso", 10);
    sel.selected = {0, 1};
    d.selections.push_back(sel);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.selections.size() == 1);
    CHECK(std::string(imp.selections[0].name) == "cut_torso");
    REQUIRE(imp.selections[0].selected.size() == 2);
    CHECK(imp.selections[0].selected[0] == 0);
    CHECK(imp.selections[0].selected[1] == 1);
  }

  TEST_CASE("ImportPreservesSelectionOrder") {
    auto d = makeTriData();
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Selection late{};
    std::memcpy(late.name, "late", 5);
    late.selected = {2};
    d.selections.push_back(late);

    pistoris::ftl::Selection early{};
    std::memcpy(early.name, "early", 6);
    early.selected = {0};
    d.selections.push_back(early);

    pistoris::ftl::Selection middle{};
    std::memcpy(middle.name, "middle", 7);
    middle.selected = {1};
    d.selections.push_back(middle);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.selections.size() == 3);
    CHECK(std::string(imp.selections[0].name) == "late");
    CHECK(std::string(imp.selections[1].name) == "early");
    CHECK(std::string(imp.selections[2].name) == "middle");
  }

  TEST_CASE("ImportGuessesSyntheticSelectionAffiliations") {
    auto d = makeTriData();
    d.faces.push_back(makeFace(0, 1, 2));

    pistoris::ftl::Group group{};
    std::memcpy(group.name, "chest", 6);
    group.origin  = 0;
    group.indices = {0, 1, 2};
    d.groups.push_back(group);

    pistoris::ftl::Action action{};
    std::memcpy(action.name, "hook", 5);
    action.vertex_idx = 1;
    d.actions.push_back(action);

    pistoris::ftl::Selection all{};
    std::memcpy(all.name, "all", 4);
    all.selected = {0, 1, 2};
    d.selections.push_back(all);

    pistoris::ftl::Selection weak{};
    std::memcpy(weak.name, "weak", 5);
    weak.selected = {0, 1};
    d.selections.push_back(weak);

    pistoris::ftl::Selection leggings{};
    std::memcpy(leggings.name, "leggings", 9);
    leggings.selected = {0};
    d.selections.push_back(leggings);

    std::vector<uint8_t> glb;
    REQUIRE(pistoris::exportFtlTeaToGlb(d, {}, glb) == ARX_OK);

    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    GlbImportLogCapture logs;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    auto find_selection = [&](std::string_view name) -> const pistoris::ftl::Selection* {
      for (const auto& sel : imp.selections)
        if (std::string_view(sel.name) == name) return &sel;
      return nullptr;
    };
    auto contains = [](const pistoris::ftl::Selection& sel, int32_t idx) {
      return std::find(sel.selected.begin(), sel.selected.end(), idx) != sel.selected.end();
    };

    REQUIRE(imp.groups.size() == 1);
    REQUIRE(imp.actions.size() == 1);
    int32_t group_origin  = static_cast<int32_t>(imp.groups[0].origin);
    int32_t action_vertex = imp.actions[0].vertex_idx;
    int32_t header_origin = static_cast<int32_t>(imp.header.origin);

    const auto* all_back = find_selection("all");
    REQUIRE(all_back != nullptr);
    CHECK(contains(*all_back, group_origin));
    CHECK(contains(*all_back, action_vertex));

    const auto* weak_back = find_selection("weak");
    REQUIRE(weak_back != nullptr);
    CHECK(contains(*weak_back, group_origin));
    CHECK(contains(*weak_back, action_vertex));

    const auto* leggings_back = find_selection("leggings");
    REQUIRE(leggings_back != nullptr);
    CHECK(contains(*leggings_back, header_origin));
    CHECK_FALSE(contains(*leggings_back, group_origin));

    bool weak_warning = false;
    bool unresolved_warning = false;
    for (const auto& msg : logs.messages) {
      if (msg.find("selection guesses were applied with low support") != std::string::npos &&
          msg.find("weak (66.7%)") != std::string::npos)
        weak_warning = true;
      if (msg.find("synthetic origin/action vertices are not in any selection") != std::string::npos)
        unresolved_warning = true;
    }
    CHECK(weak_warning);
    CHECK_FALSE(unresolved_warning);
  }

  TEST_CASE("ImportDecodesKeyAsFallbackName") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    for (float r : {0.4f, 0.6f, 0.7f}) {
      appendF32(bin, r);
      appendF32(bin, r);
      appendF32(bin, r);
      appendF32(bin, 1.0f);
    }

    std::string json = R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}],
      "meshes": [{"primitives": [{
        "attributes": {"POSITION": 0, "_X": 1},
        "mode": 4
      }]}],
      "buffers": [{"byteLength": 84}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0,  "byteLength": 36, "target": 34962},
        {"buffer": 0, "byteOffset": 36, "byteLength": 48, "target": 34962}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [0,0,0], "max": [1,1,0]},
        {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC4"}
      ]
    })";

    auto glb = assembleGlb(json, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.selections.size() == 1);
    CHECK(std::string(imp.selections[0].name) == "x");
    REQUIRE(imp.selections[0].selected.size() == 2);
    CHECK(imp.selections[0].selected[0] == 1);
    CHECK(imp.selections[0].selected[1] == 2);
  }

  TEST_CASE("ImportDecodesColorN") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    for (float r : {1.0f, 0.0f, 1.0f}) {
      appendF32(bin, r);
      appendF32(bin, r);
      appendF32(bin, r);
      appendF32(bin, 1.0f);
    }

    std::string json = R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}],
      "meshes": [{"primitives": [{
        "attributes": {"POSITION": 0, "COLOR_0": 1},
        "mode": 4
      }]}],
      "buffers": [{"byteLength": 84}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0,  "byteLength": 36, "target": 34962},
        {"buffer": 0, "byteOffset": 36, "byteLength": 48, "target": 34962}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [0,0,0], "max": [1,1,0]},
        {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC4"}
      ]
    })";

    auto glb = assembleGlb(json, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.selections.size() == 1);
    CHECK(std::string(imp.selections[0].name) == "color_0");
    REQUIRE(imp.selections[0].selected.size() == 2);
    CHECK(imp.selections[0].selected[0] == 0);
    CHECK(imp.selections[0].selected[1] == 2);
  }

  TEST_CASE("ImportAcceptsNormalizedUshortColor") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    auto q = [](float f) -> uint16_t { return static_cast<uint16_t>(f * 65535.0f); };
    for (float r : {0.4f, 0.6f, 0.7f}) {
      appendU16(bin, q(r));
      appendU16(bin, q(r));
      appendU16(bin, q(r));
      appendU16(bin, q(1.0f));
    }
    std::string json = R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}],
      "meshes": [{"primitives": [{
        "attributes": {"POSITION": 0, "COLOR_0": 1},
        "mode": 4
      }]}],
      "buffers": [{"byteLength": 60}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0,  "byteLength": 36, "target": 34962},
        {"buffer": 0, "byteOffset": 36, "byteLength": 24, "target": 34962}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [0,0,0], "max": [1,1,0]},
        {"bufferView": 1, "componentType": 5123, "count": 3, "type": "VEC4", "normalized": true}
      ]
    })";

    auto glb = assembleGlb(json, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);

    REQUIRE(imp.selections.size() == 1);
    CHECK(std::string(imp.selections[0].name) == "color_0");
    REQUIRE(imp.selections[0].selected.size() == 2);
    CHECK(imp.selections[0].selected[0] == 1);
    CHECK(imp.selections[0].selected[1] == 2);
  }

  TEST_CASE("ImportExcludesWeights0") {
    std::vector<uint8_t> bin;
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);
    appendF32(bin, 0.0f);

    appendF32(bin, 0.0f);
    appendF32(bin, 1.0f);
    appendF32(bin, 0.0f);

    for (int i = 0; i < 3; ++i) {
      appendF32(bin, 1.0f);
      appendF32(bin, 0.0f);
      appendF32(bin, 0.0f);
      appendF32(bin, 0.0f);
    }

    std::string json = R"({
      "asset": {"version": "2.0"},
      "scene": 0,
      "scenes": [{"nodes": [0]}],
      "nodes": [{"mesh": 0}],
      "meshes": [{"primitives": [{
        "attributes": {"POSITION": 0, "WEIGHTS_0": 1},
        "mode": 4
      }]}],
      "buffers": [{"byteLength": 84}],
      "bufferViews": [
        {"buffer": 0, "byteOffset": 0,  "byteLength": 36, "target": 34962},
        {"buffer": 0, "byteOffset": 36, "byteLength": 48, "target": 34962}
      ],
      "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [0,0,0], "max": [1,1,0]},
        {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC4"}
      ]
    })";

    auto glb = assembleGlb(json, bin);
    pistoris::ftl::Data imp;
    std::vector<pistoris::tea::Data> teas;
    REQUIRE(pistoris::importGlbToFtlTea(std::span(glb), "", imp, teas) == ARX_OK);
    CHECK(imp.selections.empty());
  }
}
