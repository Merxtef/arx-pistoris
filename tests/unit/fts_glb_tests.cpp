// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/pistoris.hpp"

#include "arx/conversion/level/api.h"
#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/level/api.h"
#include "external/glb/level/palette.h"
#include "external/glb/writer.h"
#include "helpers.h"
#include "image_helpers.h"
#include "level/data.h"
#include "level/level.h"
#include "level_add_helpers.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "nlohmann/json.hpp"
#include "stb/stb_image_write.h"
#include "utils/log.h"
#include "utils/math/geometry.h"
#include "utils/math/quat.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

pistoris::fts::Data makeTriangleFtsScene() {
  pistoris::fts::Data d = makeMinimalFtsData();
  d.scene.num_rooms = 1;
  d.rooms.resize(2);
  d.room_distances.resize(4);
  auto& texture = d.textures[24275104];
  std::snprintf(texture.fic, sizeof(texture.fic), "graph/levels/test.bmp");

  pistoris::fts::Poly poly{};
  poly.tex = 24275104;
  poly.room = 1;
  poly.type = pistoris::kFaceBitStone;
  poly.v[0].ssx = 0.0f;
  poly.v[0].sy = 0.0f;
  poly.v[0].ssz = 0.0f;
  poly.v[1].ssx = 1.0f;
  poly.v[1].sy = 0.0f;
  poly.v[1].ssz = 0.0f;
  poly.v[2].ssx = 0.0f;
  poly.v[2].sy = 0.0f;
  poly.v[2].ssz = 1.0f;
  poly.norm = {0.0f, -1.0f, 0.0f};
  poly.norm2 = poly.norm;
  poly.nrml[0] = poly.nrml[1] = poly.nrml[2] = poly.norm;
  poly.area = 0.5f;
  d.cells[0].polygons.push_back(poly);

  d.scene.num_textures = 1;
  d.scene.num_polys = 1;
  d.rooms[1].data.num_polys = 1;
  d.rooms[1].polygons.push_back({0, 0, 0, 0});
  return d;
}

pistoris::fts::Data makeTwoRoomFtsScene() {
  pistoris::fts::Data d = makeTriangleFtsScene();
  d.scene.num_rooms = 2;
  d.rooms.resize(3);
  d.room_distances.resize(9);

  pistoris::fts::Poly poly = d.cells[0].polygons[0];
  poly.room = 2;
  poly.v[0].ssx = 2.0f;
  poly.v[0].ssz = 2.0f;
  poly.v[1].ssx = 3.0f;
  poly.v[1].ssz = 2.0f;
  poly.v[2].ssx = 2.0f;
  poly.v[2].ssz = 3.0f;
  d.cells[0].polygons.push_back(poly);
  d.scene.num_polys = 2;
  d.rooms[2].data.num_polys = 1;
  d.rooms[2].polygons.push_back({0, 0, 1, 0});
  return d;
}

pistoris::fts::Data makeQuadFtsScene() {
  pistoris::fts::Data d = makeMinimalFtsData();
  d.scene.num_rooms = 1;
  d.rooms.resize(2);
  d.room_distances.resize(4);

  pistoris::fts::Poly poly{};
  poly.room = 1;
  poly.tex = 0;
  poly.type = pistoris::kFaceBitQuad;
  poly.v[0].ssx = 0.0f;
  poly.v[0].sy = 0.0f;
  poly.v[0].ssz = 0.0f;
  poly.v[1].ssx = 1.0f;
  poly.v[1].sy = 0.0f;
  poly.v[1].ssz = 0.0f;
  poly.v[2].ssx = 0.0f;
  poly.v[2].sy = 0.0f;
  poly.v[2].ssz = 1.0f;
  poly.v[3].ssx = 1.0f;
  poly.v[3].sy = 0.0f;
  poly.v[3].ssz = 1.0f;
  poly.norm = {0.0f, -1.0f, 0.0f};
  poly.norm2 = poly.norm;
  poly.nrml[0] = poly.nrml[1] = poly.nrml[2] = poly.nrml[3] = poly.norm;
  poly.area = 1.0f;
  d.cells[0].polygons.push_back(poly);

  d.scene.num_polys = 1;
  d.rooms[1].data.num_polys = 1;
  d.rooms[1].polygons.push_back({0, 0, 0, 0});
  return d;
}

pistoris::fts::Poly makeFlatFtsTriangle(const std::array<pistoris::ArxVector3, 3>& positions, std::int16_t room = 1) {
  pistoris::fts::Poly poly{};
  poly.room = room;
  for (std::size_t i = 0; i < positions.size(); ++i) {
    poly.v[i].ssx = positions[i].x;
    poly.v[i].sy = positions[i].y;
    poly.v[i].ssz = positions[i].z;
    poly.nrml[i] = {0.0f, -1.0f, 0.0f};
  }
  poly.norm = {0.0f, -1.0f, 0.0f};
  poly.norm2 = poly.norm;
  return poly;
}

pistoris::fts::Data makePortalWeldFtsScene(float original_x, float candidate_x) {
  pistoris::fts::Data data = makeMinimalFtsData();
  data.scene.num_rooms = 2;
  data.rooms.resize(3);
  data.room_distances.resize(9);
  data.cells[0].polygons = {
      makeFlatFtsTriangle({{{original_x, 0.0f, 1.0f}, {10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 2.0f}}}),
      makeFlatFtsTriangle({{{candidate_x, 0.0f, 1.0f}, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 2.0f}}}),
  };
  data.scene.num_polys = 2;

  pistoris::fts::Portal portal{};
  portal.room_1 = 1;
  portal.room_2 = 2;
  portal.useportal = 1;
  portal.poly.v[0].pos = {1.0f, -10.0f, 0.0f};
  portal.poly.v[1].pos = {1.0f, 10.0f, 0.0f};
  portal.poly.v[2].pos = {1.0f, 0.0f, 2.0f};
  data.portals.push_back(portal);
  data.scene.num_portals = 1;
  return data;
}

void makeAuthorableRoom(pistoris::fts::Data& data) {
  data.scene.num_rooms = 1;
  data.rooms.resize(2);
  data.room_distances.resize(4);
}

void addDefaultRoom(pistoris::LevelModules& level) { level.rooms.definitions.push_back({"room"}); }

void addPortalRooms(pistoris::LevelModules& level) { level.rooms.definitions = {{"room_1"}, {"room_2"}}; }

void setUsablePlayerSpawn(pistoris::LevelModules& level, pistoris::PlayerSpawn player_spawn) {
  REQUIRE(pistoris::scene::setPlayerSpawn(level.scene, player_spawn) == pistoris::scene::Error::kNone);
}

void connectPortal(pistoris::Portal& portal, std::string name) {
  portal.name = std::move(name);
  portal.room_1 = 0;
  portal.room_2 = 1;
}

pistoris::Portal makeLevelPortal(std::string name, float x) {
  pistoris::Portal portal;
  connectPortal(portal, std::move(name));
  portal.shape = pistoris::PortalShape::kTriangle;
  portal.vertices = {{{x, 0.0f, 0.0f}, {x, 1.0f, 0.0f}, {x, 0.0f, 1.0f}, {}}};
  return portal;
}

void addFtsPortal(pistoris::fts::Data& data, float x) {
  pistoris::fts::Portal portal;
  portal.room_1 = 1;
  portal.room_2 = 2;
  portal.useportal = 1;
  portal.poly.v[0].pos = {x, 0.0f, 0.0f};
  portal.poly.v[1].pos = {x, 1.0f, 0.0f};
  portal.poly.v[2].pos = {x, 0.0f, 1.0f};
  data.portals.push_back(portal);
  data.scene.num_portals = static_cast<std::int32_t>(data.portals.size());
}

const pistoris::ArxColor3& bakedColor(const pistoris::LevelModules& level, std::size_t face, std::size_t corner) {
  return level.lighting.corner_colors[face * 3U + corner];
}

ArxReturnCode buildLevelModules(pistoris::LevelModules& out, const pistoris::fts::Data& fts,
                                const pistoris::llf::Data* llf = nullptr, const pistoris::dlf::Data* dlf = nullptr) {
  return pistoris::arx_level_conversion::buildLevel({fts, llf, dlf}, out);
}

ArxReturnCode buildAndWeldLevel(pistoris::Level& out, const pistoris::fts::Data& fts,
                                const pistoris::Level::VertexWeldOptions& options = {}) {
  ArxReturnCode rc = pistoris::Level::fromNative(out, fts);
  return rc == ARX_OK ? out.weldVertices(options) : rc;
}

ArxReturnCode validateModules(const pistoris::LevelModules& level, pistoris::ArxAabb* bounds = nullptr,
                              pistoris::ArxAabb* referenced_bounds = nullptr) {
  return pistoris::validateLevelModules(level, bounds, referenced_bounds);
}

ArxReturnCode exportLevelGlb(const pistoris::LevelModules& level, std::vector<std::uint8_t>& out) {
  pistoris::ArxAabb referenced_bounds;
  ArxReturnCode rc = validateModules(level, nullptr, &referenced_bounds);
  if (rc != ARX_OK) return rc;
  pistoris::Level::GlbExportOptions options;
  options.arx_units_per_glb_unit = 1.0f;
  return pistoris::exportLevelToGlb(level, referenced_bounds, options, out);
}

ArxReturnCode exportLevelGlbWithOptions(const pistoris::LevelModules& level,
                                        const pistoris::Level::GlbExportOptions& options,
                                        std::vector<std::uint8_t>& out) {
  pistoris::ArxAabb referenced_bounds;
  ArxReturnCode rc = validateModules(level, nullptr, &referenced_bounds);
  if (rc != ARX_OK) return rc;
  return pistoris::exportLevelToGlb(level, referenced_bounds, options, out);
}

ArxReturnCode importLevelGlb(std::span<const std::uint8_t> glb, pistoris::LevelModules& out) {
  pistoris::LevelModules tmp;
  pistoris::Level::GlbImportOptions options;
  options.arx_units_per_glb_unit = 1.0f;
  options.arx_offset = pistoris::ArxVector3{};
  ArxReturnCode rc = pistoris::importLevelFromGlb(glb, tmp, options);
  if (rc != ARX_OK) return rc;
  rc = validateModules(tmp);
  if (rc == ARX_OK) out = std::move(tmp);
  return rc;
}

ArxReturnCode importLevelGlbWithOptions(std::span<const std::uint8_t> glb,
                                        const pistoris::Level::GlbImportOptions& options, pistoris::LevelModules& out,
                                        pistoris::Level::GlbImportInfo* info = nullptr) {
  pistoris::LevelModules tmp;
  pistoris::Level::GlbImportInfo import_info;
  ArxReturnCode rc = pistoris::importLevelFromGlb(glb, tmp, options, nullptr, &import_info);
  if (rc != ARX_OK) return rc;
  rc = validateModules(tmp);
  if (rc == ARX_OK) {
    out = std::move(tmp);
    if (info) *info = import_info;
  }
  return rc;
}

void addLevelFace(pistoris::LevelModules& level, pistoris::Face face, std::uint32_t room = 0) {
  level.geometry.faces.push_back(face);
  level.rooms.face_rooms.push_back(room);
}

void addSecondRoomTriangle(pistoris::LevelModules& level) {
  std::uint32_t base = static_cast<std::uint32_t>(level.geometry.vertices.size());
  level.geometry.vertices.push_back({{2.0f, 0.0f, 2.0f}});
  level.geometry.vertices.push_back({{3.0f, 0.0f, 2.0f}});
  level.geometry.vertices.push_back({{2.0f, 0.0f, 3.0f}});
  pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
  addLevelFace(level,
               {{{{base + 0, normal, 0.0f, 0.0f}, {base + 1, normal, 1.0f, 0.0f}, {base + 2, normal, 0.0f, 1.0f}}},
                pistoris::kNoTexture,
                0,
                0.0f},
               1);
}

const pistoris::Vertex& faceVertex(const pistoris::LevelModules& d, const pistoris::Face& face, std::size_t corner) {
  return d.geometry.vertices[face.corners[corner].vertex];
}

const pistoris::Corner& faceCorner(const pistoris::Face& face, std::size_t corner) { return face.corners[corner]; }

struct LogCapture {
  std::vector<std::string> messages;

  LogCapture() {
    pistoris::log_fn = [](ArxLogLevel, const char* msg, void* ud) {
      auto* self = static_cast<LogCapture*>(ud);
      self->messages.push_back(msg);
    };
    pistoris::log_ud = this;
  }

  ~LogCapture() {
    pistoris::log_fn = nullptr;
    pistoris::log_ud = nullptr;
  }

  bool contains(std::string_view needle) const {
    for (const auto& msg : messages) {
      if (msg.find(needle) != std::string::npos) return true;
    }
    return false;
  }
};

struct ParsedTestGlb {
  nlohmann::json gltf;
  std::vector<std::uint8_t> bin;
};

std::uint32_t testU32(std::span<const std::uint8_t> data, std::size_t offset) {
  std::uint32_t value = 0;
  std::memcpy(&value, data.data() + offset, sizeof(value));
  return value;
}

ParsedTestGlb parseTestGlb(const std::vector<std::uint8_t>& glb) {
  REQUIRE(glb.size() >= 28);
  std::uint32_t json_size = testU32(glb, 12);
  REQUIRE(20 + json_size + 8 <= glb.size());
  ParsedTestGlb parsed;
  parsed.gltf = nlohmann::json::parse(std::string_view(reinterpret_cast<const char*>(glb.data() + 20), json_size));
  std::size_t bin_header = 20 + json_size;
  std::uint32_t bin_size = testU32(glb, bin_header);
  REQUIRE(bin_header + 8 + bin_size <= glb.size());
  parsed.bin.assign(glb.begin() + static_cast<std::ptrdiff_t>(bin_header + 8),
                    glb.begin() + static_cast<std::ptrdiff_t>(bin_header + 8 + bin_size));
  return parsed;
}

float testMaterialAlpha(const nlohmann::json& material) {
  if (!material.contains("pbrMetallicRoughness")) return 1.0f;
  const auto& pbr = material.at("pbrMetallicRoughness");
  if (!pbr.contains("baseColorFactor")) return 1.0f;
  return pbr.at("baseColorFactor")[3].get<float>();
}

const nlohmann::json& testMaterial(const ParsedTestGlb& parsed, std::string_view name) {
  const auto& materials = parsed.gltf.at("materials");
  auto material = std::find_if(materials.begin(), materials.end(), [&](const nlohmann::json& candidate) {
    return candidate.value("name", std::string{}) == name;
  });
  REQUIRE(material != materials.end());
  return *material;
}

void checkTestMaterial(const ParsedTestGlb& parsed, std::string_view name, const std::array<float, 4>& color,
                       std::string_view alpha_mode, bool double_sided) {
  const nlohmann::json& material = testMaterial(parsed, name);
  const auto& actual_color = material.at("pbrMetallicRoughness").at("baseColorFactor");
  REQUIRE(actual_color.size() == color.size());
  for (std::size_t i = 0; i < color.size(); ++i) CHECK(actual_color[i].get<float>() == doctest::Approx(color[i]));
  CHECK(material.value("alphaMode", std::string("OPAQUE")) == alpha_mode);
  CHECK(material.value("doubleSided", false) == double_sided);
}

std::span<const std::uint8_t> testBufferView(const ParsedTestGlb& parsed, std::size_t index) {
  const auto& view = parsed.gltf["bufferViews"][index];
  std::size_t offset = view.value("byteOffset", 0U);
  std::size_t size = view.at("byteLength").get<std::size_t>();
  REQUIRE(offset <= parsed.bin.size());
  REQUIRE(size <= parsed.bin.size() - offset);
  return std::span<const std::uint8_t>(parsed.bin).subspan(offset, size);
}

std::string testBase64(std::span<const std::uint8_t> bytes) {
  constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((bytes.size() + 2U) / 3U * 4U);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    std::uint32_t value = static_cast<std::uint32_t>(bytes[i]) << 16;
    if (i + 1 < bytes.size()) value |= static_cast<std::uint32_t>(bytes[i + 1]) << 8;
    if (i + 2 < bytes.size()) value |= bytes[i + 2];
    out.push_back(kAlphabet[(value >> 18) & 63U]);
    out.push_back(kAlphabet[(value >> 12) & 63U]);
    out.push_back(i + 1 < bytes.size() ? kAlphabet[(value >> 6) & 63U] : '=');
    out.push_back(i + 2 < bytes.size() ? kAlphabet[value & 63U] : '=');
  }
  return out;
}

std::string testPercentEncoding(std::span<const std::uint8_t> bytes) {
  constexpr std::string_view kHex = "0123456789ABCDEF";
  std::string out;
  out.reserve(bytes.size() * 3U);
  for (std::uint8_t byte : bytes) {
    out.push_back('%');
    out.push_back(kHex[byte >> 4U]);
    out.push_back(kHex[byte & 0x0fU]);
  }
  return out;
}

void appendTestImage(void* context, void* data, int size) {
  auto& out = *static_cast<std::vector<std::uint8_t>*>(context);
  const auto* first = static_cast<const std::uint8_t*>(data);
  out.insert(out.end(), first, first + size);
}

std::vector<std::uint8_t> makeTestRgbaPng(std::uint8_t alpha) {
  const std::array<std::uint8_t, 4> pixel = {255, 0, 0, alpha};
  std::vector<std::uint8_t> encoded;
  if (stbi_write_png_to_func(appendTestImage, &encoded, 1, 1, 4, pixel.data(), 4) == 0) return {};
  return encoded;
}

void appendTestU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(value));
}

template <class T>
void appendTestPod(std::vector<std::uint8_t>& out, const T& value) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
  out.insert(out.end(), bytes, bytes + sizeof(value));
}

std::vector<std::uint8_t> writeTestGlb(ParsedTestGlb parsed) {
  parsed.gltf["buffers"][0]["byteLength"] = parsed.bin.size();
  std::string json = parsed.gltf.dump();
  while (json.size() % 4 != 0) json.push_back(' ');
  while (parsed.bin.size() % 4 != 0) parsed.bin.push_back(0);

  std::vector<std::uint8_t> glb;
  appendTestU32(glb, 0x46546C67);
  appendTestU32(glb, 2);
  appendTestU32(glb, static_cast<std::uint32_t>(12 + 8 + json.size() + 8 + parsed.bin.size()));
  appendTestU32(glb, static_cast<std::uint32_t>(json.size()));
  appendTestU32(glb, 0x4E4F534A);
  glb.insert(glb.end(), json.begin(), json.end());
  appendTestU32(glb, static_cast<std::uint32_t>(parsed.bin.size()));
  appendTestU32(glb, 0x004E4942);
  glb.insert(glb.end(), parsed.bin.begin(), parsed.bin.end());
  return glb;
}

std::vector<float> testFloatAttribute(const ParsedTestGlb& parsed, std::string_view semantic) {
  const auto& primitive = parsed.gltf["meshes"][0]["primitives"][0];
  int accessor_idx = primitive["attributes"][semantic].get<int>();
  const auto& accessor = parsed.gltf["accessors"][accessor_idx];
  const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
  std::size_t count = accessor["count"].get<std::size_t>();
  std::size_t components = accessor["type"] == "VEC4" ? 4U : 3U;
  std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
  std::vector<float> values(count * components);
  std::memcpy(values.data(), parsed.bin.data() + offset, values.size() * sizeof(float));
  return values;
}

std::vector<float> testVec3Accessor(const ParsedTestGlb& parsed, int accessor_index) {
  const auto& accessor = parsed.gltf["accessors"][accessor_index];
  REQUIRE(accessor["type"] == "VEC3");
  const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
  std::size_t count = accessor["count"].get<std::size_t>();
  std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
  std::vector<float> values(count * 3);
  std::memcpy(values.data(), parsed.bin.data() + offset, values.size() * sizeof(float));
  return values;
}

std::size_t testNodeIndex(const ParsedTestGlb& parsed, std::string_view name) {
  const auto& nodes = parsed.gltf["nodes"];
  for (std::size_t i = 0; i < nodes.size(); ++i)
    if (nodes[i].value("name", std::string{}) == name) return i;
  return nodes.size();
}

void reparentTestNode(ParsedTestGlb& parsed, std::size_t node_index, std::size_t parent_index) {
  auto remove_child = [node_index](nlohmann::json& owner, std::string_view member) {
    if (!owner.contains(member) || !owner[member].is_array()) return;
    nlohmann::json& children = owner[member];
    children.erase(std::remove(children.begin(), children.end(), node_index), children.end());
  };
  for (nlohmann::json& node : parsed.gltf["nodes"]) remove_child(node, "children");
  for (nlohmann::json& scene : parsed.gltf["scenes"]) remove_child(scene, "nodes");
  nlohmann::json& parent = parsed.gltf["nodes"][parent_index];
  if (!parent.contains("children")) parent["children"] = nlohmann::json::array();
  parent["children"].push_back(node_index);
}

template <class T>
void replaceTexcoords(ParsedTestGlb& parsed, std::span<const T> values, int component_type, bool normalized) {
  while (parsed.bin.size() % 4 != 0) parsed.bin.push_back(0);
  std::size_t offset = parsed.bin.size();
  for (const T& value : values) appendTestPod(parsed.bin, value);
  int view = static_cast<int>(parsed.gltf["bufferViews"].size());
  parsed.gltf["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", values.size_bytes()}});

  int accessor_idx = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"].get<int>();
  auto& accessor = parsed.gltf["accessors"][accessor_idx];
  accessor["bufferView"] = view;
  accessor["componentType"] = component_type;
  accessor["count"] = values.size() / 2;
  accessor["type"] = "VEC2";
  accessor["normalized"] = normalized;
}

template <class T>
void replaceColors(ParsedTestGlb& parsed, std::span<const T> values, int component_type, bool normalized,
                   std::size_t components = 3) {
  while (parsed.bin.size() % 4 != 0) parsed.bin.push_back(0);
  std::size_t offset = parsed.bin.size();
  for (const T& value : values) appendTestPod(parsed.bin, value);
  int view = static_cast<int>(parsed.gltf["bufferViews"].size());
  parsed.gltf["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", values.size_bytes()}});

  int accessor_idx = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"].get<int>();
  auto& accessor = parsed.gltf["accessors"][accessor_idx];
  accessor["bufferView"] = view;
  accessor["componentType"] = component_type;
  accessor["count"] = values.size() / components;
  accessor["type"] = components == 4 ? "VEC4" : "VEC3";
  accessor["normalized"] = normalized;
}

void replaceFirstAttributeFloat(ParsedTestGlb& parsed, std::string_view semantic, std::size_t component, float value) {
  const auto& primitive = parsed.gltf["meshes"][0]["primitives"][0];
  int accessor_index = primitive["attributes"][semantic].get<int>();
  const auto& accessor = parsed.gltf["accessors"][accessor_index];
  const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
  std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U) + component * sizeof(float);
  REQUIRE(offset + sizeof(float) <= parsed.bin.size());
  std::memcpy(parsed.bin.data() + offset, &value, sizeof(value));
}

std::vector<std::uint32_t> testIndices(const ParsedTestGlb& parsed) {
  const auto& primitive = parsed.gltf["meshes"][0]["primitives"][0];
  const auto& accessor = parsed.gltf["accessors"][primitive["indices"].get<int>()];
  const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
  std::size_t count = accessor["count"].get<std::size_t>();
  std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
  std::vector<std::uint32_t> values(count);
  std::memcpy(values.data(), parsed.bin.data() + offset, values.size() * sizeof(std::uint32_t));
  return values;
}

std::vector<std::uint32_t> testNodeIndices(const ParsedTestGlb& parsed, std::size_t node_index) {
  const int mesh = parsed.gltf["nodes"][node_index]["mesh"].get<int>();
  const auto& primitive = parsed.gltf["meshes"][mesh]["primitives"][0];
  const auto& accessor = parsed.gltf["accessors"][primitive["indices"].get<int>()];
  REQUIRE(accessor["componentType"].get<int>() == 5125);
  const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
  const std::size_t count = accessor["count"].get<std::size_t>();
  const std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
  std::vector<std::uint32_t> values(count);
  REQUIRE(offset + values.size() * sizeof(std::uint32_t) <= parsed.bin.size());
  std::memcpy(values.data(), parsed.bin.data() + offset, values.size() * sizeof(std::uint32_t));
  return values;
}

void splitNodePrimitivePositions(ParsedTestGlb& parsed, std::size_t node_index) {
  int mesh_index = parsed.gltf["nodes"][node_index]["mesh"].get<int>();
  auto& primitive = parsed.gltf["meshes"][mesh_index]["primitives"][0];
  int position_accessor_index = primitive["attributes"]["POSITION"].get<int>();
  const auto& position_accessor = parsed.gltf["accessors"][position_accessor_index];
  const auto& position_view = parsed.gltf["bufferViews"][position_accessor["bufferView"].get<int>()];
  std::size_t position_count = position_accessor["count"].get<std::size_t>();
  std::size_t position_offset = position_view.value("byteOffset", 0U) + position_accessor.value("byteOffset", 0U);
  std::vector<float> positions(position_count * 3);
  std::memcpy(positions.data(), parsed.bin.data() + position_offset, positions.size() * sizeof(float));

  int index_accessor_index = primitive["indices"].get<int>();
  const auto& index_accessor = parsed.gltf["accessors"][index_accessor_index];
  const auto& index_view = parsed.gltf["bufferViews"][index_accessor["bufferView"].get<int>()];
  std::size_t index_count = index_accessor["count"].get<std::size_t>();
  std::size_t index_offset = index_view.value("byteOffset", 0U) + index_accessor.value("byteOffset", 0U);
  std::vector<std::uint32_t> old_indices(index_count);
  const int index_component_type = index_accessor["componentType"].get<int>();
  std::size_t index_component_size = 0;
  if (index_component_type == 5121)
    index_component_size = sizeof(std::uint8_t);
  else if (index_component_type == 5123)
    index_component_size = sizeof(std::uint16_t);
  else if (index_component_type == 5125)
    index_component_size = sizeof(std::uint32_t);
  else
    FAIL("unsupported test index component type");
  REQUIRE(index_offset + index_count * index_component_size <= parsed.bin.size());
  for (std::size_t i = 0; i < index_count; ++i) {
    if (index_component_type == 5121) {
      old_indices[i] = parsed.bin[index_offset + i];
    } else if (index_component_type == 5123) {
      std::uint16_t value = 0;
      std::memcpy(&value, parsed.bin.data() + index_offset + i * sizeof(value), sizeof(value));
      old_indices[i] = value;
    } else {
      std::memcpy(&old_indices[i], parsed.bin.data() + index_offset + i * sizeof(std::uint32_t), sizeof(std::uint32_t));
    }
  }

  std::vector<float> split_positions;
  std::vector<std::uint32_t> split_indices;
  split_positions.reserve(old_indices.size() * 3);
  split_indices.reserve(old_indices.size());
  for (std::uint32_t old_index : old_indices) {
    REQUIRE(old_index < position_count);
    split_indices.push_back(static_cast<std::uint32_t>(split_indices.size()));
    split_positions.push_back(positions[old_index * 3 + 0]);
    split_positions.push_back(positions[old_index * 3 + 1]);
    split_positions.push_back(positions[old_index * 3 + 2]);
  }

  while (parsed.bin.size() % 4 != 0) parsed.bin.push_back(0);
  std::size_t split_position_offset = parsed.bin.size();
  for (float value : split_positions) appendTestPod(parsed.bin, value);
  int split_position_view = static_cast<int>(parsed.gltf["bufferViews"].size());
  parsed.gltf["bufferViews"].push_back(
      {{"buffer", 0}, {"byteOffset", split_position_offset}, {"byteLength", split_positions.size() * sizeof(float)}});
  int split_position_accessor = static_cast<int>(parsed.gltf["accessors"].size());
  parsed.gltf["accessors"].push_back({{"bufferView", split_position_view},
                                      {"componentType", 5126},
                                      {"count", split_positions.size() / 3},
                                      {"type", "VEC3"}});

  while (parsed.bin.size() % 4 != 0) parsed.bin.push_back(0);
  std::size_t split_index_offset = parsed.bin.size();
  for (std::uint32_t value : split_indices) appendTestPod(parsed.bin, value);
  int split_index_view = static_cast<int>(parsed.gltf["bufferViews"].size());
  parsed.gltf["bufferViews"].push_back({{"buffer", 0},
                                        {"byteOffset", split_index_offset},
                                        {"byteLength", split_indices.size() * sizeof(std::uint32_t)}});
  int split_index_accessor = static_cast<int>(parsed.gltf["accessors"].size());
  parsed.gltf["accessors"].push_back(
      {{"bufferView", split_index_view}, {"componentType", 5125}, {"count", split_indices.size()}, {"type", "SCALAR"}});

  primitive["attributes"]["POSITION"] = split_position_accessor;
  primitive["indices"] = split_index_accessor;
}

void checkColor(const std::vector<float>& values, std::size_t vertex, float r, float g, float b) {
  REQUIRE(values.size() >= (vertex + 1) * 4);
  CHECK(values[vertex * 4 + 0] == doctest::Approx(r));
  CHECK(values[vertex * 4 + 1] == doctest::Approx(g));
  CHECK(values[vertex * 4 + 2] == doctest::Approx(b));
  CHECK(values[vertex * 4 + 3] == doctest::Approx(1.0f));
}

pistoris::ArxVector3 normalAtDegrees(float degrees, float scale = 1.0f) {
  float radians = degrees * 3.14159265358979323846f / 180.0f;
  return {std::sin(radians) * scale, -std::cos(radians) * scale, 0.0f};
}

pistoris::LevelModules makeNormalFan(std::span<const pistoris::ArxVector3> normals) {
  pistoris::LevelModules level;
  addDefaultRoom(level);
  level.geometry.textures = {"graph/test.bmp"};
  level.geometry.vertices.push_back({{0.0f, 0.0f, 0.0f}});
  for (std::size_t i = 0; i < normals.size(); ++i) {
    std::uint32_t a = static_cast<std::uint32_t>(level.geometry.vertices.size());
    level.geometry.vertices.push_back({{static_cast<float>(i + 1), 0.0f, 0.0f}});
    std::uint32_t b = static_cast<std::uint32_t>(level.geometry.vertices.size());
    level.geometry.vertices.push_back({{static_cast<float>(i + 1), 0.0f, 1.0f}});
    addLevelFace(
        level, {{{{0, normals[i], 0.0f, 0.0f}, {a, normals[i], 0.0f, 0.0f}, {b, normals[i], 0.0f, 0.0f}}}, 0, 0, 0.0f});
  }
  return level;
}

pistoris::LevelModules makeSimpleLevel() {
  pistoris::LevelModules level;
  addDefaultRoom(level);
  level.geometry.textures = {"graph/test.bmp"};
  level.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
  pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
  addLevelFace(level, {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
  return level;
}

ArxReturnCode exportRenderSplitsDebugGlb(const pistoris::LevelModules& src, float normal_weld_degrees,
                                         std::vector<std::uint8_t>& out) {
  pistoris::Level level;
  for (const pistoris::Room& room : src.rooms.definitions) {
    if (test::addRoom(level, room) == pistoris::kInvalidRoomIndex) return ARX_LEVEL_BAD_ROOM_NAME;
  }

  test::MeshSnapshot mesh;
  mesh.vertices = src.geometry.vertices;
  mesh.faces = src.geometry.faces;
  mesh.textures = src.geometry.textures;
  mesh.face_rooms = src.rooms.face_rooms;
  mesh.corner_colors = src.lighting.corner_colors;
  ArxReturnCode rc = test::replaceMesh(level, mesh);
  if (rc != ARX_OK) return rc;

  return pistoris::level_debug::exportRenderSplitsDebugGlb(level, out, normal_weld_degrees);
}

std::size_t testAttributeCount(const ParsedTestGlb& parsed, std::string_view semantic) {
  int accessor = parsed.gltf["meshes"][0]["primitives"][0]["attributes"][semantic].get<int>();
  return parsed.gltf["accessors"][accessor]["count"].get<std::size_t>();
}

}  // namespace

TEST_SUITE("FtsGlb") {
  TEST_CASE("LevelGlbImportRequiresGeometry") {
    pistoris::glb::Builder builder;
    builder.addRoot(builder.addNode("empty"));
    std::vector<std::uint8_t> glb;
    REQUIRE(builder.write(glb) == ARX_OK);

    pistoris::LevelModules level;
    CHECK(importLevelGlb(glb, level) == ARX_GLB_NO_LEVEL_GEOMETRY);
  }

  TEST_CASE("LevelGlbPaletteCachesCompleteMaterials") {
    pistoris::glb::Builder builder;
    pistoris::glb_level::Palette palette(builder);
    int portal_material = palette.material(pistoris::glb_level::PaletteItem::kPortal);
    CHECK(portal_material == palette.material(pistoris::glb_level::PaletteItem::kPortal));
    CHECK(palette.material(pistoris::glb_level::PaletteItem::kZone) ==
          palette.material(pistoris::glb_level::PaletteItem::kZone));
    CHECK(palette.roomMaterial(0) == palette.roomMaterial(0));

    palette.material(pistoris::glb_level::PaletteItem::kNavigationSurface);
    palette.material(pistoris::glb_level::PaletteItem::kNavigationSupport);
    constexpr std::array<pistoris::glb::Vec3, 3> kPositions = {
        {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
    constexpr std::array<std::uint16_t, 3> kIndices = {0, 1, 2};
    pistoris::glb::Primitive primitive;
    primitive.material = portal_material;
    primitive.indices =
        builder.addAccessor(std::span<const std::uint16_t>(kIndices), cgltf_component_type_r_16u, cgltf_type_scalar);
    primitive.attributes.emplace_back("POSITION", builder.addVec3Accessor(kPositions));
    int mesh = builder.addMesh("test", {std::move(primitive)});
    builder.addRoot(builder.addNode("test", mesh));
    std::vector<std::uint8_t> glb;
    REQUIRE(builder.write(glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    REQUIRE(parsed.gltf.at("materials").size() == 5);
    checkTestMaterial(parsed, "arx_portal", {0.10f, 0.45f, 1.00f, 0.35f}, "BLEND", true);
    checkTestMaterial(parsed, "arx_zone", {0.65f, 0.20f, 0.90f, 0.25f}, "BLEND", true);
    checkTestMaterial(parsed, "arx_nav_surface", {0.00f, 0.65f, 0.85f, 0.40f}, "BLEND", true);
    checkTestMaterial(parsed, "arx_navigation_support", {0.00f, 0.28f, 0.08f, 1.00f}, "OPAQUE", true);
    checkTestMaterial(parsed, "arx_fts_debug_room_0", {0.70f, 0.70f, 0.70f, 1.00f}, "OPAQUE", true);
  }

  TEST_CASE("LevelNativeBundleBakeCreatesMinimalFtsLlfDlf") {
    pistoris::LevelModules level = makeSimpleLevel();
    setUsablePlayerSpawn(
        level, pistoris::PlayerSpawn{{95.0f, -20.0f, 15.0f}, pistoris::math::angleToQuat({1.0f, 2.0f, 3.0f})});
    level.geometry.vertices = {{{90.0f, 0.0f, 10.0f}}, {{110.0f, 0.0f, 10.0f}}, {{90.0f, 0.0f, 30.0f}}};
    level.navigation.anchors.push_back({{95.0f, 0.0f, 15.0f}, 4.0f, -5.0f, pistoris::kAnchorFlagBlocked, {}});
    level.navigation.anchors.push_back({{105.0f, 0.0f, 15.0f}, 6.0f, -7.0f, 0, {}});
    level.navigation.connections.push_back({0, 1});

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    CHECK(bundle.dlf.scene_path == "graph/levels/output");
    CHECK(bundle.fts.scene.sizex == 160);
    CHECK(bundle.fts.scene.sizez == 160);
    CHECK(bundle.fts.scene.num_rooms == 1);
    REQUIRE(bundle.fts.rooms.size() == 2);
    CHECK(bundle.fts.rooms[0].polygons.empty());
    CHECK(bundle.fts.rooms[1].polygons.size() == static_cast<std::size_t>(bundle.fts.scene.num_polys));
    REQUIRE(bundle.fts.room_distances.size() == 4);
    for (const pistoris::fts::RoomDistData& distance : bundle.fts.room_distances)
      CHECK(distance.distance == doctest::Approx(-1.0f));

    const std::size_t cell0 = 0;
    const std::size_t cell1 = 1;
    REQUIRE(bundle.fts.cells[cell0].polygons.size() == 1);
    CHECK((bundle.fts.cells[cell0].polygons[0].type & pistoris::kFaceBitQuad) != 0);
    CHECK(bundle.fts.cells[cell1].polygons.size() == 1);
    CHECK((bundle.fts.cells[cell1].polygons[0].type & pistoris::kFaceBitQuad) == 0);
    CHECK(bundle.fts.scene.num_anchors == 2);
    REQUIRE(bundle.fts.anchors.size() == 2);
    CHECK(bundle.fts.anchors[0].data.pos.x == doctest::Approx(95.0f));
    CHECK(bundle.fts.anchors[0].data.radius == doctest::Approx(4.0f));
    CHECK(bundle.fts.anchors[0].data.height == doctest::Approx(-5.0f));
    CHECK(bundle.fts.anchors[0].data.flags == pistoris::kAnchorFlagBlocked);
    REQUIRE(bundle.fts.anchors[0].linked.size() == 1);
    REQUIRE(bundle.fts.anchors[1].linked.size() == 1);
    CHECK(bundle.fts.anchors[0].linked[0] == 1);
    CHECK(bundle.fts.anchors[1].linked[0] == 0);
    CHECK(bundle.fts.cells[cell0].anchor_ids == std::vector<std::int32_t>{0});
    CHECK(bundle.fts.cells[cell1].anchor_ids == std::vector<std::int32_t>{1});
    CHECK(bundle.fts.scene.num_polys == 2);
    CHECK(bundle.llf.colors.size() == 7);
    CHECK(logs.contains("reconstructed 1 FTS quad(s): 1 clipped-fragment, 0 cross-face; 1 triangle(s) remain"));
    for (const pistoris::ArxColor3& color : bundle.llf.colors) {
      CHECK(color.r == doctest::Approx(0.5f));
      CHECK(color.g == doctest::Approx(0.5f));
      CHECK(color.b == doctest::Approx(0.5f));
    }
    CHECK(bundle.llf.lights.empty());
    CHECK(bundle.dlf.player_spawn.position.x == doctest::Approx(95.0f));
    CHECK(bundle.dlf.player_spawn.angle.yaw == doctest::Approx(2.0f));

    std::vector<std::uint8_t> fts_bytes;
    CHECK(pistoris::writeFts(bundle.fts, fts_bytes) == ARX_OK);
    std::vector<std::uint8_t> llf_bytes;
    CHECK(pistoris::writeLlf(bundle.llf, {}, llf_bytes) == ARX_OK);
    std::vector<std::uint8_t> dlf_bytes;
    CHECK(pistoris::writeDlf(bundle.dlf, {}, dlf_bytes) == ARX_OK);
  }

  TEST_CASE("LevelNativeBundleBakeCanDisableQuadReconstruction") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.vertices = {{{90.0f, 0.0f, 10.0f}}, {{110.0f, 0.0f, 10.0f}}, {{90.0f, 0.0f, 30.0f}}};

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = "", .reconstruct_quads = false}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.cells[0].polygons.size() == 2);
    REQUIRE(bundle.fts.cells[1].polygons.size() == 1);
    for (const pistoris::fts::Cell& cell : bundle.fts.cells)
      for (const pistoris::fts::Poly& poly : cell.polygons) CHECK((poly.type & pistoris::kFaceBitQuad) == 0);
    CHECK(bundle.fts.scene.num_polys == 3);
    CHECK(bundle.llf.colors.size() == 9);
    CHECK(logs.contains("FTS quad reconstruction disabled; emitted 3 triangle(s)"));
  }

  TEST_CASE("LevelNativeBundleBakePacksCompatibleNonplanarFaces") {
    pistoris::LevelModules level;
    addDefaultRoom(level);
    level.geometry.textures = {"graph/test.bmp"};
    level.geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}}, {{10.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 10.0f}}, {{10.0f, -5.0f, 10.0f}}};
    const pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    addLevelFace(level, {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    addLevelFace(level, {{{{3, normal, 1.0f, 1.0f}, {2, normal, 0.0f, 1.0f}, {1, normal, 1.0f, 0.0f}}}, 0, 0, 0.0f});
    level.lighting.corner_colors = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f},
    };

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.cells[0].polygons.size() == 1);
    const pistoris::fts::Poly& poly = bundle.fts.cells[0].polygons[0];
    CHECK((poly.type & pistoris::kFaceBitQuad) != 0);
    CHECK(poly.norm.y == doctest::Approx(-1.0f));
    CHECK(poly.norm2.x != doctest::Approx(poly.norm.x));
    REQUIRE(bundle.llf.colors.size() == 4);
    CHECK(bundle.llf.colors[0].r == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[1].g == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[2].b == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[3].r == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[3].g == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[3].b == doctest::Approx(1.0f));
    CHECK(logs.contains("reconstructed 1 FTS quad(s): 0 clipped-fragment, 1 cross-face; 0 triangle(s) remain"));

    pistoris::LevelModules imported;
    REQUIRE(buildLevelModules(imported, bundle.fts, &bundle.llf, &bundle.dlf) == ARX_OK);
    REQUIRE(imported.geometry.faces.size() == 2);
    REQUIRE(imported.lighting.corner_colors.size() == 6);
    CHECK(bakedColor(imported, 0, 0).r == doctest::Approx(1.0f));
    CHECK(bakedColor(imported, 0, 1).g == doctest::Approx(1.0f));
    CHECK(bakedColor(imported, 0, 2).b == doctest::Approx(1.0f));
    CHECK(bakedColor(imported, 1, 0).r == doctest::Approx(1.0f));
    CHECK(bakedColor(imported, 1, 1).b == doctest::Approx(1.0f));
    CHECK(bakedColor(imported, 1, 2).g == doctest::Approx(1.0f));
  }

  TEST_CASE("LevelNativeBundleBakeKeepsFacesSeparateWhenSharedAttributesDiffer") {
    pistoris::LevelModules level;
    addDefaultRoom(level);
    level.geometry.textures = {"graph/test.bmp"};
    level.geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}}, {{10.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 10.0f}}, {{10.0f, 0.0f, 10.0f}}};
    const pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    addLevelFace(level, {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    addLevelFace(level, {{{{3, normal, 1.0f, 1.0f}, {2, normal, 0.25f, 1.0f}, {1, normal, 1.0f, 0.0f}}}, 0, 0, 0.0f});

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.cells[0].polygons.size() == 2);
    CHECK((bundle.fts.cells[0].polygons[0].type & pistoris::kFaceBitQuad) == 0);
    CHECK((bundle.fts.cells[0].polygons[1].type & pistoris::kFaceBitQuad) == 0);
    CHECK(bundle.llf.colors.size() == 6);
  }

  TEST_CASE("LevelNativeBundleBakeDoesNotPackCoincidentUnsharedEdges") {
    pistoris::LevelModules level;
    addDefaultRoom(level);
    level.geometry.textures = {"graph/test.bmp"};
    level.geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{10.0f, 0.0f, 0.0f}},
        {{0.0f, 0.0f, 10.0f}},
        {{10.0f, 0.0f, 10.0f}},
        {{0.0f, 0.0f, 10.0f}},
        {{10.0f, 0.0f, 0.0f}},
    };
    const pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    addLevelFace(level, {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    addLevelFace(level, {{{{3, normal, 1.0f, 1.0f}, {4, normal, 0.0f, 1.0f}, {5, normal, 1.0f, 0.0f}}}, 0, 0, 0.0f});

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.cells[0].polygons.size() == 2);
    CHECK((bundle.fts.cells[0].polygons[0].type & pistoris::kFaceBitQuad) == 0);
    CHECK((bundle.fts.cells[0].polygons[1].type & pistoris::kFaceBitQuad) == 0);
  }

  TEST_CASE("LevelNativeBundleBakeWritesCompactRoomDistances") {
    pistoris::LevelModules level = makeSimpleLevel();
    setUsablePlayerSpawn(level, pistoris::PlayerSpawn{{95.0f, -20.0f, 15.0f}, {}});
    addPortalRooms(level);
    addSecondRoomTriangle(level);
    level.rooms.portals = {makeLevelPortal("low", 1.0f), makeLevelPortal("high", 4.0f)};
    level.rooms.distances = {{.distance = 123.0f, .low_room_portal = 0, .high_room_portal = 1}};

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.room_distances.size() == 9);
    const pistoris::fts::RoomDistData& forward = bundle.fts.room_distances[1 * 3 + 2];
    const pistoris::fts::RoomDistData& backward = bundle.fts.room_distances[2 * 3 + 1];
    CHECK(forward.distance == doctest::Approx(123.0f));
    CHECK(forward.startpos.x == doctest::Approx(4.0f));
    CHECK(forward.endpos.x == doctest::Approx(1.0f));
    CHECK(backward.distance == doctest::Approx(123.0f));
    CHECK(backward.startpos.x == doctest::Approx(1.0f));
    CHECK(backward.endpos.x == doctest::Approx(4.0f));
  }

  TEST_CASE("LevelNativeBundleBakeWarnsWhenRoomDistancesAreMissing") {
    pistoris::LevelModules level = makeSimpleLevel();
    setUsablePlayerSpawn(level, pistoris::PlayerSpawn{{95.0f, -20.0f, 15.0f}, {}});
    addPortalRooms(level);
    addSecondRoomTriangle(level);

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    CHECK(logs.contains("room-distance data missing or incomplete"));
    CHECK(logs.contains("writing default -1 distances for 1 room pair(s)"));
  }

  TEST_CASE("LevelNativeBundleBakeWarnsWhenRoomDistancesHaveNoPositiveRealPairs") {
    pistoris::LevelModules level = makeSimpleLevel();
    setUsablePlayerSpawn(level, pistoris::PlayerSpawn{{95.0f, -20.0f, 15.0f}, {}});
    level.rooms.definitions = {{"room_1"}, {"room_2"}, {"room_3"}, {"room_4"}};
    level.rooms.distances.assign(6, {});

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    CHECK(logs.contains("room-distance data contains no positive real-room distances"));
    CHECK(logs.contains("writing non-positive fallbacks"));
  }

  TEST_CASE("LevelNativeBundleBakePreservesRoomsAndPortals") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.rooms.definitions.push_back({"room_2"});
    addSecondRoomTriangle(level);
    setUsablePlayerSpawn(level, pistoris::PlayerSpawn{{1.0f, -1.0f, 1.0f}, {}});

    pistoris::Portal portal;
    connectPortal(portal, "door");
    portal.shape = pistoris::PortalShape::kTriangle;
    portal.vertices = {{{1.0f, 0.0f, 0.0f}, {1.0f, -10.0f, 0.0f}, {1.0f, 0.0f, 10.0f}, {}}};
    level.rooms.portals.push_back(portal);

    pistoris::NativeLevelBundle bundle;
    LogCapture logs;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    CHECK(bundle.fts.scene.num_rooms == 2);
    REQUIRE(bundle.fts.rooms.size() == 3);
    CHECK(bundle.fts.rooms[0].polygons.empty());
    CHECK(bundle.fts.rooms[1].polygons.size() == 1);
    CHECK(bundle.fts.rooms[2].polygons.size() == 1);
    CHECK(bundle.fts.cells[0].polygons[0].room == 1);
    CHECK(bundle.fts.cells[0].polygons[1].room == 2);

    REQUIRE(bundle.fts.portals.size() == 1);
    CHECK(bundle.fts.scene.num_portals == 1);
    CHECK(bundle.fts.portals[0].room_1 == 1);
    CHECK(bundle.fts.portals[0].room_2 == 2);
    CHECK(bundle.fts.portals[0].useportal == 1);
    CHECK(bundle.fts.portals[0].poly.v[0].rhw == doctest::Approx(std::sqrt(425.0 / 8.0)));
    REQUIRE(bundle.fts.rooms[1].portal_ids.size() == 1);
    REQUIRE(bundle.fts.rooms[2].portal_ids.size() == 1);
    CHECK(bundle.fts.rooms[1].portal_ids[0] == 0);
    CHECK(bundle.fts.rooms[2].portal_ids[0] == 0);
    CHECK_FALSE(logs.contains("portal(s)"));
  }

  TEST_CASE("LevelNativeBundleBakeWritesDefaultPlayerSpawn") {
    pistoris::LevelModules level = makeSimpleLevel();
    pistoris::NativeLevelBundle bundle;

    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    CHECK(bundle.fts.scene.playerpos.x == doctest::Approx(0.0f));
    CHECK(bundle.dlf.player_spawn.position.x == doctest::Approx(0.0f));
    CHECK(bundle.dlf.player_spawn.angle.yaw == doctest::Approx(0.0f));
  }

  TEST_CASE("LevelNativeBundleBakeRejectsUnrepresentableZoneHeightTransactionally") {
    pistoris::LevelModules level = makeSimpleLevel();
    pistoris::Zone zone;
    zone.name = "too_tall";
    zone.perimeter_xz = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    zone.height = std::numeric_limits<float>::max();
    level.scene.zones.push_back(std::move(zone));

    pistoris::NativeLevelBundle bundle;
    bundle.dlf.scene_path = "unchanged";
    CHECK(pistoris::arx_level_conversion::bakeNativeLevelBundle(
              level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_DLF_BAD_ZONE_HEIGHT);
    CHECK(bundle.dlf.scene_path == "unchanged");
  }

  TEST_CASE("LevelNativeBundleBakeValidatesGeneratedDlfScenePathTransactionally") {
    pistoris::LevelModules level = makeSimpleLevel();
    pistoris::NativeLevelBundle bundle;
    bundle.dlf.scene_path = "unchanged";

    const std::string embedded_nul("bad\0name", 8);
    CHECK(pistoris::arx_level_conversion::bakeNativeLevelBundle(
              level, {.level_name = embedded_nul, .texture_folder = ""}, bundle) == ARX_DLF_BAD_SCENE_PATH);
    CHECK(bundle.dlf.scene_path == "unchanged");

    const std::string long_name(512, 'x');
    CHECK(pistoris::arx_level_conversion::bakeNativeLevelBundle(
              level, {.level_name = long_name, .texture_folder = ""}, bundle) == ARX_DLF_BAD_SCENE_PATH);
    CHECK(bundle.dlf.scene_path == "unchanged");
  }

  TEST_CASE("LevelNativeBundleBakeRoundsFiniteZoneHeight") {
    pistoris::LevelModules level = makeSimpleLevel();
    pistoris::Zone zone;
    zone.name = "rounded";
    zone.perimeter_xz = {{0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f}};
    zone.height = 2.5f;
    level.scene.zones.push_back(std::move(zone));

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    REQUIRE(bundle.dlf.zones.size() == 1);
    CHECK(bundle.dlf.zones[0].height == 3);
  }

  TEST_CASE("LevelNativeBundleBakeReportsDiscardedClippedFragmentsAndFaces") {
    pistoris::LevelModules level = makeSimpleLevel();
    const pistoris::VertexIndex first = static_cast<pistoris::VertexIndex>(level.geometry.vertices.size());
    level.geometry.vertices.push_back({{0.0f, 0.0f, 0.0f}});
    level.geometry.vertices.push_back({{16000.0f, 0.0f, 0.0f}});
    level.geometry.vertices.push_back({{0.0f, 0.0f, 1.0e-8f}});
    pistoris::Face thin = level.geometry.faces.front();
    for (std::size_t i = 0; i < 3; ++i) thin.corners[i].vertex = first + static_cast<std::uint32_t>(i);
    level.geometry.faces.push_back(thin);
    level.rooms.face_rooms.push_back(0);

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    CHECK(logs.contains("clipped polygon fragment(s) with non-finite or minimum area discarded"));
    CHECK(logs.contains("1 source face(s) produced no output polygons after clipping"));
  }

  TEST_CASE("LevelNativeBundleBakeDoesNotEmitFragmentsRejectedByNativeImport") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.vertices = {{{99.999985f, 0.0f, 0.0f}}, {{100.000015f, 0.0f, 0.0f}}, {{100.000015f, 0.0f, 10.0f}}};

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    CHECK(logs.contains("clipped polygon fragment(s) with non-finite or minimum area discarded"));
    REQUIRE(bundle.fts.scene.num_polys > 0);
    for (const pistoris::fts::Cell& cell : bundle.fts.cells) {
      for (const pistoris::fts::Poly& poly : cell.polygons) {
        CHECK_FALSE(pistoris::geometry::degenerateTriangle({poly.v[0].ssx, poly.v[0].sy, poly.v[0].ssz},
                                                           {poly.v[1].ssx, poly.v[1].sy, poly.v[1].ssz},
                                                           {poly.v[2].ssx, poly.v[2].sy, poly.v[2].ssz}));
      }
    }

    pistoris::LevelModules imported;
    CHECK(buildLevelModules(imported, bundle.fts, &bundle.llf, &bundle.dlf) == ARX_OK);
  }

  TEST_CASE("LevelNativeBundleBakeRebasesTextureFilesAndUsesExtensionlessReferences") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.scene.player_spawn = pistoris::PlayerSpawn{{0.0f, 0.0f, 0.0f}, {}};
    level.geometry.textures = {"source/wall.jpg"};
    level.geometry.textures[0].encoded_image = makeTestBmp();

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level,
                {.level_name = "output",
                 .texture_folder = "GRAPH\\OBJ3D\\TEXTURES",
                 .texture_path_mode = pistoris::NativeTexturePathMode::kRebase},
                bundle) == ARX_OK);

    REQUIRE(bundle.fts.textures.contains(1));
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "graph/obj3d/textures/wall");
    REQUIRE(bundle.texture_files.size() == 1);
    CHECK(bundle.texture_files[0].source_texture == 0);
    CHECK(bundle.texture_files[0].resource_path == "graph/obj3d/textures/wall.bmp");
    CHECK(bundle.texture_files[0].encoded_image == makeTestBmp());
  }

  TEST_CASE("LevelNativeBundleBakeCanSkipTextureFilesWithoutDroppingReferences") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"source/my??wall.jpg"};
    level.geometry.textures[0].encoded_image = makeTestBmp();

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = "", .include_texture_files = false}, bundle) ==
            ARX_OK);

    REQUIRE(bundle.fts.textures.contains(1));
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "source/my_wall");
    CHECK(bundle.texture_files.empty());
  }

  TEST_CASE("LevelNativeBundleBakeRescalesNpotTextureFilesAndReportsTheTotal") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"source/wall.bmp", "source/floor.tga", "source/pot.bmp"};
    const std::vector<std::uint8_t> npot = makeTestNpotBmp();
    level.geometry.textures[0].encoded_image = npot;
    level.geometry.textures[1].encoded_image = npot;
    level.geometry.textures[2].encoded_image = makeTestBmp();

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.texture_files.size() == 3);
    CHECK(bundle.texture_files[0].resource_path == "source/wall.png");
    CHECK(bundle.texture_files[1].resource_path == "source/floor.png");
    CHECK(bundle.texture_files[2].resource_path == "source/pot.bmp");
    CHECK(bundle.texture_files[2].encoded_image == makeTestBmp());
    pistoris::geometry::ImageInfo info;
    REQUIRE(pistoris::geometry::inspectImage(bundle.texture_files[0].encoded_image, &info) ==
            pistoris::geometry::ImageError::kNone);
    CHECK(info.format == pistoris::geometry::ImageFormat::kPng);
    CHECK(info.width == 4);
    CHECK(info.height == 2);
    CHECK(level.geometry.textures[0].encoded_image == npot);
    CHECK(level.geometry.textures[1].encoded_image == npot);
    CHECK(logs.contains("rescaled 2 non-power-of-two texture image(s)"));
  }

  TEST_CASE("LevelNativeBundleBakePreservesTextureDirectoriesAndDisambiguatesExtensionlessNames") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"custom/wall.jpg", "custom/wall.png", "custom/wall_1.tga"};
    level.geometry.textures[0].encoded_image = makeTestBmp();
    level.geometry.textures[1].encoded_image = makeTestTga();

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    REQUIRE(bundle.fts.textures.size() == 3);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "custom/wall");
    CHECK(std::string(bundle.fts.textures.at(2).fic) == "custom/wall_2");
    CHECK(std::string(bundle.fts.textures.at(3).fic) == "custom/wall_1");
    REQUIRE(bundle.texture_files.size() == 2);
    CHECK(bundle.texture_files[0].resource_path == "custom/wall.bmp");
    CHECK(bundle.texture_files[1].resource_path == "custom/wall_2.tga");
  }

  TEST_CASE("LevelNativeBundleBakeShardsOversizedRoomTextureBatches") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"custom/wall.bmp", "custom/wall_1.bmp"};
    level.geometry.textures[0].encoded_image = makeTestBmp();
    const pistoris::Face face = level.geometry.faces.front();
    constexpr std::size_t kBaseTriangles = pistoris::kFtsMaxRoomTextureVertices / 3U;
    level.geometry.faces.assign(kBaseTriangles + 1U, face);
    level.rooms.face_rooms.assign(level.geometry.faces.size(), 0);

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = "", .reconstruct_quads = false}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.textures.size() == 3);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "custom/wall");
    CHECK(std::string(bundle.fts.textures.at(2).fic) == "custom/wall_1");
    CHECK(std::string(bundle.fts.textures.at(3).fic) == "custom/wall_2");
    std::size_t base_polygons = 0;
    std::size_t shard_polygons = 0;
    for (const pistoris::fts::EpData& reference : bundle.fts.rooms[1].polygons) {
      const std::size_t cell =
          static_cast<std::size_t>(reference.py) * static_cast<std::size_t>(bundle.fts.scene.sizex) +
          static_cast<std::size_t>(reference.px);
      const pistoris::fts::Poly& polygon = bundle.fts.cells[cell].polygons[static_cast<std::size_t>(reference.idx)];
      if (polygon.tex == 1) ++base_polygons;
      if (polygon.tex == 3) ++shard_polygons;
    }
    CHECK(base_polygons == kBaseTriangles);
    CHECK(shard_polygons == 1);
    REQUIRE(bundle.texture_files.size() == 2);
    CHECK(bundle.texture_files[0].source_texture == 0);
    CHECK(bundle.texture_files[0].resource_path == "custom/wall.bmp");
    CHECK(bundle.texture_files[1].source_texture == 0);
    CHECK(bundle.texture_files[1].resource_path == "custom/wall_2.bmp");
    CHECK(bundle.texture_files[0].encoded_image == bundle.texture_files[1].encoded_image);
    CHECK(logs.contains("texture resource 'custom/wall' was sharded as 'custom/wall_2'"));
    CHECK(logs.contains("provide a copy of the original image under every shard resource name"));

    pistoris::NativeLevelBundle references_only;
    REQUIRE(
        pistoris::arx_level_conversion::bakeNativeLevelBundle(
            level,
            {.level_name = "output", .texture_folder = "", .reconstruct_quads = false, .include_texture_files = false},
            references_only) == ARX_OK);
    CHECK(references_only.texture_files.empty());
    REQUIRE(references_only.fts.textures.size() == 3);
    CHECK(std::string(references_only.fts.textures.at(3).fic) == "custom/wall_2");
  }

  TEST_CASE("LevelNativeBundleBakeSanitizesEmittedTextureFilesBeforeDisambiguation") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"custom/CON.png", "custom/my??tex.png", "custom/my_tex.png", "custom/my_tex_1.png"};
    level.geometry.textures[0].encoded_image = makeTestBmp();
    level.geometry.textures[1].encoded_image = makeTestTga();

    LogCapture logs;
    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    REQUIRE(bundle.fts.textures.size() == 4);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "custom/CON_1");
    CHECK(std::string(bundle.fts.textures.at(2).fic) == "custom/my_tex");
    CHECK(std::string(bundle.fts.textures.at(3).fic) == "custom/my_tex_2");
    CHECK(std::string(bundle.fts.textures.at(4).fic) == "custom/my_tex_1");
    REQUIRE(bundle.texture_files.size() == 2);
    CHECK(bundle.texture_files[0].resource_path == "custom/CON_1.bmp");
    CHECK(bundle.texture_files[1].resource_path == "custom/my_tex.tga");
    CHECK(logs.contains("sanitized 2 native texture path(s)"));
  }

  TEST_CASE("LevelNativeBundleBakeRejectsUnsafeUnresolvedTexturePaths") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"custom/CON.png"};

    pistoris::NativeLevelBundle bundle;
    CHECK(pistoris::arx_level_conversion::bakeNativeLevelBundle(
              level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_FTS_BAD_TEXTURE_PATH);

    level.geometry.textures[0].path = R"(C:\textures\wall.bmp)";
    level.geometry.textures[0].encoded_image = makeTestBmp();
    CHECK(pistoris::arx_level_conversion::bakeNativeLevelBundle(
              level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_FTS_BAD_TEXTURE_PATH);
  }

  TEST_CASE("LevelNativeBundleBakePreservesPortableNativeTexturePunctuation") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"GRAPH/OBJ3D/TEXTURES/L1_[ICE] (ROCK)&WALL.BMP"};

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    REQUIRE(bundle.fts.textures.size() == 1);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "GRAPH/OBJ3D/TEXTURES/L1_[ICE] (ROCK)&WALL");
  }

  TEST_CASE("LevelNativeBundleBakePreservesSquareBracketsInEmittedTextureFiles") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"graph/obj3d/textures/l1_wall_[metal].bmp"};
    level.geometry.textures[0].encoded_image = makeTestBmp();

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    REQUIRE(bundle.fts.textures.size() == 1);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "graph/obj3d/textures/l1_wall_[metal]");
    REQUIRE(bundle.texture_files.size() == 1);
    CHECK(bundle.texture_files[0].resource_path == "graph/obj3d/textures/l1_wall_[metal].bmp");
  }

  TEST_CASE("LevelNativeBundleBakeRestoresKnownGameTexturePathsOnlyAtTheirCanonicalLocation") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.textures = {"graph/obj3d/textures/l4_dwarf_[stone]_wall01.bmp"};

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);
    REQUIRE(bundle.fts.textures.size() == 1);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "graph/obj3d/textures/l4_dwarf_[stone]__wall01");

    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level,
                {.level_name = "output",
                 .texture_folder = "custom/textures",
                 .texture_path_mode = pistoris::NativeTexturePathMode::kRebase},
                bundle) == ARX_OK);
    REQUIRE(bundle.fts.textures.size() == 1);
    CHECK(std::string(bundle.fts.textures.at(1).fic) == "custom/textures/l4_dwarf_[stone]_wall01");
  }

  TEST_CASE("LevelNativeBundleBakePreservesLightingAndDlfModules") {
    pistoris::LevelModules level = makeSimpleLevel();
    setUsablePlayerSpawn(level,
                         pistoris::PlayerSpawn{{1.0f, -2.0f, 3.0f}, pistoris::math::angleToQuat({4.0f, 5.0f, 6.0f})});
    level.lighting.corner_colors = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    level.lighting.lights.push_back({"torch",
                                     {10.0f, -20.0f, 30.0f},
                                     {0.7f, 0.6f, 0.5f},
                                     2.0f,
                                     8.0f,
                                     1.5f,
                                     {0.1f, 0.2f, 0.3f},
                                     4.0f,
                                     5.0f,
                                     6.0f,
                                     7.0f,
                                     8.0f,
                                     pistoris::kLightFlagFlare});
    level.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/timed_lever/timed_lever",
                                    17,
                                    {2.0f, -3.0f, 4.0f},
                                    pistoris::math::angleToQuat({1.0f, 2.0f, 3.0f}),
                                    "timed_lever"});
    level.scene.fogs.push_back({{5.0f, -6.0f, 7.0f},
                                {0.2f, 0.3f, 0.4f},
                                9.0f,
                                true,
                                1.25f,
                                pistoris::math::angleToQuat({3.0f, 4.0f, 5.0f}),
                                6.0f,
                                7.0f,
                                800,
                                0.5f,
                                {}});
    level.scene.zones.push_back({"hall",
                                 {{1.0f, 2.0f}, {3.0f, 2.0f}, {3.0f, 4.0f}},
                                 -12.0f,
                                 pistoris::ZoneHeightMode::kFinite,
                                 20.0f,
                                 pistoris::ArxColor3{0.3f, 0.4f, 0.5f},
                                 900.0f,
                                 pistoris::ZoneAmbiance{"none", 80.0f}});
    level.scene.paths.push_back(
        {"patrol",
         {9.0f, -10.0f, 11.0f},
         {{{}, pistoris::PathNodeType::kStandard, 0}, {{1.0f, 2.0f, 3.0f}, pistoris::PathNodeType::kBezier, 100}}});

    pistoris::NativeLevelBundle bundle;
    LogCapture logs;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.llf.colors.size() == 3);
    CHECK(bundle.llf.colors[0].r == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[1].g == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[2].b == doctest::Approx(1.0f));
    REQUIRE(bundle.llf.lights.size() == 1);
    CHECK(bundle.llf.lights[0].position.x == doctest::Approx(10.0f));
    CHECK(bundle.llf.lights[0].fallstart == doctest::Approx(2.0f));
    CHECK(bundle.llf.lights[0].fallend == doctest::Approx(8.0f));
    CHECK(bundle.llf.lights[0].flags == pistoris::kLightFlagFlare);

    REQUIRE(bundle.dlf.entities.size() == 1);
    CHECK(bundle.dlf.entities[0].ident == 17);
    CHECK(bundle.dlf.entities[0].class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    REQUIRE(bundle.dlf.fogs.size() == 1);
    CHECK(bundle.dlf.fogs[0].directional);
    CHECK(bundle.dlf.fogs[0].lifetime_ms == 800);
    REQUIRE(bundle.dlf.zones.size() == 1);
    CHECK(bundle.dlf.zones[0].position.x == doctest::Approx(0.0f));
    CHECK(bundle.dlf.zones[0].position.y == doctest::Approx(-12.0f));
    CHECK(bundle.dlf.zones[0].height == 20);
    REQUIRE(bundle.dlf.zones[0].points.size() == 3);
    CHECK(bundle.dlf.zones[0].points[1].x == doctest::Approx(3.0f));
    REQUIRE(bundle.dlf.zones[0].ambiance.has_value());
    CHECK(bundle.dlf.zones[0].ambiance->name == "none");
    REQUIRE(bundle.dlf.paths.size() == 1);
    CHECK(bundle.dlf.paths[0].position.x == doctest::Approx(9.0f));
    REQUIRE(bundle.dlf.paths[0].nodes.size() == 2);
    CHECK(bundle.dlf.paths[0].nodes[1].type == pistoris::dlf::PathNodeType::kBezier);
    CHECK(bundle.dlf.paths[0].nodes[1].time_ms == 100);
    CHECK_FALSE(logs.contains("light(s)"));
    CHECK_FALSE(logs.contains("fog(s)"));
    CHECK_FALSE(logs.contains("zone(s)"));
    CHECK_FALSE(logs.contains("path(s)"));
    CHECK_FALSE(logs.contains("entity/entities"));
    CHECK_FALSE(logs.contains("room distance(s)"));
  }

  TEST_CASE("LevelNativeBundleBakeOrdersLlfColorsByFinalFtsCells") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.scene.player_spawn = pistoris::PlayerSpawn{{0.0f, 0.0f, 0.0f}, {}};
    level.geometry.vertices = {{{120.0f, 0.0f, 10.0f}},
                               {{130.0f, 0.0f, 10.0f}},
                               {{120.0f, 0.0f, 20.0f}},
                               {{10.0f, 0.0f, 10.0f}},
                               {{20.0f, 0.0f, 10.0f}},
                               {{10.0f, 0.0f, 20.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    level.geometry.faces.clear();
    level.rooms.face_rooms.clear();
    level.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    level.rooms.face_rooms.push_back(0);
    level.geometry.faces.push_back(
        {{{{3, normal, 0.0f, 0.0f}, {4, normal, 1.0f, 0.0f}, {5, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    level.rooms.face_rooms.push_back(0);
    level.lighting.corner_colors = {{1.0f, 0.0f, 0.0f},
                                    {1.0f, 0.0f, 0.0f},
                                    {1.0f, 0.0f, 0.0f},
                                    {0.0f, 1.0f, 0.0f},
                                    {0.0f, 1.0f, 0.0f},
                                    {0.0f, 1.0f, 0.0f}};

    pistoris::NativeLevelBundle bundle;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    REQUIRE(bundle.fts.cells[0].polygons.size() == 1);
    REQUIRE(bundle.fts.cells[1].polygons.size() == 1);
    REQUIRE(bundle.llf.colors.size() == 6);
    CHECK(bundle.llf.colors[0].g == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[1].g == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[2].g == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[3].r == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[4].r == doctest::Approx(1.0f));
    CHECK(bundle.llf.colors[5].r == doctest::Approx(1.0f));
  }

  TEST_CASE("LevelNativeBundleBakeKeepsDisconnectedAnchorOutput") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.scene.player_spawn = pistoris::PlayerSpawn{{0.0f, 0.0f, 0.0f}, {}};
    level.navigation.anchors.push_back({{0.1f, 0.0f, 0.1f}, 1.0f, -1.0f, 0, {}});
    level.navigation.anchors.push_back({{0.2f, 0.0f, 0.1f}, 1.0f, -1.0f, 0, {}});
    level.navigation.anchors.push_back({{0.3f, 0.0f, 0.1f}, 1.0f, -1.0f, 0, {}});
    level.navigation.connections.push_back({0, 1});

    pistoris::NativeLevelBundle bundle;
    LogCapture logs;
    REQUIRE(pistoris::arx_level_conversion::bakeNativeLevelBundle(
                level, {.level_name = "output", .texture_folder = ""}, bundle) == ARX_OK);

    CHECK(!logs.contains("pruned"));
    REQUIRE(bundle.fts.anchors.size() == 3);
    REQUIRE(bundle.fts.anchors[0].linked.size() == 1);
    REQUIRE(bundle.fts.anchors[1].linked.size() == 1);
    CHECK(bundle.fts.anchors[2].linked.empty());
    CHECK(bundle.fts.anchors[0].linked[0] == 1);
    CHECK(bundle.fts.anchors[1].linked[0] == 0);
    CHECK(bundle.fts.cells[0].anchor_ids == std::vector<std::int32_t>{0, 1, 2});
    CHECK(level.navigation.anchors.size() == 3);
  }

  TEST_CASE("FtsToLevelFlattensQuadToConnectedTriangles") {
    pistoris::fts::Data src = makeQuadFtsScene();

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    CHECK(level.geometry.vertices.size() == 4);
    REQUIRE(level.geometry.faces.size() == 2);
    CHECK((level.geometry.faces[0].flags & pistoris::kFaceBitQuad) == 0);
    CHECK((level.geometry.faces[1].flags & pistoris::kFaceBitQuad) == 0);

    CHECK(faceVertex(level, level.geometry.faces[0], 0).position.x == 0.0f);
    CHECK(faceVertex(level, level.geometry.faces[0], 1).position.x == 1.0f);
    CHECK(faceVertex(level, level.geometry.faces[0], 2).position.z == 1.0f);
    CHECK(faceVertex(level, level.geometry.faces[1], 0).position.x == 1.0f);
    CHECK(faceVertex(level, level.geometry.faces[1], 0).position.z == 1.0f);
    CHECK(faceVertex(level, level.geometry.faces[1], 1).position.x == 0.0f);
    CHECK(faceVertex(level, level.geometry.faces[1], 1).position.z == 1.0f);
    CHECK(faceVertex(level, level.geometry.faces[1], 2).position.x == 1.0f);
    CHECK(faceVertex(level, level.geometry.faces[1], 2).position.z == 0.0f);
  }

  TEST_CASE("NativeLevelUsesDefaultColorsWithoutLlf") {
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, makeTriangleFtsScene()) == ARX_OK);

    REQUIRE(level.geometry.faces.size() == 1);
    CHECK(level.lighting.corner_colors.empty());
    CHECK(level.lighting.lights.empty());
    CHECK(level.scene.player_spawn_is_fallback);
  }

  TEST_CASE("FtsToLevelUsesDedicatedNormalRepairTolerance") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.cells[0].polygons[0].nrml[0] = {0.0f, -1.0e-5f, 0.0f};
    src.cells[0].polygons[0].nrml[1] = {0.0f, -2.0f, 0.0f};

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    CHECK(logs.contains("1 corner normal(s) regenerated"));
    CHECK(logs.contains("1 corner normal(s) normalized"));
    REQUIRE(level.geometry.faces.size() == 1);
    CHECK(level.geometry.faces[0].corners[0].normal.y == doctest::Approx(-1.0f));
    CHECK(level.geometry.faces[0].corners[1].normal.y == doctest::Approx(-1.0f));
  }

  TEST_CASE("NativeLevelMapsLlfColorsThroughQuadCornerOrder") {
    pistoris::fts::Data src = makeQuadFtsScene();
    pistoris::llf::Data llf;
    llf.colors = {
        {0.1f, 0.0f, 0.0f},
        {0.2f, 0.0f, 0.0f},
        {0.3f, 0.0f, 0.0f},
        {0.4f, 0.0f, 0.0f},
    };

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, &llf) == ARX_OK);

    REQUIRE(level.geometry.faces.size() == 2);
    REQUIRE(level.lighting.corner_colors.size() == 6);
    CHECK(bakedColor(level, 0, 0).r == doctest::Approx(0.1f));
    CHECK(bakedColor(level, 0, 1).r == doctest::Approx(0.2f));
    CHECK(bakedColor(level, 0, 2).r == doctest::Approx(0.3f));
    CHECK(bakedColor(level, 1, 0).r == doctest::Approx(0.4f));
    CHECK(bakedColor(level, 1, 1).r == doctest::Approx(0.3f));
    CHECK(bakedColor(level, 1, 2).r == doctest::Approx(0.2f));
  }

  TEST_CASE("NativeLevelColorCursorSurvivesDiscardedFaces") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    pistoris::fts::Poly degenerate{};
    degenerate.nrml[0] = degenerate.nrml[1] = degenerate.nrml[2] = {0.0f, -1.0f, 0.0f};

    pistoris::fts::Poly valid = makeTriangleFtsScene().cells[0].polygons[0];
    valid.tex = 0;
    src.cells[0].polygons = {degenerate, valid};
    src.scene.num_polys = 2;

    pistoris::llf::Data llf;
    llf.colors = {
        {0.1f, 0.0f, 0.0f},
        {0.2f, 0.0f, 0.0f},
        {0.3f, 0.0f, 0.0f},
        {0.4f, 0.0f, 0.0f},
        {0.5f, 0.0f, 0.0f},
        {0.6f, 0.0f, 0.0f},
    };

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, &llf) == ARX_OK);

    REQUIRE(level.geometry.faces.size() == 1);
    REQUIRE(level.lighting.corner_colors.size() == 3);
    CHECK(bakedColor(level, 0, 0).r == doctest::Approx(0.4f));
    CHECK(bakedColor(level, 0, 1).r == doctest::Approx(0.5f));
    CHECK(bakedColor(level, 0, 2).r == doctest::Approx(0.6f));
  }

  TEST_CASE("NativeLevelDefaultsMismatchedColorsAndPreservesLights") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::llf::Data llf;
    llf.colors = {{0.1f, 0.2f, 0.3f}};
    llf.lights.push_back({});

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, &llf) == ARX_OK);

    REQUIRE(level.geometry.faces.size() == 1);
    CHECK(level.lighting.corner_colors.empty());
    REQUIRE(level.lighting.lights.size() == 1);
    CHECK(level.lighting.lights[0].name == "light_0");
    CHECK(logs.contains("expected 3, got 1"));
    CHECK(logs.contains("preserved 1 light(s)"));
  }

  TEST_CASE("NativeLevelAppliesMsceneposToLlfLights") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.scene.Mscenepos = {10.0f, 20.0f, 30.0f};
    pistoris::llf::Data llf;
    llf.colors.resize(3, {0.5f, 0.5f, 0.5f});
    llf.lights.push_back({});
    llf.lights[0].position = {1.0f, 2.0f, 3.0f};

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, &llf) == ARX_OK);

    REQUIRE(level.lighting.lights.size() == 1);
    CHECK(level.lighting.lights[0].name == "light_0");
    CHECK(level.lighting.lights[0].position.x == 11.0f);
    CHECK(level.lighting.lights[0].position.y == 22.0f);
    CHECK(level.lighting.lights[0].position.z == 33.0f);
  }

  TEST_CASE("NativeLevelNarrowsEqualPositiveLlfLightFalloff") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::llf::Data llf;
    llf.colors.resize(3, {0.5f, 0.5f, 0.5f});
    llf.lights.push_back({});
    llf.lights[0].fallstart = 10.0f;
    llf.lights[0].fallend = 10.0f;

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, &llf) == ARX_OK);

    REQUIRE(level.lighting.lights.size() == 1);
    CHECK(level.lighting.lights[0].fallstart == doctest::Approx(9.0f));
    CHECK(level.lighting.lights[0].fallend == doctest::Approx(10.0f));
    CHECK(logs.contains("1 equal light falloff range(s) narrowed"));
  }

  TEST_CASE("NativeLevelKeepsZeroLlfLightFalloff") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::llf::Data llf;
    llf.colors.resize(3, {0.5f, 0.5f, 0.5f});
    llf.lights.push_back({});
    llf.lights[0].fallstart = 0.0f;
    llf.lights[0].fallend = 0.0f;

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, &llf) == ARX_OK);

    REQUIRE(level.lighting.lights.size() == 1);
    CHECK(level.lighting.lights[0].fallstart == doctest::Approx(0.0f));
    CHECK(level.lighting.lights[0].fallend == doctest::Approx(0.0f));
    CHECK_FALSE(logs.contains("equal light falloff"));
  }

  TEST_CASE("NativeLevelRejectsInvalidLlfWithoutPublishing") {
    pistoris::llf::Data llf;
    llf.colors = {{2.0f, 0.0f, 0.0f}};

    pistoris::LevelModules level;
    level.geometry.vertices.push_back({{7.0f, 8.0f, 9.0f}});
    CHECK(buildLevelModules(level, makeTriangleFtsScene(), &llf) == ARX_LLF_BAD_BAKED_COLOR);
    REQUIRE(level.geometry.vertices.size() == 1);
    CHECK(level.geometry.vertices[0].position.x == 7.0f);
  }

  TEST_CASE("NativeLevelMapsDlfModulesAndNormalizesZones") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.scene.Mscenepos = {10.0f, 20.0f, 30.0f};

    pistoris::dlf::Data dlf;
    dlf.scene_path = "graph/levels/level1/";
    dlf.player_spawn = {{1.0f, 2.0f, 3.0f}, {10.0f, 20.0f, 30.0f}};
    dlf.entities.push_back(
        {"graph/obj3d/interactive/fix_inter/timed_lever/timed_lever", 7, {4.0f, 5.0f, 6.0f}, {1.0f, 2.0f, 3.0f}});
    for (int i = 0; i < 3; ++i)
      dlf.entities.push_back({"graph/obj3d/interactive/npc/spider_base/spider_base", i, {4.0f, 5.0f, 6.0f}, {}});
    dlf.fogs.push_back({{7.0f, 8.0f, 9.0f}});

    pistoris::dlf::Zone zone;
    zone.name = "hall";
    zone.position = {1.0f, 2.0f, 3.0f};
    zone.points = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 0.0f}};
    zone.height = 5;
    zone.ambiance = pistoris::dlf::ZoneAmbiance{"SFX\\ROOM.AMB", 75.0f};
    dlf.zones.push_back(zone);

    pistoris::dlf::Path path;
    path.name = "guard";
    path.position = {2.0f, 3.0f, 4.0f};
    path.nodes = {{{0.0f, 0.0f, 0.0f}, pistoris::dlf::PathNodeType::kStandard, 0},
                  {{1.0f, 0.0f, 0.0f}, pistoris::dlf::PathNodeType::kBezier, 100}};
    dlf.paths.push_back(path);

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src, nullptr, &dlf) == ARX_OK);

    CHECK(level.scene.player_spawn.position.x == 11.0f);
    CHECK(level.scene.player_spawn.position.y == 22.0f);
    CHECK(level.scene.player_spawn.position.z == 33.0f);
    CHECK_FALSE(level.scene.player_spawn_is_fallback);
    REQUIRE(level.scene.entities.size() == 4);
    CHECK(level.scene.entities[0].ident == 7);
    CHECK(level.scene.entities[0].position.x == 14.0f);
    CHECK(level.scene.entities[0].name == "timed_lever");
    CHECK(level.scene.entities[1].name == "spider");
    CHECK(level.scene.entities[2].name == "spider_1");
    CHECK(level.scene.entities[3].name == "spider_2");
    REQUIRE(level.scene.fogs.size() == 1);
    CHECK(level.scene.fogs[0].position.z == 39.0f);
    REQUIRE(level.scene.zones.size() == 1);
    CHECK(level.scene.zones[0].reference_y == 22.0f);
    CHECK(level.scene.zones[0].perimeter_xz.size() == 4);
    REQUIRE(level.scene.zones[0].ambiance.has_value());
    CHECK(level.scene.zones[0].ambiance->name == "sfx/room");
    REQUIRE(level.scene.paths.size() == 1);
    CHECK(level.scene.paths[0].position.x == 12.0f);
    CHECK(level.scene.paths[0].position.y == 23.0f);
    CHECK(level.scene.paths[0].position.z == 34.0f);
    CHECK(logs.contains("1 consecutive zone point(s) collapsed"));
  }

  TEST_CASE("NativeLevelRepairsDuplicatePathNames") {
    pistoris::dlf::Data dlf;
    dlf.scene_path = "graph/levels/level1/";
    auto add_path = [&](std::string name) {
      pistoris::dlf::Path path;
      path.name = std::move(name);
      path.nodes.push_back({});
      dlf.paths.push_back(std::move(path));
    };
    add_path("patrol_");
    add_path("patrol_1");
    add_path("patrol_");

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, makeTriangleFtsScene(), nullptr, &dlf) == ARX_OK);
    REQUIRE(level.scene.paths.size() == 3);
    CHECK(level.scene.paths[0].name == "patrol_");
    CHECK(level.scene.paths[1].name == "patrol_1");
    CHECK(level.scene.paths[2].name == "patrol_2");
    CHECK(logs.contains("1 duplicate path name(s) renamed"));
  }

  TEST_CASE("NativeLevelPreservesNoneZoneAmbiance") {
    pistoris::dlf::Data dlf;
    dlf.scene_path = "graph/levels/level1/";
    pistoris::dlf::Zone zone;
    zone.name = "silent";
    zone.points = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f}};
    zone.height = 5;
    zone.ambiance = pistoris::dlf::ZoneAmbiance{"NONE", 100.0f};
    dlf.zones.push_back(std::move(zone));

    pistoris::dlf::Zone empty;
    empty.name = "empty";
    empty.position = {4.0f, 0.0f, 0.0f};
    empty.points = {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 2.0f}};
    empty.height = 5;
    empty.ambiance = pistoris::dlf::ZoneAmbiance{"", 100.0f};
    dlf.zones.push_back(std::move(empty));

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, makeTriangleFtsScene(), nullptr, &dlf) == ARX_OK);
    REQUIRE(level.scene.zones.size() == 2);
    REQUIRE(level.scene.zones[0].ambiance.has_value());
    CHECK(level.scene.zones[0].ambiance->name == "none");
    CHECK_FALSE(level.scene.zones[1].ambiance.has_value());
    CHECK(logs.contains("1 empty zone ambiance override(s) ignored"));
  }

  TEST_CASE("NativeLevelMapsTheSewerCompatibilityPathToAnInfiniteZone") {
    pistoris::dlf::Data dlf;
    dlf.scene_path = "graph/levels/level11/";
    pistoris::dlf::Path path;
    path.name = "level11_sewer1";
    path.nodes = {{{0.0f, 0.0f, 0.0f}, pistoris::dlf::PathNodeType::kStandard, 0},
                  {{1.0f, 0.0f, 0.0f}, pistoris::dlf::PathNodeType::kStandard, 0},
                  {{0.0f, 0.0f, 1.0f}, pistoris::dlf::PathNodeType::kStandard, 0}};
    dlf.paths.push_back(path);

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, makeTriangleFtsScene(), nullptr, &dlf) == ARX_OK);

    CHECK(level.scene.paths.empty());
    REQUIRE(level.scene.zones.size() == 1);
    CHECK(level.scene.zones[0].name == "level11_sewer1");
    CHECK(level.scene.zones[0].height_mode == pistoris::ZoneHeightMode::kInfinite);
  }

  TEST_CASE("FtsToLevelPreservesPerPolygonTopologyAndCornerUvs") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);

    pistoris::fts::Poly a{};
    a.room = 1;
    a.tex = 0;
    a.v[0].ssx = 0.0f;
    a.v[0].ssz = 0.0f;
    a.v[1].ssx = 1.0f;
    a.v[1].ssz = 0.0f;
    a.v[1].stu = 1.0f;
    a.v[2].ssx = 0.0f;
    a.v[2].ssz = 1.0f;
    a.norm = {0.0f, -1.0f, 0.0f};
    a.nrml[0] = a.nrml[1] = a.nrml[2] = a.norm;

    pistoris::fts::Poly b{};
    b.room = 1;
    b.tex = 0;
    b.v[0].ssx = 1.0f;
    b.v[0].ssz = 0.0f;
    b.v[0].stu = 7.0f;
    b.v[1].ssx = 1.0f;
    b.v[1].ssz = 1.0f;
    b.v[2].ssx = 0.0f;
    b.v[2].ssz = 1.0f;
    b.v[2].stu = 8.0f;
    b.norm = {0.0f, -1.0f, 0.0f};
    b.nrml[0] = b.nrml[1] = b.nrml[2] = b.norm;

    src.cells[0].polygons = {a, b};
    src.scene.num_polys = 2;

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.geometry.faces.size() == 2);
    CHECK(level.geometry.vertices.size() == 6);
    CHECK(faceCorner(level.geometry.faces[0], 1).vertex != faceCorner(level.geometry.faces[1], 0).vertex);
    CHECK(faceCorner(level.geometry.faces[0], 1).u == 1.0f);
    CHECK(faceCorner(level.geometry.faces[1], 0).u == 7.0f);
    CHECK(faceCorner(level.geometry.faces[0], 2).vertex != faceCorner(level.geometry.faces[1], 2).vertex);
    CHECK(faceCorner(level.geometry.faces[1], 2).u == 8.0f);
  }

  TEST_CASE("FtsToLevelPreservesNativeQuadTopology") {
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, makeQuadFtsScene()) == ARX_OK);

    REQUIRE(level.geometry.faces.size() == 2);
    CHECK(level.geometry.vertices.size() == 4);
    CHECK(faceCorner(level.geometry.faces[0], 1).vertex == faceCorner(level.geometry.faces[1], 2).vertex);
    CHECK(faceCorner(level.geometry.faces[0], 2).vertex == faceCorner(level.geometry.faces[1], 1).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingDoesNotCrossRooms") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    for (std::size_t i = 0; i < 3; ++i) src.cells[0].polygons[1].v[i] = src.cells[0].polygons[0].v[i];

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    REQUIRE(level.faceCount() == 2);
    CHECK(level.vertexCount() == 6);
    for (std::size_t i = 0; i < 3; ++i)
      CHECK(faceCorner(test::face(level, 0), i).vertex != faceCorner(test::face(level, 1), i).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingPreservesCloseCornersOfOneFace") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.cells[0].polygons[0] =
        makeFlatFtsTriangle({{{10.0f, 0.0f, 10.0f}, {10.00008f, 0.0f, 10.0f}, {10.0f, 0.0f, 12.0f}}});

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    REQUIRE(level.faceCount() == 1);
    CHECK(level.vertexCount() == 3);
    CHECK(faceCorner(test::face(level, 0), 0).vertex != faceCorner(test::face(level, 0), 1).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingKeepsPortalAdjacentVerticesProtected") {
    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, makePortalWeldFtsScene(1.00008f, 1.0f)) == ARX_OK);

    REQUIRE(level.faceCount() == 2);
    const pistoris::VertexIndex first = faceCorner(test::face(level, 0), 0).vertex;
    const pistoris::VertexIndex second = faceCorner(test::face(level, 1), 0).vertex;
    CHECK(first != second);
    CHECK(test::vertex(level, first).position.x == doctest::Approx(1.00008f));
    CHECK(test::vertex(level, second).position.x == doctest::Approx(1.0f));
  }

  TEST_CASE("ExplicitLevelWeldingWeldsOntoNearbyPortalProtectedVertex") {
    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, makePortalWeldFtsScene(1.00018f, 1.00009f)) == ARX_OK);

    REQUIRE(level.faceCount() == 2);
    const pistoris::VertexIndex first = faceCorner(test::face(level, 0), 0).vertex;
    const pistoris::VertexIndex second = faceCorner(test::face(level, 1), 0).vertex;
    CHECK(first == second);
    CHECK(test::vertex(level, first).position.x == doctest::Approx(1.00009f));
  }

  TEST_CASE("ExplicitLevelWeldingJoinsPositionsImportedFromAdjacentCells") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.scene.sizex = 2;
    src.cells.resize(2);

    pistoris::fts::Poly left{};
    left.room = 1;
    left.tex = 0;
    left.v[0].ssx = 90.0f;
    left.v[0].ssz = 0.0f;
    left.v[1].ssx = 100.0f;
    left.v[1].ssz = 0.0f;
    left.v[2].ssx = 90.0f;
    left.v[2].ssz = 10.0f;
    left.norm = {0.0f, -1.0f, 0.0f};
    left.nrml[0] = left.nrml[1] = left.nrml[2] = left.norm;

    pistoris::fts::Poly right{};
    right.room = 1;
    right.tex = 0;
    right.v[0].ssx = 100.0f;
    right.v[0].ssz = 0.0f;
    right.v[1].ssx = 110.0f;
    right.v[1].ssz = 0.0f;
    right.v[2].ssx = 110.0f;
    right.v[2].ssz = 10.0f;
    right.norm = {0.0f, -1.0f, 0.0f};
    right.nrml[0] = right.nrml[1] = right.nrml[2] = right.norm;

    src.cells[0].polygons.push_back(left);
    src.cells[1].polygons.push_back(right);
    src.scene.num_polys = 2;
    src.rooms[0].data.num_polys = 2;
    src.rooms[0].polygons = {{0, 0, 0, 0}, {1, 0, 0, 0}};

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    REQUIRE(level.faceCount() == 2);
    CHECK(level.vertexCount() == 5);
    CHECK(faceCorner(test::face(level, 0), 1).vertex == faceCorner(test::face(level, 1), 0).vertex);
  }

  TEST_CASE("FtsToLevelHandlesLargeFiniteYCoordinates") {
    constexpr float kLargeY = 1.0e15f;
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::fts::Poly& triangle = src.cells[0].polygons[0];
    triangle.v[0].sy = triangle.v[1].sy = triangle.v[2].sy = kLargeY;

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);
    REQUIRE(level.geometry.vertices.size() == 3);
    for (const pistoris::Vertex& vertex : level.geometry.vertices) CHECK(vertex.position.y == kLargeY);
  }

  TEST_CASE("ExplicitLevelWeldingUsesDefaultEuclideanRadius") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);

    pistoris::fts::Poly a{};
    a.room = 1;
    a.tex = 0;
    a.v[0].ssx = 0.0f;
    a.v[0].ssz = 0.0f;
    a.v[1].ssx = 1.0f;
    a.v[1].ssz = 0.0f;
    a.v[2].ssx = 0.0f;
    a.v[2].ssz = 1.0f;
    a.norm = {0.0f, -1.0f, 0.0f};
    a.nrml[0] = a.nrml[1] = a.nrml[2] = a.norm;

    pistoris::fts::Poly b{};
    b.room = 1;
    b.tex = 0;
    b.v[0].ssx = 0.000075f;
    b.v[0].ssz = 0.0f;
    b.v[1].ssx = 1.0f;
    b.v[1].ssz = 1.0f;
    b.v[2].ssx = 0.0f;
    b.v[2].ssz = 1.0f;
    b.norm = {0.0f, -1.0f, 0.0f};
    b.nrml[0] = b.nrml[1] = b.nrml[2] = b.norm;

    src.cells[0].polygons = {a, b};
    src.scene.num_polys = 2;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    CHECK(level.vertexCount() == 4);
  }

  TEST_CASE("ExplicitLevelWeldingUsesEuclideanRatherThanComponentWiseDistance") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.cells[0].polygons = {
        makeFlatFtsTriangle({{{1.0f, 0.0f, 1.0f}, {10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 2.0f}}}),
        makeFlatFtsTriangle({{{1.000075f, 0.0f, 1.000075f}, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 2.0f}}}),
    };
    src.scene.num_polys = 2;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    CHECK(faceCorner(test::face(level, 0), 0).vertex != faceCorner(test::face(level, 1), 0).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingUsesAFrozenRepresentativeGroup") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.cells[0].polygons = {
        makeFlatFtsTriangle({{{1.0f, 0.0f, 1.0f}, {10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 2.0f}}}),
        makeFlatFtsTriangle({{{1.000075f, 0.0f, 1.0f}, {20.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 2.0f}}}),
        makeFlatFtsTriangle({{{1.00015f, 0.0f, 1.0f}, {30.0f, 0.0f, 0.0f}, {30.0f, 0.0f, 2.0f}}}),
        makeFlatFtsTriangle({{{1.000225f, 0.0f, 1.0f}, {40.0f, 0.0f, 0.0f}, {40.0f, 0.0f, 2.0f}}}),
    };
    src.scene.num_polys = 4;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    CHECK(faceCorner(test::face(level, 0), 0).vertex == faceCorner(test::face(level, 1), 0).vertex);
    CHECK(faceCorner(test::face(level, 1), 0).vertex == faceCorner(test::face(level, 2), 0).vertex);
    CHECK(faceCorner(test::face(level, 2), 0).vertex != faceCorner(test::face(level, 3), 0).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingIgnoresFormerSourceCellDistance") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.scene.sizex = 3;
    src.cells.resize(3);

    pistoris::fts::Poly left{};
    left.room = 1;
    left.tex = 0;
    left.v[0].ssx = 0.0f;
    left.v[0].ssz = 0.0f;
    left.v[1].ssx = 1.0f;
    left.v[1].ssz = 0.0f;
    left.v[2].ssx = 0.0f;
    left.v[2].ssz = 1.0f;
    left.norm = {0.0f, -1.0f, 0.0f};
    left.nrml[0] = left.nrml[1] = left.nrml[2] = left.norm;

    pistoris::fts::Poly right{};
    right.room = 1;
    right.tex = 0;
    right.v[0].ssx = 0.0f;
    right.v[0].ssz = 0.0f;
    right.v[1].ssx = 200.0f;
    right.v[1].ssz = 0.0f;
    right.v[2].ssx = 200.0f;
    right.v[2].ssz = 1.0f;
    right.norm = {0.0f, -1.0f, 0.0f};
    right.nrml[0] = right.nrml[1] = right.nrml[2] = right.norm;

    src.cells[0].polygons.push_back(left);
    src.cells[2].polygons.push_back(right);
    src.scene.num_polys = 2;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    REQUIRE(level.faceCount() == 2);
    CHECK(level.vertexCount() == 5);
    CHECK(faceCorner(test::face(level, 0), 0).vertex == faceCorner(test::face(level, 1), 0).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingPropagatesIdentityAcrossFormerSourceCells") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.scene.sizex = 5;
    src.cells.resize(5);

    auto make_poly = [](float x) {
      pistoris::fts::Poly poly{};
      poly.room = 1;
      poly.tex = 0;
      poly.v[0].ssx = 0.0f;
      poly.v[0].ssz = 0.0f;
      poly.v[1].ssx = x;
      poly.v[1].ssz = 0.0f;
      poly.v[2].ssx = x;
      poly.v[2].ssz = 1.0f;
      poly.norm = {0.0f, -1.0f, 0.0f};
      poly.nrml[0] = poly.nrml[1] = poly.nrml[2] = poly.norm;
      return poly;
    };

    src.cells[0].polygons.push_back(make_poly(1.0f));
    src.cells[2].polygons.push_back(make_poly(201.0f));
    src.cells[4].polygons.push_back(make_poly(401.0f));
    src.scene.num_polys = 3;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    REQUIRE(level.faceCount() == 3);
    CHECK(level.vertexCount() == 7);
    CHECK(faceCorner(test::face(level, 0), 0).vertex == faceCorner(test::face(level, 1), 0).vertex);
    CHECK(faceCorner(test::face(level, 1), 0).vertex == faceCorner(test::face(level, 2), 0).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingDoesNotDependOnFormerSourceCellOccurrences") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.scene.sizex = 5;
    src.cells.resize(5);

    pistoris::fts::Poly first{};
    first.room = 1;
    first.tex = 0;
    first.v[0].ssx = 0.0f;
    first.v[1].ssx = 1.0f;
    first.v[2].ssz = 1.0f;
    first.norm = {0.0f, -1.0f, 0.0f};
    first.nrml[0] = first.nrml[1] = first.nrml[2] = first.norm;

    pistoris::fts::Poly last = first;
    last.v[1].ssx = 401.0f;
    last.v[2].ssx = 401.0f;

    src.cells[0].polygons.push_back(first);
    src.cells[4].polygons.push_back(last);
    src.scene.num_polys = 2;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);
    CHECK(level.vertexCount() == 5);
    CHECK(faceCorner(test::face(level, 0), 0).vertex == faceCorner(test::face(level, 1), 0).vertex);
  }

  TEST_CASE("ExplicitLevelWeldingJoinsPositionsFromDistantSourceCells") {
    pistoris::fts::Data src = makeMinimalFtsData();
    makeAuthorableRoom(src);
    src.scene.sizex = 4;
    src.cells.resize(4);

    pistoris::fts::Poly left{};
    left.room = 1;
    left.tex = 0;
    left.v[0].ssx = 0.0f;
    left.v[0].ssz = 0.0f;
    left.v[1].ssx = 1.0f;
    left.v[1].ssz = 0.0f;
    left.v[2].ssx = 0.0f;
    left.v[2].ssz = 1.0f;
    left.norm = {0.0f, -1.0f, 0.0f};
    left.nrml[0] = left.nrml[1] = left.nrml[2] = left.norm;

    pistoris::fts::Poly right{};
    right.room = 1;
    right.tex = 0;
    right.v[0].ssx = 0.0f;
    right.v[0].ssz = 0.0f;
    right.v[1].ssx = 300.0f;
    right.v[1].ssz = 0.0f;
    right.v[2].ssx = 300.0f;
    right.v[2].ssz = 1.0f;
    right.norm = {0.0f, -1.0f, 0.0f};
    right.nrml[0] = right.nrml[1] = right.nrml[2] = right.norm;

    src.cells[0].polygons.push_back(left);
    src.cells[3].polygons.push_back(right);
    src.scene.num_polys = 2;

    pistoris::Level level;
    REQUIRE(buildAndWeldLevel(level, src) == ARX_OK);

    CHECK(level.vertexCount() == 5);
    CHECK(faceCorner(test::face(level, 0), 0).vertex == faceCorner(test::face(level, 1), 0).vertex);
  }

  TEST_CASE("FtsToLevelResolvesTextureIndicesByTc") {
    pistoris::fts::Data src = makeTriangleFtsScene();

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.geometry.textures.size() == 1);
    CHECK(level.geometry.faces[0].texture == 0);
    CHECK(level.geometry.textures[0] == "graph/levels/test.bmp");
  }

  TEST_CASE("FtsToLevelMapsKnownGameTexturePathsToLibraryAliases") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    auto& texture = src.textures.at(24275104);
    std::snprintf(texture.fic, sizeof(texture.fic), R"(GRAPH\OBJ3D\TEXTURES\NPC_HUMAN__BASE_HERO_HEAD.BMP)");

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.geometry.textures.size() == 1);
    CHECK(level.geometry.textures[0] == "graph/obj3d/textures/npc_human_base_hero_head_1.BMP");
  }

  TEST_CASE("FtsToLevelTreatsEmptyTexturePathsAsNoTexture") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    src.textures.at(24275104).fic[0] = '\0';
    auto& texture = src.textures[1];
    std::snprintf(texture.fic, sizeof(texture.fic), "graph/levels/real.bmp");
    src.scene.num_textures = 2;
    src.cells[0].polygons[1].tex = 1;

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.geometry.textures.size() == 1);
    CHECK(level.geometry.textures[0] == "graph/levels/real.bmp");
    REQUIRE(level.geometry.faces.size() == 2);
    CHECK(level.geometry.faces[0].texture == pistoris::kNoTexture);
    CHECK(level.geometry.faces[1].texture == 0);
  }

  TEST_CASE("FtsToLevelNamesPortalsForDebugging") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.scene.num_rooms = 7;
    src.rooms.resize(8);
    src.room_distances.resize(64);
    src.portals.resize(1);
    src.scene.num_portals = 1;
    src.portals[0].room_1 = 2;
    src.portals[0].room_2 = 7;
    src.portals[0].useportal = 1;
    src.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};
    src.portals[0].poly.v[3].pos = {1.0f, 1.0f, 0.0f};

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.rooms.portals.size() == 1);
    CHECK(level.rooms.portals[0].name == "portal_1");
    CHECK(level.rooms.portals[0].room_1 == 1);
    CHECK(level.rooms.portals[0].room_2 == 6);
    CHECK(level.rooms.portals[0].shape == pistoris::PortalShape::kTriangle);
  }

  TEST_CASE("FtsToLevelPreservesQuadPortalShape") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.scene.num_rooms = 2;
    src.rooms.resize(3);
    src.room_distances.resize(9);
    src.portals.resize(1);
    src.scene.num_portals = 1;
    src.portals[0].room_1 = 1;
    src.portals[0].room_2 = 2;
    src.portals[0].poly.type = pistoris::kFaceBitQuad;
    src.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};
    src.portals[0].poly.v[3].pos = {1.0f, 1.0f, 0.0f};

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.rooms.portals.size() == 1);
    CHECK(level.rooms.portals[0].shape == pistoris::PortalShape::kQuad);
    CHECK(level.rooms.portals[0].vertices[0].x == 0.0f);
    CHECK(level.rooms.portals[0].vertices[1].x == 1.0f);
    CHECK(level.rooms.portals[0].vertices[2].x == 1.0f);
    CHECK(level.rooms.portals[0].vertices[3].x == 0.0f);
  }

  TEST_CASE("FtsToLevelAveragesMatchedRoomDistanceDirections") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    addFtsPortal(src, 1.0f);
    addFtsPortal(src, 4.0f);
    pistoris::fts::RoomDistData& forward = src.room_distances[1 * 3 + 2];
    forward.distance = 10.0f;
    forward.startpos = pistoris::rooms::portalCentroid(makeLevelPortal("high", 4.0f));
    forward.endpos = pistoris::rooms::portalCentroid(makeLevelPortal("low", 1.0f));
    pistoris::fts::RoomDistData& backward = src.room_distances[2 * 3 + 1];
    backward.distance = 14.0f;
    backward.startpos = forward.endpos;
    backward.endpos = forward.startpos;

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.rooms.distances.size() == 1);
    CHECK(level.rooms.distances[0].distance == doctest::Approx(12.0f));
    CHECK(level.rooms.distances[0].low_room_portal == 0);
    CHECK(level.rooms.distances[0].high_room_portal == 1);
    CHECK(logs.contains("1 positive room distance pair(s) had asymmetric distances"));
  }

  TEST_CASE("FtsToLevelAveragesLargeMatchedRoomDistanceDirectionsWithoutOverflow") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    addFtsPortal(src, 1.0f);
    addFtsPortal(src, 4.0f);
    pistoris::fts::RoomDistData& forward = src.room_distances[1 * 3 + 2];
    forward.distance = std::numeric_limits<float>::max();
    forward.startpos = pistoris::rooms::portalCentroid(makeLevelPortal("high", 4.0f));
    forward.endpos = pistoris::rooms::portalCentroid(makeLevelPortal("low", 1.0f));
    pistoris::fts::RoomDistData& backward = src.room_distances[2 * 3 + 1];
    backward.distance = std::numeric_limits<float>::max();
    backward.startpos = forward.endpos;
    backward.endpos = forward.startpos;

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.rooms.distances.size() == 1);
    CHECK(level.rooms.distances[0].distance == std::numeric_limits<float>::max());
  }

  TEST_CASE("FtsToLevelPrefersRepresentedDirectPortalOverCloserFallback") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();

    pistoris::fts::Portal represented;
    represented.room_1 = 1;
    represented.room_2 = 2;
    represented.useportal = 1;
    represented.poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    represented.poly.v[1].pos = {4.0f, 0.0f, 0.0f};
    represented.poly.v[2].pos = {0.0f, 4.0f, 0.0f};
    src.portals.push_back(represented);

    pistoris::fts::Portal fallback;
    fallback.room_1 = 1;
    fallback.room_2 = 2;
    fallback.useportal = 1;
    fallback.poly.v[0].pos = {1.0f, 0.0f, 0.0f};
    fallback.poly.v[1].pos = {1.0f, 4.0f, 0.0f};
    fallback.poly.v[2].pos = {1.0f, 0.0f, 4.0f};
    src.portals.push_back(fallback);
    src.scene.num_portals = static_cast<std::int32_t>(src.portals.size());

    pistoris::fts::RoomDistData& forward = src.room_distances[1 * 3 + 2];
    forward.distance = -1.0f;
    forward.endpos = {1.0f, 1.0f, 0.9f};
    forward.startpos = {2.1f, 1.0f, 0.9f};
    pistoris::fts::RoomDistData& backward = src.room_distances[2 * 3 + 1];
    backward.distance = -1.0f;
    backward.startpos.x = std::numeric_limits<float>::quiet_NaN();

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.rooms.distances.size() == 1);
    CHECK(level.rooms.distances[0].low_room_portal == 0);
    CHECK(level.rooms.distances[0].high_room_portal == 0);
    CHECK_FALSE(logs.contains("fallback direct portal"));
  }

  TEST_CASE("FtsToLevelCanonicalizesUnusableRoomDistanceEndpoints") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    addFtsPortal(src, 1.0f);
    pistoris::fts::RoomDistData& forward = src.room_distances[1 * 3 + 2];
    pistoris::fts::RoomDistData& backward = src.room_distances[2 * 3 + 1];
    forward.distance = -1.0f;
    forward.startpos.x = std::numeric_limits<float>::quiet_NaN();
    backward.distance = -1.0f;
    backward.startpos = {4.0f, 5.0f, 6.0f};
    backward.endpos = {1.0f, 2.0f, 3.0f};

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);
    REQUIRE(level.rooms.distances.size() == 1);
    CHECK(level.rooms.distances[0].distance == doctest::Approx(-1.0f));
    CHECK(level.rooms.distances[0].low_room_portal == 0);
    CHECK(level.rooms.distances[0].high_room_portal == 0);

    forward.endpos.y = std::numeric_limits<float>::infinity();
    backward.startpos = {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};
    REQUIRE(buildLevelModules(level, src) == ARX_OK);
    CHECK(level.rooms.distances[0].low_room_portal == 0);
    CHECK(level.rooms.distances[0].high_room_portal == 0);
  }

  TEST_CASE("FtsToLevelDiscardsPositiveDirectionsWithUnusableEndpoints") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    addFtsPortal(src, 1.0f);
    addFtsPortal(src, 4.0f);
    pistoris::fts::RoomDistData& forward = src.room_distances[1 * 3 + 2];
    pistoris::fts::RoomDistData& backward = src.room_distances[2 * 3 + 1];
    forward.distance = 10.0f;
    forward.startpos.x = std::numeric_limits<float>::quiet_NaN();
    backward.distance = 14.0f;
    backward.startpos = pistoris::rooms::portalCentroid(makeLevelPortal("low", 1.0f));
    backward.endpos = pistoris::rooms::portalCentroid(makeLevelPortal("high", 4.0f));

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);
    REQUIRE(level.rooms.distances.size() == 1);
    CHECK(level.rooms.distances[0].distance == doctest::Approx(14.0f));
    CHECK(level.rooms.distances[0].low_room_portal == 0);
    CHECK(level.rooms.distances[0].high_room_portal == 1);
    CHECK(logs.contains("1 positive room distance direction(s) discarded"));

    backward.startpos.x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(buildLevelModules(level, src) == ARX_OK);
    CHECK(level.rooms.distances[0].distance == doctest::Approx(-1.0f));
    CHECK(level.rooms.distances[0].low_room_portal == pistoris::kInvalidPortalIndex);
    CHECK(level.rooms.distances[0].high_room_portal == pistoris::kInvalidPortalIndex);
  }

  TEST_CASE("FtsToLevelStoresDirectPortalRoomDistanceAsSentinel") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    src.portals.resize(1);
    src.scene.num_portals = 1;
    src.portals[0].room_1 = 1;
    src.portals[0].room_2 = 2;
    src.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};
    const pistoris::ArxVector3 center = {1.0f / 3.0f, 1.0f / 3.0f, 0.0f};
    src.room_distances[1 * 3 + 2] = {-1.0f, center, center};
    src.room_distances[2 * 3 + 1] = {-1.0f, center, center};

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    REQUIRE(level.rooms.distances.size() == 1);
    CHECK(level.rooms.distances[0].distance == doctest::Approx(-1.0f));
    CHECK(level.rooms.distances[0].low_room_portal == 0);
    CHECK(level.rooms.distances[0].high_room_portal == 0);
  }

  TEST_CASE("FtsToLevelWarnsWhenRoom0RoomDistancesArePositive") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    src.room_distances[1].distance = 10.0f;
    src.room_distances[6].distance = 20.0f;

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    CHECK(logs.contains("2 positive room-0 room distance value(s) ignored"));
  }

  TEST_CASE("FtsToLevelWarnsWhenRoomDistanceMatrixHasNoPositiveRealPairs") {
    pistoris::fts::Data src = makeTwoRoomFtsScene();
    src.scene.num_rooms = 4;
    src.rooms.resize(5);
    src.room_distances.clear();
    src.room_distances.resize(25);

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);

    CHECK(logs.contains("room distance matrix has no positive real-room distances"));
  }

  TEST_CASE("LevelGlbExportSilentlyOmitsUnreferencedEmptyRooms") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.rooms.definitions.push_back({"empty"});

    LogCapture logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    CHECK_FALSE(logs.contains("empty room"));
    std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
    CHECK(text.find("arx_room__empty") == std::string::npos);
  }

  TEST_CASE("LevelGlbImportRejectsAmbiguousSceneSelection") {
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(makeSimpleLevel(), glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf.erase("scene");
    parsed.gltf["scenes"].push_back(parsed.gltf["scenes"][0]);

    pistoris::LevelModules dst;
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_AMBIGUOUS_SCENE);
  }

  TEST_CASE("LevelGlbExportReportsDataLostWithEmptyRooms") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.rooms.definitions.push_back({"empty_a"});
    src.rooms.definitions.push_back({"empty_b"});

    pistoris::Portal portal;
    portal.name = "to_empty";
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}}};
    src.rooms.portals.push_back(portal);
    src.rooms.distances.resize(pistoris::rooms::roomDistancePairCount(src.rooms.definitions.size()));
    src.rooms.distances[pistoris::rooms::roomDistancePairIndex(0, 1)] = {
        .distance = 25.0f, .low_room_portal = 0, .high_room_portal = 0};

    LogCapture logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    CHECK(logs.contains("2 empty room(s), 1 referencing portal(s), and 1 positive room-distance pair(s) discarded"));
    std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
    CHECK(text.find("arx_room__empty_a") == std::string::npos);
    CHECK(text.find("arx_room__empty_b") == std::string::npos);
    CHECK(text.find("arx_portal__to_empty") == std::string::npos);
    CHECK(text.find("portals_parent") == std::string::npos);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.rooms.definitions.size() == 1);
    CHECK(dst.rooms.portals.empty());
    CHECK(dst.rooms.distances.empty());
  }

  TEST_CASE("LevelGlbExportCentersEachRoomOriginOnItsAabb") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.rooms.definitions[0].name = "room_1";
    src.rooms.definitions.push_back({"room_2"});
    addSecondRoomTriangle(src);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);

    const std::size_t parent_index = testNodeIndex(parsed, "rooms_parent");
    const std::size_t room_1_index = testNodeIndex(parsed, "arx_room__room_1");
    const std::size_t room_2_index = testNodeIndex(parsed, "arx_room__room_2");
    REQUIRE(parent_index < parsed.gltf["nodes"].size());
    REQUIRE(room_1_index < parsed.gltf["nodes"].size());
    REQUIRE(room_2_index < parsed.gltf["nodes"].size());

    const auto& parent_translation = parsed.gltf["nodes"][parent_index]["translation"];
    const auto& room_1_translation = parsed.gltf["nodes"][room_1_index]["translation"];
    const auto& room_2_translation = parsed.gltf["nodes"][room_2_index]["translation"];
    CHECK(parent_translation[0].get<float>() == doctest::Approx(1.5f));
    CHECK(parent_translation[2].get<float>() == doctest::Approx(-1.5f));
    CHECK(parent_translation[0].get<float>() + room_1_translation[0].get<float>() == doctest::Approx(0.5f));
    CHECK(parent_translation[2].get<float>() + room_1_translation[2].get<float>() == doctest::Approx(-0.5f));
    CHECK(parent_translation[0].get<float>() + room_2_translation[0].get<float>() == doctest::Approx(2.5f));
    CHECK(parent_translation[2].get<float>() + room_2_translation[2].get<float>() == doctest::Approx(-2.5f));

    auto check_local_aabb_center = [&](std::size_t node_index) {
      const int mesh = parsed.gltf["nodes"][node_index]["mesh"].get<int>();
      const int accessor = parsed.gltf["meshes"][mesh]["primitives"][0]["attributes"]["POSITION"].get<int>();
      const std::vector<float> positions = testVec3Accessor(parsed, accessor);
      REQUIRE_FALSE(positions.empty());
      for (std::size_t axis = 0; axis < 3; ++axis) {
        float min = positions[axis];
        float max = min;
        for (std::size_t i = axis; i < positions.size(); i += 3) {
          min = std::min(min, positions[i]);
          max = std::max(max, positions[i]);
        }
        CHECK(std::midpoint(min, max) == doctest::Approx(0.0f));
      }
    };
    check_local_aabb_center(room_1_index);
    check_local_aabb_center(room_2_index);
  }

  TEST_CASE("LevelGlbCoordinateOptionsRoundtripAllSpatialModules") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.surface = pistoris::NavSurface{
        {{{-5.0f, 1.0f, -5.0f}}, {{5.0f, 1.0f, -5.0f}}, {{-5.0f, 1.0f, 5.0f}}},
        {{{{0, 1, 2}}}},
    };
    src.navigation.anchors.push_back({{0.25f, 0.0f, 0.25f}, 12.0f, -34.0f, 0, {}});

    pistoris::Light light;
    light.name = "scaled_light";
    light.position = {4.0f, 6.0f, 8.0f};
    light.color = {0.25f, 0.5f, 0.75f};
    light.fallstart = 3.0f;
    light.fallend = 10.0f;
    light.intensity = 2.0f;
    light.effect_radius = 4.0f;
    src.lighting.lights.push_back(light);

    setUsablePlayerSpawn(src, {{4.0f, 5.0f, 6.0f}, pistoris::math::angleToQuat({10.0f, 20.0f, 30.0f})});
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door",
                                  7,
                                  {7.0f, 8.0f, 9.0f},
                                  pistoris::math::angleToQuat({5.0f, 15.0f, 25.0f}),
                                  "door"});

    pistoris::Fog fog;
    fog.position = {1.0f, 2.0f, 3.0f};
    fog.color = {0.2f, 0.3f, 0.4f};
    fog.size = 30.0f;
    fog.scale = 0.5f;
    fog.speed = 4.0f;
    src.scene.fogs.push_back(fog);

    pistoris::Zone zone;
    zone.name = "scaled_zone";
    zone.perimeter_xz = {{2.0f, 2.0f}, {6.0f, 2.0f}, {6.0f, 6.0f}, {2.0f, 6.0f}};
    zone.reference_y = -1.0f;
    zone.height = 2.0f;
    zone.farclip = 1200.0f;
    src.scene.zones.push_back(zone);

    src.scene.paths.push_back(
        {"scaled_path",
         {10.0f, 20.0f, 30.0f},
         {{{}, pistoris::PathNodeType::kStandard, 0}, {{2.0f, 3.0f, 4.0f}, pistoris::PathNodeType::kBezier, 500}}});

    pistoris::Level::GlbExportOptions export_options;
    export_options.arx_offset = {100.0f, -200.0f, 300.0f};
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlbWithOptions(src, export_options, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t transform_index = testNodeIndex(parsed, "level_space");
    REQUIRE(transform_index < parsed.gltf["nodes"].size());
    REQUIRE(parsed.gltf["scenes"][0]["nodes"].size() == 1);
    CHECK(parsed.gltf["scenes"][0]["nodes"][0] == transform_index);
    CHECK(parsed.gltf["nodes"][transform_index]["scale"][0].get<float>() == doctest::Approx(0.01f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][0].get<float>() == doctest::Approx(-1.0f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][1].get<float>() == doctest::Approx(-2.0f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][2].get<float>() == doctest::Approx(3.0f));
    CHECK_FALSE(parsed.gltf["nodes"][transform_index].contains("rotation"));

    pistoris::Level::GlbImportOptions import_options;
    import_options.arx_offset = export_options.arx_offset;
    pistoris::Level::GlbImportInfo info;
    pistoris::LevelModules dst;
    LogCapture logs;
    REQUIRE(importLevelGlbWithOptions(glb, import_options, dst, &info) == ARX_OK);
    CHECK_FALSE(logs.contains("nonidentity local scale"));
    CHECK_FALSE(logs.contains("rotation or positive scale"));

    CHECK(info.applied_arx_offset.x == doctest::Approx(100.0f));
    CHECK(info.applied_arx_offset.y == doctest::Approx(-200.0f));
    CHECK(info.applied_arx_offset.z == doctest::Approx(300.0f));
    CHECK(dst.geometry.vertices[1].position.x == doctest::Approx(1.0f));
    CHECK(dst.geometry.faces[0].corners[0].normal.y == doctest::Approx(src.geometry.faces[0].corners[0].normal.y));
    REQUIRE(dst.navigation.surface.has_value());
    CHECK(dst.navigation.surface->vertices[0].position.x == doctest::Approx(-5.0f));
    CHECK(dst.navigation.surface->vertices[0].position.z == doctest::Approx(-5.0f));
    REQUIRE(dst.navigation.anchors.size() == 1);
    CHECK(dst.navigation.anchors[0].position.x == doctest::Approx(0.25f));
    CHECK(dst.navigation.anchors[0].radius == doctest::Approx(12.0f));
    CHECK(dst.navigation.anchors[0].height == doctest::Approx(-34.0f));
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].position.y == doctest::Approx(6.0f));
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(3.0f));
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(10.0f));
    CHECK(dst.lighting.lights[0].effect_radius == doctest::Approx(4.0f));
    CHECK(dst.scene.player_spawn.position.z == doctest::Approx(6.0f));
    CHECK(pistoris::math::quatToAngle(dst.scene.player_spawn.rotation).yaw == doctest::Approx(20.0f).epsilon(1.0e-4));
    REQUIRE(dst.scene.entities.size() == 1);
    CHECK(dst.scene.entities[0].position.x == doctest::Approx(7.0f));
    CHECK(pistoris::math::quatToAngle(dst.scene.entities[0].rotation).yaw == doctest::Approx(15.0f).epsilon(1.0e-4));
    REQUIRE(dst.scene.fogs.size() == 1);
    CHECK(dst.scene.fogs[0].position.z == doctest::Approx(3.0f));
    CHECK(dst.scene.fogs[0].size == doctest::Approx(30.0f));
    CHECK(dst.scene.fogs[0].scale == doctest::Approx(0.5f));
    CHECK(dst.scene.fogs[0].speed == doctest::Approx(4.0f));
    REQUIRE(dst.scene.zones.size() == 1);
    REQUIRE(dst.scene.zones[0].perimeter_xz.size() == zone.perimeter_xz.size());
    for (const pistoris::ArxVector2& expected : zone.perimeter_xz) {
      CHECK(std::any_of(dst.scene.zones[0].perimeter_xz.begin(),
                        dst.scene.zones[0].perimeter_xz.end(),
                        [&](const pistoris::ArxVector2& actual) {
                          return actual.x == doctest::Approx(expected.x) && actual.y == doctest::Approx(expected.y);
                        }));
    }
    CHECK(dst.scene.zones[0].reference_y == doctest::Approx(-1.0f));
    CHECK(dst.scene.zones[0].height == doctest::Approx(2.0f));
    REQUIRE(dst.scene.zones[0].farclip.has_value());
    CHECK(*dst.scene.zones[0].farclip == doctest::Approx(1200.0f));
    REQUIRE(dst.scene.paths.size() == 1);
    CHECK(dst.scene.paths[0].position.x == doctest::Approx(10.0f));
    CHECK(dst.scene.paths[0].nodes[1].relative_position.z == doctest::Approx(4.0f));
  }

  TEST_CASE("LevelGlbCoordinateUnitRangeIsInclusive") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    for (float units : {pistoris::kMinArxUnitsPerGlbUnit, pistoris::kMaxArxUnitsPerGlbUnit}) {
      pistoris::Level::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = units;
      REQUIRE(exportLevelGlbWithOptions(src, export_options, glb) == ARX_OK);

      pistoris::Level::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = units;
      import_options.arx_offset = pistoris::ArxVector3{};
      pistoris::LevelModules dst;
      REQUIRE(importLevelGlbWithOptions(glb, import_options, dst) == ARX_OK);
      CHECK(dst.geometry.vertices[1].position.x == doctest::Approx(1.0f));
    }

    pistoris::Level::GlbExportOptions export_options;
    export_options.arx_units_per_glb_unit =
        std::nextafter(pistoris::kMinArxUnitsPerGlbUnit, -std::numeric_limits<float>::infinity());
    CHECK(exportLevelGlbWithOptions(src, export_options, glb) == ARX_INVALID_OPTIONS);
    export_options.arx_units_per_glb_unit =
        std::nextafter(pistoris::kMaxArxUnitsPerGlbUnit, std::numeric_limits<float>::infinity());
    CHECK(exportLevelGlbWithOptions(src, export_options, glb) == ARX_INVALID_OPTIONS);

    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::Level::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = 0.0f;
    pistoris::LevelModules dst;
    CHECK(importLevelGlbWithOptions(glb, import_options, dst) == ARX_INVALID_OPTIONS);
    import_options.arx_units_per_glb_unit = std::numeric_limits<float>::infinity();
    CHECK(importLevelGlbWithOptions(glb, import_options, dst) == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("LevelGlbCoordinateConversionRejectsOverflow") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src, {{}, {}});
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t spawn_index = testNodeIndex(parsed, "arx_player_spawn__spawn");
    REQUIRE(spawn_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][spawn_index]["translation"][0] = std::numeric_limits<float>::max();

    pistoris::Level::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = pistoris::kMaxArxUnitsPerGlbUnit;
    import_options.arx_offset = pistoris::ArxVector3{};
    pistoris::LevelModules dst;
    CHECK(importLevelGlbWithOptions(writeTestGlb(std::move(parsed)), import_options, dst) == ARX_GLB_BAD_FORMAT);

    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door",
                                  7,
                                  {std::numeric_limits<float>::max(), 0.0f, 0.0f},
                                  {},
                                  "overflow_entity"});
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    import_options.arx_units_per_glb_unit = pistoris::kMinArxUnitsPerGlbUnit;
    import_options.arx_offset = pistoris::ArxVector3{std::numeric_limits<float>::max(), 0.0f, 0.0f};
    CHECK(importLevelGlbWithOptions(glb, import_options, dst) == ARX_INVALID_OPTIONS);
  }

  TEST_CASE("LevelGlbZoneClassificationUsesPreplacementArxCoordinates") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone infinite;
    infinite.name = "infinite";
    infinite.perimeter_xz = {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
    infinite.height_mode = pistoris::ZoneHeightMode::kInfinite;
    src.scene.zones.push_back(infinite);

    pistoris::Zone finite;
    finite.name = "finite";
    finite.perimeter_xz = {{3.0f, 0.0f}, {5.0f, 0.0f}, {5.0f, 2.0f}, {3.0f, 2.0f}};
    finite.reference_y = 1.0f;
    finite.height = 0.999f;
    src.scene.zones.push_back(finite);

    pistoris::Level::GlbExportOptions export_options;
    export_options.arx_units_per_glb_unit = pistoris::kMaxArxUnitsPerGlbUnit;
    export_options.arx_offset = {100.0f, -200.0f, 300.0f};
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlbWithOptions(src, export_options, glb) == ARX_OK);

    pistoris::Level::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = export_options.arx_units_per_glb_unit;
    import_options.arx_offset = export_options.arx_offset;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlbWithOptions(glb, import_options, dst) == ARX_OK);
    REQUIRE(dst.scene.zones.size() == 2);
    CHECK(dst.scene.zones[0].height_mode == pistoris::ZoneHeightMode::kInfinite);
    CHECK(dst.scene.zones[1].height_mode == pistoris::ZoneHeightMode::kFinite);
    CHECK(dst.scene.zones[1].height == doctest::Approx(0.999f));
  }

  TEST_CASE("LevelGlbImportAutoPlacesOnHundredUnitStepsAndRejectsOversizeGeometry") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t room_parent_index = testNodeIndex(parsed, "rooms_parent");
    REQUIRE(room_parent_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][room_parent_index]["translation"][0] = -0.7f;

    pistoris::Level::GlbImportOptions options;
    pistoris::Level::GlbImportInfo info;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlbWithOptions(writeTestGlb(parsed), options, dst, &info) == ARX_OK);
    CHECK(info.applied_arx_offset.x == doctest::Approx(200.0f));
    CHECK(info.applied_arx_offset.y == doctest::Approx(0.0f));
    CHECK(info.applied_arx_offset.z == doctest::Approx(0.0f));
    CHECK(dst.geometry.vertices[0].position.x == doctest::Approx(80.0f));
    CHECK(dst.geometry.vertices[1].position.x == doctest::Approx(180.0f));

    src.geometry.vertices[1].position.x = pistoris::kLevelMaxXZ;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(importLevelGlbWithOptions(glb, options, dst) == ARX_GLB_BAD_LEVEL_GEOMETRY);
  }

  TEST_CASE("LevelGlbNameDefaultsUseSourceUnitsBeforeScaling") {
    pistoris::LevelModules src = makeSimpleLevel();

    pistoris::Anchor default_anchor;
    default_anchor.position = {0.25f, 0.0f, 0.25f};
    default_anchor.name = "default_anchor";
    src.navigation.anchors.push_back(default_anchor);

    pistoris::Anchor custom_anchor;
    custom_anchor.position = {0.5f, 0.0f, 0.25f};
    custom_anchor.radius = 75.0f;
    custom_anchor.height = -200.0f;
    custom_anchor.name = "custom_anchor";
    src.navigation.anchors.push_back(custom_anchor);

    pistoris::Light light;
    light.name = "default_light";
    light.position = {0.25f, 0.0f, 0.25f};
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    src.lighting.lights.push_back(light);

    pistoris::Level::GlbExportOptions export_options;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlbWithOptions(src, export_options, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(testNodeIndex(parsed, "arx_anchor__default_anchor") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "arx_anchor__RADIUS_0.75__HEIGHT_2__custom_anchor") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "arx_light__FALLEND_0.1__default_light") < parsed.gltf["nodes"].size());

    pistoris::Level::GlbImportOptions import_options;
    import_options.arx_offset = pistoris::ArxVector3{};
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlbWithOptions(glb, import_options, dst) == ARX_OK);

    REQUIRE(dst.navigation.anchors.size() == 2);
    CHECK(dst.navigation.anchors[0].name == "default_anchor");
    CHECK(dst.navigation.anchors[0].radius == pistoris::kDefaultAnchorRadius);
    CHECK(dst.navigation.anchors[0].height == pistoris::kDefaultAnchorHeight);
    CHECK(dst.navigation.anchors[1].name == "custom_anchor");
    CHECK(dst.navigation.anchors[1].radius == doctest::Approx(75.0f));
    CHECK(dst.navigation.anchors[1].height == doctest::Approx(-200.0f));
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(5.0f));
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(10.0f));
  }

  TEST_CASE("LevelGlbImportRetainsAnchorsOutsideGeometryAndRejectsNativeBoundsViolations") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.anchors.push_back({{2.0f, 0.0f, 0.0f}, 50.0f, -165.0f, 0, "outside_geometry"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.navigation.anchors.size() == 1);
    CHECK(dst.navigation.anchors[0].position.x == doctest::Approx(2.0f));
    CHECK(logs.contains("1 anchor(s) outside referenced geometry bounds retained"));

    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t anchor_index = testNodeIndex(parsed, "arx_anchor__outside_geometry");
    REQUIRE(anchor_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][anchor_index]["translation"][0] = 20000.0f;
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ANCHOR);
  }

  TEST_CASE("LevelGlbRoundtripToleratesCoordinateRoundoffAtGeometryBounds") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.vertices[1].position.y = 975.07904052734375f;
    src.navigation.anchors.push_back({{1.0f, 975.07904052734375f, 0.0f}, 50.0f, -165.0f, 0, "upper_boundary"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.navigation.anchors.size() == 1);
    CHECK_FALSE(logs.contains("outside referenced geometry bounds"));
  }

  TEST_CASE("LevelDebugGlbUsesTheConfiguredCoordinateConversion") {
    pistoris::Level level;
    pistoris::fts::Data fts = makeTriangleFtsScene();
    REQUIRE(pistoris::Level::fromNative(level, fts) == ARX_OK);

    pistoris::Level::GlbExportOptions options;
    options.arx_units_per_glb_unit = 50.0f;
    options.arx_offset = {100.0f, 0.0f, 200.0f};
    std::vector<std::uint8_t> glb;
    REQUIRE(pistoris::level_debug::exportNavigationDebugGlb(level, glb, nullptr, options) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t transform_index = testNodeIndex(parsed, "level_space");
    REQUIRE(transform_index < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][transform_index]["scale"][0].get<float>() == doctest::Approx(0.02f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][0].get<float>() == doctest::Approx(-2.0f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][2].get<float>() == doctest::Approx(4.0f));
    CHECK_FALSE(parsed.gltf["nodes"][transform_index].contains("rotation"));
  }

  TEST_CASE("LevelGlbRoundtripPreservesPortalAndAnchorNamesAndDimensions") {
    pistoris::LevelModules src;
    addPortalRooms(src);
    src.geometry.textures.push_back("graph/test.bmp");
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f}}},
                                  0,
                                  pistoris::kFaceBitStone,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    addSecondRoomTriangle(src);
    pistoris::Portal portal;
    connectPortal(portal, "hallway");
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, {200.0f, 300.0f, 3.0f}, {0.0f, 300.0f, 0.0f}}};
    src.rooms.portals.push_back(portal);
    src.navigation.anchors.push_back(
        {{0.25f, 0.0f, 0.25f}, 4.0f, -5.0f, pistoris::kAnchorFlagBlocked, "navigation_start"});
    pistoris::Anchor default_anchor;
    default_anchor.position = {0.75f, 0.0f, 0.25f};
    default_anchor.name = "navigation_end";
    src.navigation.anchors.push_back(default_anchor);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
    CHECK(text.find("\"indices\"") != std::string::npos);
    CHECK(text.find("arx_anchor_links") == std::string::npos);
    CHECK(text.find("arx_portal__room_1__room_2__hallway") != std::string::npos);
    CHECK(text.find("arx_anchor__RADIUS_4__HEIGHT_5__BLOCKED__navigation_start") != std::string::npos);
    CHECK(text.find("__HEIGHT_5") != std::string::npos);
    CHECK(text.find("__BLOCKED") != std::string::npos);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.rooms.portals.size() == 1);
    CHECK(dst.rooms.portals[0].name == "hallway");
    CHECK(dst.rooms.portals[0].shape == pistoris::PortalShape::kQuad);
    for (std::size_t i = 0; i < 4; ++i) {
      CHECK(dst.rooms.portals[0].vertices[i].x == doctest::Approx(src.rooms.portals[0].vertices[i].x));
      CHECK(dst.rooms.portals[0].vertices[i].y == doctest::Approx(src.rooms.portals[0].vertices[i].y));
      CHECK(dst.rooms.portals[0].vertices[i].z == doctest::Approx(src.rooms.portals[0].vertices[i].z));
    }
    REQUIRE(dst.navigation.anchors.size() == 2);
    CHECK(dst.navigation.anchors[0].name == "navigation_start");
    CHECK(dst.navigation.anchors[0].flags == pistoris::kAnchorFlagBlocked);
    CHECK(dst.navigation.anchors[0].position.x == 0.25f);
    CHECK(dst.navigation.anchors[0].position.y == 0.0f);
    CHECK(dst.navigation.anchors[0].position.z == 0.25f);
    CHECK(dst.navigation.anchors[0].radius == 4.0f);
    CHECK(dst.navigation.anchors[0].height == -5.0f);
    CHECK(dst.navigation.anchors[1].name == "navigation_end");
    CHECK(dst.navigation.anchors[1].radius == pistoris::kDefaultAnchorRadius);
    CHECK(dst.navigation.anchors[1].height == pistoris::kDefaultAnchorHeight);
    CHECK(dst.navigation.connections.empty());

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t custom_anchor = testNodeIndex(parsed, "arx_anchor__RADIUS_4__HEIGHT_5__BLOCKED__navigation_start");
    std::size_t default_anchor_node = testNodeIndex(parsed, "arx_anchor__navigation_end");
    std::size_t room_parent = testNodeIndex(parsed, "rooms_parent");
    std::size_t anchor_parent = testNodeIndex(parsed, "anchors_parent");
    std::size_t portal_node = testNodeIndex(parsed, "arx_portal__room_1__room_2__hallway");
    REQUIRE(custom_anchor < parsed.gltf["nodes"].size());
    REQUIRE(default_anchor_node < parsed.gltf["nodes"].size());
    REQUIRE(room_parent < parsed.gltf["nodes"].size());
    REQUIRE(anchor_parent < parsed.gltf["nodes"].size());
    REQUIRE(portal_node < parsed.gltf["nodes"].size());
    CHECK_FALSE(parsed.gltf["nodes"][custom_anchor].contains("scale"));
    CHECK(parsed.gltf["nodes"][room_parent]["translation"][1].get<float>() == doctest::Approx(0.0f));
    CHECK(parsed.gltf["nodes"][anchor_parent]["translation"][1].get<float>() == doctest::Approx(-10.0f));

    parsed.gltf["nodes"][custom_anchor]["name"] = "arx_anchor__RADIUS_4__HEIGHT_5__BLOCKED__navigation_start.001";
    parsed.gltf["nodes"][portal_node]["name"] = "arx_portal__room_1__room_2__hallway.001";
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.rooms.portals.size() == 1);
    REQUIRE(dst.navigation.anchors.size() == 2);
    CHECK(dst.rooms.portals[0].name == "hallway.001");
    CHECK(dst.navigation.anchors[0].name == "navigation_start.001");
    CHECK(dst.navigation.anchors[0].radius == 4.0f);
    CHECK(dst.navigation.anchors[0].height == -5.0f);
    CHECK(dst.navigation.anchors[0].flags == pistoris::kAnchorFlagBlocked);
  }

  TEST_CASE("LevelAnchorConnectionGenerationIsExplicit") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.anchors.push_back({{0.25f, 0.0f, 0.25f}, 10.0f, -20.0f, 0, {}});
    src.navigation.anchors.push_back({{0.75f, 0.0f, 0.25f}, 10.0f, -20.0f, 0, {}});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(dst.navigation.connections.empty());

    std::vector<pistoris::AnchorConnection> connections;
    REQUIRE(pistoris::navigation::generateAnchorConnections(
                connections, dst.geometry, dst.navigation.anchors, {.max_step_up = 10.0f}) ==
            pistoris::navigation::Error::kNone);
    dst.navigation.connections = std::move(connections);
    REQUIRE(dst.navigation.connections.size() == 1);
    CHECK(dst.navigation.connections[0].first == 0);
    CHECK(dst.navigation.connections[0].second == 1);

    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(pistoris::navigation::generateAnchorConnections(
                connections, dst.geometry, dst.navigation.anchors, {.max_distance = 0.25f, .max_step_up = 10.0f}) ==
            pistoris::navigation::Error::kNone);
    dst.navigation.connections = std::move(connections);
    CHECK(dst.navigation.connections.empty());
  }

  TEST_CASE("LevelGlbImportDoesNotLinkBlockedAnchors") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::ArxVector3 wall_normal{-1.0f, 0.0f, 0.0f};
    std::uint32_t base = static_cast<std::uint32_t>(src.geometry.vertices.size());
    src.geometry.vertices.push_back({{0.5f, 0.0f, 0.0f}});
    src.geometry.vertices.push_back({{0.5f, -100.0f, 0.0f}});
    src.geometry.vertices.push_back({{0.5f, 0.0f, 2.0f}});
    src.geometry.vertices.push_back({{0.5f, -100.0f, 2.0f}});
    src.geometry.faces.push_back({{{{base + 0, wall_normal, 0.0f, 0.0f},
                                    {base + 1, wall_normal, 0.0f, 0.0f},
                                    {base + 2, wall_normal, 0.0f, 0.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back({{{{base + 2, wall_normal, 0.0f, 0.0f},
                                    {base + 1, wall_normal, 0.0f, 0.0f},
                                    {base + 3, wall_normal, 0.0f, 0.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    src.navigation.anchors.push_back({{0.25f, 0.0f, 0.25f}, 10.0f, -20.0f, 0, {}});
    src.navigation.anchors.push_back({{0.75f, 0.0f, 0.25f}, 10.0f, -20.0f, 0, {}});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    std::vector<pistoris::AnchorConnection> connections;
    REQUIRE(pistoris::navigation::generateAnchorConnections(connections, dst.geometry, dst.navigation.anchors, {}) ==
            pistoris::navigation::Error::kNone);
    dst.navigation.connections = std::move(connections);
    CHECK(dst.navigation.connections.empty());
  }

  TEST_CASE("LevelAnchorGenerationUsesNavSurface") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}}, {{200.0f, 0.0f, 0.0f}}, {{200.0f, 0.0f, 200.0f}}, {{0.0f, 0.0f, 200.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    level.geometry.faces.clear();
    level.rooms.face_rooms.clear();
    level.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 1.0f, 1.0f}}}, 0, 0, 0.0f});
    level.rooms.face_rooms.push_back(0);
    level.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {2, normal, 1.0f, 1.0f}, {3, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    level.rooms.face_rooms.push_back(0);
    level.navigation.surface = pistoris::NavSurface{
        {{{0.0f, 0.0f, 0.0f}}, {{200.0f, 0.0f, 0.0f}}, {{200.0f, 0.0f, 200.0f}}, {{0.0f, 0.0f, 200.0f}}},
        {{{{0, 1, 2}}}, {{{0, 2, 3}}}},
    };
    level.navigation.anchors.push_back({{10.0f, 0.0f, 10.0f}, 1.0f, -1.0f, 0, {}});
    level.navigation.connections.push_back({0, 0});

    pistoris::GeometryDerived derived;
    REQUIRE(pistoris::geometry::validate(level.geometry, &derived) == pistoris::geometry::Error::kNone);
    std::vector<pistoris::Anchor> anchors;
    REQUIRE(pistoris::navigation::generateAnchors(anchors,
                                                  level.geometry,
                                                  *level.navigation.surface,
                                                  derived.referenced_bounds,
                                                  {.sample_spacing = 100.0f, .radius = 50.0f, .height = -165.0f}) ==
            pistoris::navigation::Error::kNone);
    level.navigation.anchors = std::move(anchors);
    level.navigation.connections.clear();

    CHECK(level.navigation.connections.empty());
    REQUIRE(level.navigation.anchors.size() >= 3);
    for (const pistoris::Anchor& anchor : level.navigation.anchors) {
      CHECK(anchor.radius == doctest::Approx(50.0f));
      CHECK(anchor.height == doctest::Approx(-165.0f));
      CHECK(anchor.flags == 0);
    }
  }

  TEST_CASE("LevelGlbRoundtripPreservesNavSurface") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.surface = pistoris::NavSurface{
        {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, 1.0f}}},
        {{{{0, 1, 2}}}, {{{0, 2, 3}}}},
    };

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    checkTestMaterial(parsed, "arx_nav_surface", {0.00f, 0.65f, 0.85f, 0.40f}, "BLEND", true);
    const std::size_t nav_surface = testNodeIndex(parsed, "arx_nav_surface__surface");
    REQUIRE(nav_surface < parsed.gltf["nodes"].size());
    const std::size_t mesh = parsed.gltf["nodes"][nav_surface]["mesh"].get<std::size_t>();
    REQUIRE(mesh < parsed.gltf["meshes"].size());
    CHECK(parsed.gltf["meshes"][mesh]["name"] == "arx_nav_surface__surface");

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.navigation.surface.has_value());
    CHECK(dst.navigation.surface->vertices.size() == 4);
    CHECK(dst.navigation.surface->triangles.size() == 2);
  }

  TEST_CASE("LevelGlbRoundtripHandlesLargeFiniteNavSurfaceYCoordinates") {
    constexpr float kLargeY = 1.0e15f;
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.surface = pistoris::NavSurface{
        {{{0.0f, kLargeY, 0.0f}}, {{1.0f, kLargeY, 0.0f}}, {{0.0f, kLargeY, 1.0f}}},
        {{{{0, 1, 2}}}},
    };

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.navigation.surface.has_value());
    REQUIRE(dst.navigation.surface->vertices.size() == 3);
    for (const pistoris::Vertex& vertex : dst.navigation.surface->vertices) CHECK(std::isfinite(vertex.position.y));
  }

  TEST_CASE("LevelGlbImportWarnsAboutDisconnectedNavSurface") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.surface = pistoris::NavSurface{
        {{{0.0f, 0.0f, 0.0f}},
         {{1.0f, 0.0f, 0.0f}},
         {{0.0f, 0.0f, 1.0f}},
         {{10.0f, 0.0f, 10.0f}},
         {{11.0f, 0.0f, 10.0f}},
         {{10.0f, 0.0f, 11.0f}}},
        {{{{0, 1, 2}}}, {{{3, 4, 5}}}},
    };

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    pistoris::LevelModules dst;
    LogCapture logs;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(logs.contains("navigation surface has 2 disconnected component(s)"));
    REQUIRE(dst.navigation.surface.has_value());
    CHECK(dst.navigation.surface->triangles.size() == 2);
  }

  TEST_CASE("LevelGlbExportGroupsAndCentersPortals") {
    pistoris::LevelModules src;
    addPortalRooms(src);
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{4.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 4.0f}}};
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f}}}});
    src.rooms.face_rooms.push_back(0);
    addSecondRoomTriangle(src);
    pistoris::Portal quad;
    connectPortal(quad, "quad");
    quad.vertices = {{{1.0f, -2.0f, 1.0f}, {3.0f, -2.0f, 1.0f}, {3.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}}};
    src.rooms.portals.push_back(quad);
    pistoris::Portal triangle;
    connectPortal(triangle, "triangle");
    triangle.shape = pistoris::PortalShape::kTriangle;
    triangle.vertices = {{{0.0f, -3.0f, 2.0f}, {3.0f, -3.0f, 2.0f}, {0.0f, 0.0f, 2.0f}, {}}};
    src.rooms.portals.push_back(triangle);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    checkTestMaterial(parsed, "arx_portal", {0.10f, 0.45f, 1.00f, 0.35f}, "BLEND", true);

    std::size_t parent_index = testNodeIndex(parsed, "portals_parent");
    REQUIRE(parent_index < parsed.gltf["nodes"].size());
    const auto& parent = parsed.gltf["nodes"][parent_index];
    REQUIRE(parent["translation"].size() == 3);
    CHECK(parent["translation"][0].get<float>() == doctest::Approx(2.0f));
    CHECK(parent["translation"][1].get<float>() == doctest::Approx(-20.0f));
    CHECK(parent["translation"][2].get<float>() == doctest::Approx(-2.0f));
    REQUIRE(parent["children"].size() == 2);

    const auto& roots = parsed.gltf["scenes"][0]["nodes"];
    REQUIRE(roots.size() == 1);
    const std::size_t level_space_index = testNodeIndex(parsed, "level_space");
    REQUIRE(level_space_index < parsed.gltf["nodes"].size());
    CHECK(roots[0] == level_space_index);
    const auto& level_roots = parsed.gltf["nodes"][level_space_index]["children"];
    CHECK(std::find(level_roots.begin(), level_roots.end(), parent_index) != level_roots.end());
    for (const pistoris::Portal& portal : src.rooms.portals) {
      std::string name = std::string("arx_portal__room_1__room_2__") + portal.name;
      std::size_t portal_index = testNodeIndex(parsed, name);
      REQUIRE(portal_index < parsed.gltf["nodes"].size());
      CHECK(std::find(roots.begin(), roots.end(), portal_index) == roots.end());
      CHECK(std::find(parent["children"].begin(), parent["children"].end(), portal_index) != parent["children"].end());

      const auto& node = parsed.gltf["nodes"][portal_index];
      int mesh_index = node["mesh"].get<int>();
      int position_accessor = parsed.gltf["meshes"][mesh_index]["primitives"][0]["attributes"]["POSITION"].get<int>();
      std::vector<float> positions = testVec3Accessor(parsed, position_accessor);
      std::size_t count = portal.shape == pistoris::PortalShape::kQuad ? 4U : 3U;
      for (std::size_t component = 0; component < 3; ++component) {
        float sum = 0.0f;
        for (std::size_t i = 0; i < count; ++i) sum += positions[i * 3 + component];
        CHECK(sum / static_cast<float>(count) == doctest::Approx(0.0f));
      }
      for (std::size_t i = 0; i < count; ++i) {
        std::array<float, 3> expected = {
            portal.vertices[i].x,
            -portal.vertices[i].y,
            -portal.vertices[i].z,
        };
        for (std::size_t component = 0; component < 3; ++component) {
          float world = parent["translation"][component].get<float>() + node["translation"][component].get<float>() +
                        positions[i * 3 + component];
          CHECK(world == doctest::Approx(expected[component]));
        }
      }
    }
  }

  TEST_CASE("LevelGlbRoundtripPreservesTrianglePortalShape") {
    pistoris::LevelModules src;
    addPortalRooms(src);
    src.geometry.textures.push_back("graph/test.bmp");
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    addSecondRoomTriangle(src);
    pistoris::Portal portal;
    connectPortal(portal, "tri");
    portal.shape = pistoris::PortalShape::kTriangle;
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {}}};
    src.rooms.portals.push_back(portal);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.rooms.portals.size() == 1);
    CHECK(dst.rooms.portals[0].name == "tri");
    CHECK(dst.rooms.portals[0].shape == pistoris::PortalShape::kTriangle);
  }

  TEST_CASE("LevelGlbPlayerSpawnFallbackStateIsSemantic") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(testNodeIndex(parsed, "arx_player_spawn__spawn") == parsed.gltf["nodes"].size());

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(dst.scene.player_spawn_is_fallback);

    setUsablePlayerSpawn(src, {});
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    CHECK(testNodeIndex(parsed, "arx_player_spawn__spawn") < parsed.gltf["nodes"].size());

    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK_FALSE(dst.scene.player_spawn_is_fallback);
    CHECK(dst.scene.player_spawn.position.x == doctest::Approx(0.0f));
    CHECK(dst.scene.player_spawn.rotation.w == doctest::Approx(1.0f));
  }

  TEST_CASE("LevelGlbSingletonRootsAcceptShortAndHelperNames") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src, {{4.0f, 5.0f, 6.0f}, {}});
    src.navigation.surface = pistoris::NavSurface{
        {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}},
        {{{{0, 1, 2}}}},
    };

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t spawn_index = testNodeIndex(parsed, "arx_player_spawn__spawn");
    const std::size_t nav_surface_index = testNodeIndex(parsed, "arx_nav_surface__surface");
    REQUIRE(spawn_index < parsed.gltf["nodes"].size());
    REQUIRE(nav_surface_index < parsed.gltf["nodes"].size());

    parsed.gltf["nodes"][spawn_index]["name"] = "arx_player_spawn";
    parsed.gltf["nodes"][nav_surface_index]["name"] = "arx_nav_surface";
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK_FALSE(dst.scene.player_spawn_is_fallback);
    REQUIRE(dst.navigation.surface.has_value());

    parsed.gltf["nodes"][spawn_index]["name"] = "arx_player_spawn__spawn.001";
    parsed.gltf["nodes"][nav_surface_index]["name"] = "arx_nav_surface__surface.001";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK_FALSE(dst.scene.player_spawn_is_fallback);
    REQUIRE(dst.navigation.surface.has_value());

    parsed.gltf["nodes"][spawn_index]["name"] = "arx_player_spawn__";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_PLAYER_SPAWN);

    parsed.gltf["nodes"][spawn_index]["name"] = "arx_player_spawn__spawn";
    parsed.gltf["nodes"][nav_surface_index]["name"] = "arx_nav_surface__surface__extra";
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_NAV_SURFACE);
  }

  TEST_CASE("LevelGlbImportAcceptsDuplicatePlayerSpawnsWithoutSelectionGuarantee") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src, {{4.0f, 5.0f, 6.0f}, {}});
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t spawn_index = testNodeIndex(parsed, "arx_player_spawn__spawn");
    REQUIRE(spawn_index < parsed.gltf["nodes"].size());
    nlohmann::json duplicate = parsed.gltf["nodes"][spawn_index];
    duplicate["translation"] = {40.0f, 50.0f, 60.0f};
    const std::size_t duplicate_index = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back(duplicate);
    const std::size_t scene_index = parsed.gltf.value("scene", 0U);
    parsed.gltf["scenes"][scene_index]["nodes"].push_back(duplicate_index);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK_FALSE(dst.scene.player_spawn_is_fallback);
    const pistoris::ArxVector3& selected = dst.scene.player_spawn.position;
    const bool selected_original = selected.x == doctest::Approx(4.0f) && selected.y == doctest::Approx(5.0f) &&
                                   selected.z == doctest::Approx(6.0f);
    const bool selected_duplicate = selected.x == doctest::Approx(40.0f) && selected.y == doctest::Approx(50.0f) &&
                                    selected.z == doctest::Approx(60.0f);
    const bool selected_known = selected_original || selected_duplicate;
    CHECK(selected_known);
  }

  TEST_CASE("LevelGlbRoundtripPreservesPlayerSpawnAndEntities") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src,
                         pistoris::PlayerSpawn{{4.0f, 5.0f, 6.0f}, pistoris::math::angleToQuat({10.0f, 20.0f, 30.0f})});
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/timed_lever/timed_lever",
                                  -1,
                                  {7.0f, 8.0f, 9.0f},
                                  pistoris::math::angleToQuat({5.0f, 15.0f, 25.0f}),
                                  "IDENT_42"});
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door",
                                  42,
                                  {10.0f, 11.0f, 12.0f},
                                  pistoris::math::angleToQuat({2.0f, 4.0f, 6.0f}),
                                  "door"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(testNodeIndex(parsed, "arx_player_spawn__spawn") < parsed.gltf["nodes"].size());
    std::size_t entity_parent = testNodeIndex(parsed, "entities_parent");
    REQUIRE(entity_parent < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][entity_parent]["translation"][1].get<float>() == doctest::Approx(-40.0f));
    CHECK(testNodeIndex(parsed, "arx_entity__000__IDENT_-1__IDENT_42") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "arx_entity__001__IDENT_42__door") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "CLASS_model:fix_inter:timed_lever__IDENT_42") < parsed.gltf["nodes"].size());

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(dst.scene.player_spawn.position.x == doctest::Approx(4.0f));
    CHECK(dst.scene.player_spawn.position.y == doctest::Approx(5.0f));
    CHECK(dst.scene.player_spawn.position.z == doctest::Approx(6.0f));
    CHECK_FALSE(dst.scene.player_spawn_is_fallback);
    pistoris::ArxAngle spawn_angle = pistoris::math::quatToAngle(dst.scene.player_spawn.rotation);
    CHECK(spawn_angle.pitch == doctest::Approx(10.0f).epsilon(1.0e-4));
    CHECK(spawn_angle.yaw == doctest::Approx(20.0f).epsilon(1.0e-4));
    CHECK(spawn_angle.roll == doctest::Approx(30.0f).epsilon(1.0e-4));
    REQUIRE(dst.scene.entities.size() == 2);
    CHECK(dst.scene.entities[0].ident == -1);
    CHECK(dst.scene.entities[0].name == "IDENT_42");
    CHECK(dst.scene.entities[0].class_path == "graph/obj3d/interactive/fix_inter/timed_lever/timed_lever");
    CHECK(dst.scene.entities[0].position.x == doctest::Approx(7.0f));
    pistoris::ArxAngle first_entity_angle = pistoris::math::quatToAngle(dst.scene.entities[0].rotation);
    CHECK(first_entity_angle.pitch == doctest::Approx(5.0f).epsilon(1.0e-4));
    CHECK(first_entity_angle.yaw == doctest::Approx(15.0f).epsilon(1.0e-4));
    CHECK(first_entity_angle.roll == doctest::Approx(25.0f).epsilon(1.0e-4));
    CHECK(dst.scene.entities[1].ident == 42);
    CHECK(dst.scene.entities[1].name == "door");
    CHECK(dst.scene.entities[1].class_path == "graph/obj3d/interactive/fix_inter/door/door");
    CHECK(dst.scene.entities[1].position.z == doctest::Approx(12.0f));
    pistoris::ArxAngle second_entity_angle = pistoris::math::quatToAngle(dst.scene.entities[1].rotation);
    CHECK(second_entity_angle.pitch == doctest::Approx(2.0f).epsilon(1.0e-4));
    CHECK(second_entity_angle.yaw == doctest::Approx(4.0f).epsilon(1.0e-4));
    CHECK(second_entity_angle.roll == doctest::Approx(6.0f).epsilon(1.0e-4));
  }

  TEST_CASE("LevelGlbEntityClassHelpersUseShorthandOnlyForCanonicalModelClasses") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door", -1, {1.0f, 2.0f, 3.0f}, {}, "door"});
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/fix_inter/custom/layout/door", -1, {4.0f, 5.0f, 6.0f}, {}, "custom"});
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/items/armor/chest_chain/chest_chain", -1, {7.0f, 8.0f, 9.0f}, {}, "armor"});
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin2", -1, {10.0f, 11.0f, 12.0f}, {}, "variant"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(testNodeIndex(parsed, "CLASS_model:fix_inter:door__door") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "CLASS_model:armor:chest_chain__armor") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "CLASS_graph/obj3d/interactive/fix_inter/custom/layout/door__custom") <
          parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "CLASS_graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin2__variant") <
          parsed.gltf["nodes"].size());

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.scene.entities.size() == 4);
    CHECK(dst.scene.entities[0].class_path == "graph/obj3d/interactive/fix_inter/door/door");
    CHECK(dst.scene.entities[1].class_path == "graph/obj3d/interactive/fix_inter/custom/layout/door");
    CHECK(dst.scene.entities[2].class_path == "graph/obj3d/interactive/items/armor/chest_chain/chest_chain");
    CHECK(dst.scene.entities[3].class_path == "graph/obj3d/interactive/items/jewelry/gold_coin/gold_coin2");

    const std::size_t shorthand = testNodeIndex(parsed, "CLASS_model:armor:chest_chain__armor");
    REQUIRE(shorthand < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][shorthand]["name"] = "CLASS_model:armor:chest_chain.teo__armor";
    LogCapture logs;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.entities.size() == 4);
    CHECK(dst.scene.entities[2].class_path == "graph/obj3d/interactive/items/armor/chest_chain/chest_chain");
    CHECK(logs.contains("normalized 1 legacy .teo entity class path(s)"));

    parsed.gltf["nodes"][shorthand]["name"] = "CLASS_model:items:armor:chest_chain__armor";
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ENTITY);

    parsed = parseTestGlb(glb);
    const std::size_t nonitem_shorthand = testNodeIndex(parsed, "CLASS_model:fix_inter:door__door");
    REQUIRE(nonitem_shorthand < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][nonitem_shorthand]["name"] = "CLASS_model:fix_inter::door__door";
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ENTITY);
  }

  TEST_CASE("LevelGlbImportMakesDuplicateEntityNamesUnique") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/npc/spider_base/spider_base", -1, {1.0f, 2.0f, 3.0f}, {}, "spider"});
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/npc/spider_base/spider_base", -1, {4.0f, 5.0f, 6.0f}, {}, "other"});
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/fix_inter/marker/marker", -1, {7.0f, 8.0f, 9.0f}, {}, "spider_1"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t other_node = testNodeIndex(parsed, "arx_entity__001__other");
    REQUIRE(other_node < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][other_node]["name"] = "arx_entity__001__spider";
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.scene.entities.size() == 3);
    CHECK(dst.scene.entities[0].name == "spider");
    CHECK(dst.scene.entities[1].name == "spider_2");
    CHECK(dst.scene.entities[2].name == "spider_1");
  }

  TEST_CASE("LevelGlbExportUsesIdentityLocalRotationForUprightIdentityEntities") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src, {{4.0f, 5.0f, 6.0f}, pistoris::math::kIdentityQuat});
    src.scene.entities.push_back(
        {"graph/obj3d/interactive/fix_inter/door/door", -1, {7.0f, 8.0f, 9.0f}, pistoris::math::kIdentityQuat, "door"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);

    const std::size_t level_space = testNodeIndex(parsed, "level_space");
    REQUIRE(level_space < parsed.gltf["nodes"].size());
    CHECK_FALSE(parsed.gltf["nodes"][level_space].contains("rotation"));

    for (std::string_view name : {"arx_player_spawn__spawn", "arx_entity__000__door"}) {
      const std::size_t node = testNodeIndex(parsed, name);
      REQUIRE(node < parsed.gltf["nodes"].size());
      REQUIRE(parsed.gltf["nodes"][node]["rotation"].size() == 4);
      CHECK(parsed.gltf["nodes"][node]["rotation"][0].get<float>() == doctest::Approx(0.0f));
      CHECK(parsed.gltf["nodes"][node]["rotation"][1].get<float>() == doctest::Approx(0.0f));
      CHECK(parsed.gltf["nodes"][node]["rotation"][2].get<float>() == doctest::Approx(0.0f));
      CHECK(parsed.gltf["nodes"][node]["rotation"][3].get<float>() == doctest::Approx(1.0f));
    }

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(pistoris::math::quatToAngle(dst.scene.player_spawn.rotation).pitch == doctest::Approx(0.0f));
    REQUIRE(dst.scene.entities.size() == 1);
    CHECK(pistoris::math::quatToAngle(dst.scene.entities[0].rotation).pitch == doctest::Approx(0.0f));
  }

  TEST_CASE("LevelGlbImportNormalizesNodeQuaternionsAndRejectsNearZero") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src,
                         pistoris::PlayerSpawn{{4.0f, 5.0f, 6.0f}, pistoris::math::angleToQuat({10.0f, 20.0f, 30.0f})});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t spawn_index = testNodeIndex(parsed, "arx_player_spawn__spawn");
    REQUIRE(spawn_index < parsed.gltf["nodes"].size());
    for (auto& component : parsed.gltf["nodes"][spawn_index]["rotation"]) component = component.get<float>() * 2.0f;

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(pistoris::math::norm(dst.scene.player_spawn.rotation) == doctest::Approx(1.0f));
    CHECK(pistoris::math::quatToAngle(dst.scene.player_spawn.rotation).yaw == doctest::Approx(20.0f).epsilon(1.0e-4));

    const std::size_t original_vertex_count = dst.geometry.vertices.size();
    parsed.gltf["nodes"][spawn_index]["rotation"] = {0.0f, 0.0f, 0.0f, 1.0e-7f};
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);
    CHECK(dst.geometry.vertices.size() == original_vertex_count);
  }

  TEST_CASE("LevelGlbImportIgnoresPlayerSpawnScale") {
    pistoris::LevelModules src = makeSimpleLevel();
    setUsablePlayerSpawn(src,
                         pistoris::PlayerSpawn{{4.0f, 5.0f, 6.0f}, pistoris::math::angleToQuat({10.0f, 20.0f, 30.0f})});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t spawn_index = testNodeIndex(parsed, "arx_player_spawn__spawn");
    REQUIRE(spawn_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][spawn_index]["scale"] = {100.0f, 100.0f, 100.0f};

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.scene.player_spawn.position.x == doctest::Approx(4.0f));
    CHECK(pistoris::math::quatToAngle(dst.scene.player_spawn.rotation).yaw == doctest::Approx(20.0f).epsilon(1.0e-4));
    CHECK(logs.contains("player spawn node"));
    CHECK(logs.contains("nonidentity local scale; scale ignored"));

    parsed.gltf["nodes"][spawn_index]["scale"] = {-1.0f, 1.0f, 1.0f};
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_PLAYER_SPAWN);
  }

  TEST_CASE("LevelGlbImportHandlesEntityScaleAndRejectsReflection") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door", -1, {1.0f, 2.0f, 3.0f}, {}, "door"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t entity_index = testNodeIndex(parsed, "arx_entity__000__door");
    REQUIRE(entity_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][entity_index]["scale"] = {2.0f, 3.0f, 4.0f};

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.entities.size() == 1);
    CHECK(dst.scene.entities[0].position.x == doctest::Approx(1.0f));
    CHECK(logs.contains("nonidentity local scale; scale ignored"));

    parsed.gltf["nodes"][entity_index]["scale"] = {-1.0f, 1.0f, 1.0f};
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ENTITY);
  }

  TEST_CASE("LevelGlbImportWarnsWhenAnchorAndLightLocalScaleIsIgnored") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Anchor anchor;
    anchor.name = "scale_anchor";
    anchor.position = {0.25f, 0.0f, 0.25f};
    src.navigation.anchors.push_back(anchor);
    pistoris::Light light;
    light.name = "scale_light";
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t anchor_index = testNodeIndex(parsed, "arx_anchor__scale_anchor");
    const std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__scale_light");
    REQUIRE(anchor_index < parsed.gltf["nodes"].size());
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][anchor_index]["scale"] = {-1.0f, -1.0f, 1.0f};
    parsed.gltf["nodes"][light_index]["scale"] = {3.0f, 3.0f, 3.0f};

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(logs.contains("anchor node"));
    CHECK(logs.contains("light node"));
    CHECK(logs.contains("nonidentity local scale; scale ignored"));
  }

  TEST_CASE("LevelGlbImportValidatesEveryDuplicateEntityClassHelperAndSelectsOne") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door", -1, {1.0f, 2.0f, 3.0f}, {}, "door"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t entity_index = testNodeIndex(parsed, "arx_entity__000__door");
    REQUIRE(entity_index < parsed.gltf["nodes"].size());
    const std::size_t duplicate = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "CLASS_graph/obj3d/interactive/fix_inter/chest/chest__duplicate"}});
    parsed.gltf["nodes"][entity_index]["children"].push_back(duplicate);
    ParsedTestGlb malformed = parsed;
    malformed.gltf["nodes"][duplicate]["name"] = "CLASS___duplicate";

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.scene.entities.size() == 1);
    CHECK((dst.scene.entities[0].class_path == "graph/obj3d/interactive/fix_inter/door/door" ||
           dst.scene.entities[0].class_path == "graph/obj3d/interactive/fix_inter/chest/chest"));
    CHECK(importLevelGlb(writeTestGlb(std::move(malformed)), dst) == ARX_GLB_BAD_LEVEL_ENTITY);
  }

  TEST_CASE("LevelGlbImportAllowsEntityPreviewMeshWithoutImportingItAsGeometry") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door", -1, {1.0f, 2.0f, 3.0f}, {}, "door"});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t entity_index = testNodeIndex(parsed, "arx_entity__000__door");
    REQUIRE(entity_index < parsed.gltf["nodes"].size());
    std::optional<int> mesh;
    for (const nlohmann::json& node : parsed.gltf["nodes"]) {
      if (node.contains("mesh")) {
        mesh = node["mesh"].get<int>();
        break;
      }
    }
    REQUIRE(mesh.has_value());
    parsed.gltf["nodes"][entity_index]["mesh"] = *mesh;

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(dst.scene.entities.size() == 1);
    CHECK(dst.geometry.faces.size() == 1);
    CHECK_FALSE(logs.contains("unexpected descendants"));
    CHECK_FALSE(logs.contains("mesh discarded"));
  }

  TEST_CASE("LevelGlbRoundtripPreservesFogs") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Fog fog;
    fog.position = {1.0f, 2.0f, 3.0f};
    fog.color = {0.25f, 0.5f, 0.75f};
    fog.size = 30.0f;
    fog.directional = true;
    fog.scale = 0.5f;
    fog.rotation = pistoris::math::angleToQuat({15.0f, 25.0f, 0.0f});
    fog.speed = 4.0f;
    fog.rotate_speed = 1.5f;
    fog.lifetime_ms = 2000;
    fog.frequency = 500.0f;
    fog.name = "sewer_mist";
    src.scene.fogs.push_back(fog);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t parent_index = testNodeIndex(parsed, "fogs_parent");
    std::size_t fog_index = testNodeIndex(parsed, "arx_fog__sewer_mist");
    REQUIRE(parent_index < parsed.gltf["nodes"].size());
    REQUIRE(fog_index < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][parent_index]["translation"][1].get<float>() == doctest::Approx(-70.0f));
    REQUIRE(parsed.gltf["nodes"][fog_index]["children"].size() == 2);
    std::size_t settings = parsed.gltf["nodes"][fog_index]["children"][0].get<std::size_t>();
    std::size_t direction = parsed.gltf["nodes"][fog_index]["children"][1].get<std::size_t>();
    CHECK(parsed.gltf["nodes"][settings]["name"].get<std::string>().starts_with("SETTINGS__"));
    CHECK(parsed.gltf["nodes"][direction]["name"].get<std::string>().starts_with("DIRECTION__"));

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.scene.fogs.size() == 1);
    CHECK(dst.scene.fogs[0].name == "sewer_mist");
    CHECK(dst.scene.fogs[0].position.x == doctest::Approx(1.0f));
    CHECK(dst.scene.fogs[0].position.y == doctest::Approx(2.0f));
    CHECK(dst.scene.fogs[0].position.z == doctest::Approx(3.0f));
    CHECK(dst.scene.fogs[0].color.r == doctest::Approx(0.25f));
    CHECK(dst.scene.fogs[0].color.g == doctest::Approx(0.5f));
    CHECK(dst.scene.fogs[0].color.b == doctest::Approx(0.75f));
    CHECK(dst.scene.fogs[0].size == doctest::Approx(30.0f));
    CHECK(dst.scene.fogs[0].directional);
    CHECK(dst.scene.fogs[0].scale == doctest::Approx(0.5f));
    CHECK(dst.scene.fogs[0].speed == doctest::Approx(4.0f));
    CHECK(dst.scene.fogs[0].rotate_speed == doctest::Approx(1.5f));
    CHECK(dst.scene.fogs[0].lifetime_ms == 2000);
    CHECK(dst.scene.fogs[0].frequency == doctest::Approx(500.0f));

    const std::size_t duplicate_settings = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back(
        {{"name", "SETTINGS__RGB_1_0_0__SIZE_10__SCALE_1__SPEED_2__ROTATESPEED_3__LIFETIME_4__FREQUENCY_5__other"}});
    const std::size_t duplicate_direction = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back(parsed.gltf["nodes"][direction]);
    parsed.gltf["nodes"][duplicate_direction]["name"] = "DIRECTION__other";
    parsed.gltf["nodes"][fog_index]["children"].push_back(duplicate_settings);
    parsed.gltf["nodes"][fog_index]["children"].push_back(duplicate_direction);
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.fogs.size() == 1);
    CHECK((dst.scene.fogs[0].size == doctest::Approx(30.0f) || dst.scene.fogs[0].size == doctest::Approx(10.0f)));

    ParsedTestGlb malformed_settings = parsed;
    malformed_settings.gltf["nodes"][duplicate_settings]["name"] = "SETTINGS__UNKNOWN_1__other";
    CHECK(importLevelGlb(writeTestGlb(std::move(malformed_settings)), dst) == ARX_GLB_BAD_LEVEL_FOG);
    ParsedTestGlb malformed_direction = parsed;
    malformed_direction.gltf["nodes"][duplicate_direction]["name"] = "DIRECTION__extra__other";
    CHECK(importLevelGlb(writeTestGlb(std::move(malformed_direction)), dst) == ARX_GLB_BAD_LEVEL_FOG);

    parsed.gltf["nodes"][direction]["name"] = "DIRECTION__extra__sewer_mist";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_FOG);
  }

  TEST_CASE("LevelGlbDirectionalFogUsesLevelSpaceBasis") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Fog forward;
    forward.name = "forward";
    forward.position = {0.25f, 0.0f, 0.25f};
    forward.directional = true;
    forward.rotation = pistoris::math::kIdentityQuat;
    pistoris::Fog inclined = forward;
    inclined.name = "inclined";
    inclined.rotation = pistoris::math::angleToQuat({30.0f, 0.0f, 0.0f});
    src.scene.fogs = {forward, inclined};

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.scene.fogs.size() == src.scene.fogs.size());
    for (std::size_t i = 0; i < src.scene.fogs.size(); ++i) {
      const pistoris::ArxVector3 source = pistoris::math::rotate(src.scene.fogs[i].rotation, {0.0f, 0.0f, 1.0f});
      const pistoris::ArxVector3 imported = pistoris::math::rotate(dst.scene.fogs[i].rotation, {0.0f, 0.0f, 1.0f});
      CHECK(imported.x == doctest::Approx(source.x).epsilon(1.0e-4));
      CHECK(imported.y == doctest::Approx(source.y).epsilon(1.0e-4));
      CHECK(imported.z == doctest::Approx(source.z).epsilon(1.0e-4));
    }
  }

  TEST_CASE("LevelGlbRoundtripPreservesPaths") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.paths.push_back({"patrol",
                               {10.0f, 20.0f, 30.0f},
                               {{{}, pistoris::PathNodeType::kStandard, 0},
                                {{2.0f, 3.0f, 4.0f}, pistoris::PathNodeType::kBezier, 500},
                                {{5.0f, 6.0f, 7.0f}, pistoris::PathNodeType::kControlPoint, 750}}});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t parent_index = testNodeIndex(parsed, "paths_parent");
    std::size_t path_index = testNodeIndex(parsed, "arx_path__patrol");
    std::size_t start_index = testNodeIndex(parsed, "000__STANDARD__TIME_0__patrol");
    std::size_t turn_index = testNodeIndex(parsed, "001__BEZIER__TIME_500__patrol");
    std::size_t control_index = testNodeIndex(parsed, "002__CONTROL__TIME_750__patrol");
    REQUIRE(parent_index < parsed.gltf["nodes"].size());
    REQUIRE(path_index < parsed.gltf["nodes"].size());
    REQUIRE(start_index < parsed.gltf["nodes"].size());
    REQUIRE(turn_index < parsed.gltf["nodes"].size());
    REQUIRE(control_index < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][parent_index]["translation"][1].get<float>() == doctest::Approx(-60.0f));

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.paths.size() == 1);
    CHECK(dst.scene.paths[0].name == "patrol");
    CHECK(dst.scene.paths[0].position.x == doctest::Approx(10.0f));
    CHECK(dst.scene.paths[0].position.y == doctest::Approx(20.0f));
    CHECK(dst.scene.paths[0].position.z == doctest::Approx(30.0f));
    REQUIRE(dst.scene.paths[0].nodes.size() == 3);
    CHECK(dst.scene.paths[0].nodes[0].type == pistoris::PathNodeType::kStandard);
    CHECK(dst.scene.paths[0].nodes[0].time_ms == 0);
    CHECK(dst.scene.paths[0].nodes[1].type == pistoris::PathNodeType::kBezier);
    CHECK(dst.scene.paths[0].nodes[1].time_ms == 500);
    CHECK(dst.scene.paths[0].nodes[1].relative_position.x == doctest::Approx(2.0f));
    CHECK(dst.scene.paths[0].nodes[1].relative_position.y == doctest::Approx(3.0f));
    CHECK(dst.scene.paths[0].nodes[1].relative_position.z == doctest::Approx(4.0f));
    CHECK(dst.scene.paths[0].nodes[2].type == pistoris::PathNodeType::kControlPoint);
    CHECK(dst.scene.paths[0].nodes[2].time_ms == 750);

    parsed.gltf["nodes"][turn_index]["name"] = "001__BEZIER__TIME_500__anything.001";
    parsed.gltf["nodes"][control_index]["name"] = "002__CONTROL__TIME_750";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.paths[0].nodes.size() == 3);
    CHECK(dst.scene.paths[0].nodes[1].type == pistoris::PathNodeType::kBezier);
    CHECK(dst.scene.paths[0].nodes[2].type == pistoris::PathNodeType::kControlPoint);

    parsed.gltf["nodes"][turn_index]["name"] = "000__BEZIER__TIME_500__patrol";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_PATH);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][turn_index]["name"] = "turn";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_PATH);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][path_index]["scale"] = {2.0f, 2.0f, 2.0f};
    LogCapture logs;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.scene.paths[0].nodes[1].relative_position.x == doctest::Approx(4.0f));
    CHECK_FALSE(logs.contains("rotation or positive scale"));
  }

  TEST_CASE("LevelGlbImportRepairsDuplicatePathNames") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.scene.paths.push_back({"patrol_", {10.0f, 20.0f, 30.0f}, {{{}, pistoris::PathNodeType::kStandard, 0}}});
    src.scene.paths.push_back({"patrol_1", {40.0f, 50.0f, 60.0f}, {{{}, pistoris::PathNodeType::kStandard, 0}}});
    src.scene.paths.push_back({"other", {70.0f, 80.0f, 90.0f}, {{{}, pistoris::PathNodeType::kStandard, 0}}});

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t first_root = testNodeIndex(parsed, "arx_path__patrol_");
    const std::size_t second_root = testNodeIndex(parsed, "arx_path__patrol_1");
    const std::size_t third_root = testNodeIndex(parsed, "arx_path__other");
    const std::size_t first_point = testNodeIndex(parsed, "000__STANDARD__TIME_0__patrol_");
    REQUIRE(first_root < parsed.gltf["nodes"].size());
    REQUIRE(second_root < parsed.gltf["nodes"].size());
    REQUIRE(third_root < parsed.gltf["nodes"].size());
    REQUIRE(first_point < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][third_root]["name"] = "arx_path__patrol_";

    pistoris::LevelModules dst;
    LogCapture logs;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.paths.size() == 3);
    CHECK(dst.scene.paths[0].name == "patrol_");
    CHECK(dst.scene.paths[1].name == "patrol_1");
    CHECK(dst.scene.paths[2].name == "patrol_2");
    CHECK(dst.scene.paths[0].position.x == doctest::Approx(10.0f));
    CHECK(dst.scene.paths[1].position.x == doctest::Approx(40.0f));
    CHECK(dst.scene.paths[2].position.x == doctest::Approx(70.0f));
    CHECK(logs.contains("1 duplicate path name(s) renamed"));
  }

  TEST_CASE("LevelGlbRoundtripPreservesZonesAndSelfCrossingTopology") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone finite;
    finite.name = "hall";
    finite.perimeter_xz = {{0.0f, 0.0f}, {4.0f, 4.0f}, {0.0f, 4.0f}, {4.0f, 0.0f}};
    finite.reference_y = -1.0f;
    finite.height = 2.0f;
    finite.color = pistoris::ArxColor3{0.25f, 0.5f, 0.75f};
    finite.farclip = 1200.0f;
    finite.ambiance = pistoris::ZoneAmbiance{"ambient_cave_a", 80.0f};
    src.scene.zones.push_back(finite);

    pistoris::Zone infinite;
    infinite.name = "sewer";
    infinite.perimeter_xz = {{10.0f, 10.0f}, {12.0f, 10.0f}, {12.0f, 12.0f}, {10.0f, 12.0f}};
    infinite.height_mode = pistoris::ZoneHeightMode::kInfinite;
    src.scene.zones.push_back(infinite);

    LogCapture export_logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(export_logs.contains("zone 'hall' perimeter is not simple; using fan cap triangulation"));
    ParsedTestGlb parsed = parseTestGlb(glb);
    checkTestMaterial(parsed, "arx_zone", {0.65f, 0.20f, 0.90f, 0.25f}, "BLEND", true);
    std::size_t parent_index = testNodeIndex(parsed, "zones_parent");
    REQUIRE(parent_index < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][parent_index]["translation"][1].get<float>() == doctest::Approx(-50.0f));
    const std::size_t zone_index = testNodeIndex(parsed, "arx_zone__000__hall");
    REQUIRE(zone_index < parsed.gltf["nodes"].size());
    const std::size_t sewer_index = testNodeIndex(parsed, "arx_zone__001__sewer");
    REQUIRE(sewer_index < parsed.gltf["nodes"].size());
    std::size_t settings_index = testNodeIndex(parsed, "SETTINGS__RGB_0.25_0.5_0.75__FARCLIP_1200__VOLUME_80__hall");
    REQUIRE(settings_index < parsed.gltf["nodes"].size());
    const std::size_t ambiance_index = testNodeIndex(parsed, "AMBIANCE_ambient_cave_a__hall");
    REQUIRE(ambiance_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][settings_index]["name"] = "SETTINGS__VOLUME_80__RGB_0.25_0.5_0.75__FARCLIP_1200__different";

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.zones.size() == 2);
    CHECK(dst.scene.zones[0].name == "hall");
    CHECK(dst.scene.zones[0].height_mode == pistoris::ZoneHeightMode::kFinite);
    CHECK(dst.scene.zones[0].reference_y == doctest::Approx(-1.0f));
    CHECK(dst.scene.zones[0].height == doctest::Approx(2.0f));
    REQUIRE(dst.scene.zones[0].color.has_value());
    CHECK(dst.scene.zones[0].color->g == doctest::Approx(0.5f));
    REQUIRE(dst.scene.zones[0].farclip.has_value());
    CHECK(*dst.scene.zones[0].farclip == doctest::Approx(1200.0f));
    REQUIRE(dst.scene.zones[0].ambiance.has_value());
    CHECK(dst.scene.zones[0].ambiance->name == "ambient_cave_a");
    CHECK(dst.scene.zones[0].ambiance->volume == doctest::Approx(80.0f));
    REQUIRE(dst.scene.zones[0].perimeter_xz.size() == 4);
    CHECK(dst.scene.zones[0].perimeter_xz[0].x == doctest::Approx(dst.scene.zones[0].perimeter_xz[2].x));
    CHECK(dst.scene.zones[0].perimeter_xz[1].x == doctest::Approx(dst.scene.zones[0].perimeter_xz[3].x));
    CHECK(dst.scene.zones[1].name == "sewer");
    CHECK(dst.scene.zones[1].height_mode == pistoris::ZoneHeightMode::kInfinite);

    const std::size_t duplicate_settings = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "SETTINGS__RGB_1_0_0__FARCLIP_500__VOLUME_50__other"}});
    const std::size_t duplicate_ambiance = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "AMBIANCE_ambient_other__other"}});
    parsed.gltf["nodes"][zone_index]["children"].push_back(duplicate_settings);
    parsed.gltf["nodes"][zone_index]["children"].push_back(duplicate_ambiance);
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.scene.zones.size() == 2);
    CHECK((dst.scene.zones[0].farclip == std::optional<float>{1200.0f} ||
           dst.scene.zones[0].farclip == std::optional<float>{500.0f}));
    REQUIRE(dst.scene.zones[0].ambiance.has_value());
    CHECK((dst.scene.zones[0].ambiance->name == "ambient_cave_a" ||
           dst.scene.zones[0].ambiance->name == "ambient_other"));

    ParsedTestGlb malformed_settings = parsed;
    malformed_settings.gltf["nodes"][duplicate_settings]["name"] = "SETTINGS__UNKNOWN_1__other";
    CHECK(importLevelGlb(writeTestGlb(std::move(malformed_settings)), dst) == ARX_GLB_BAD_LEVEL_ZONE);
    ParsedTestGlb malformed_ambiance = parsed;
    malformed_ambiance.gltf["nodes"][duplicate_ambiance]["name"] = "AMBIANCE___other";
    CHECK(importLevelGlb(writeTestGlb(std::move(malformed_ambiance)), dst) == ARX_GLB_BAD_LEVEL_ZONE);
    ParsedTestGlb volume_without_ambiance = parsed;
    const std::size_t plain_settings = volume_without_ambiance.gltf["nodes"].size();
    volume_without_ambiance.gltf["nodes"].push_back({{"name", "SETTINGS__RGB_1_1_1__first"}});
    const std::size_t volume_settings = volume_without_ambiance.gltf["nodes"].size();
    volume_without_ambiance.gltf["nodes"].push_back({{"name", "SETTINGS__VOLUME_50__second"}});
    volume_without_ambiance.gltf["nodes"][sewer_index]["children"] = {plain_settings, volume_settings};
    CHECK(importLevelGlb(writeTestGlb(std::move(volume_without_ambiance)), dst) == ARX_GLB_BAD_LEVEL_ZONE);

    parsed.gltf["nodes"][settings_index]["name"] = "SETTINGS__RGB_0.25_0.5_0.75__RGB_1_1_1__hall";
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ZONE);
  }

  TEST_CASE("LevelGlbExportTriangulatesConcaveZoneCaps") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone zone;
    zone.name = "concave";
    zone.perimeter_xz = {{0.0f, 0.0f}, {4.0f, 0.0f}, {4.0f, 4.0f}, {2.0f, 1.0f}, {0.0f, 4.0f}};
    zone.reference_y = -1.0f;
    zone.height = 2.0f;
    src.scene.zones.push_back(zone);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t node = testNodeIndex(parsed, "arx_zone__000__concave");
    REQUIRE(node < parsed.gltf["nodes"].size());
    const std::vector<std::uint32_t> indices = testNodeIndices(parsed, node);
    REQUIRE(indices.size() == 48);

    double top_area = 0.0;
    for (std::size_t triangle = 0; triangle < 3; ++triangle) {
      const std::size_t offset = triangle * 6;
      const pistoris::ArxVector2& a = zone.perimeter_xz[indices[offset]];
      const pistoris::ArxVector2& b = zone.perimeter_xz[indices[offset + 1]];
      const pistoris::ArxVector2& c = zone.perimeter_xz[indices[offset + 2]];
      top_area += std::abs(pistoris::math::orient2d(a, b, c)) * 0.5;
    }
    CHECK(top_area == doctest::Approx(10.0));

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.scene.zones.size() == 1);
    CHECK(dst.scene.zones[0].perimeter_xz.size() == zone.perimeter_xz.size());
  }

  TEST_CASE("LevelGlbImportIgnoresZoneMaterials") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone zone;
    zone.name = "test";
    zone.perimeter_xz = {{0.0f, 0.0f}, {2.0f, 0.0f}, {0.0f, 2.0f}};
    zone.reference_y = -1.0f;
    zone.height = 1.0f;
    src.scene.zones.push_back(zone);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;
    auto zone_mesh = [](const ParsedTestGlb& parsed_glb) {
      const std::size_t zone_node = testNodeIndex(parsed_glb, "arx_zone__000__test");
      REQUIRE(zone_node < parsed_glb.gltf["nodes"].size());
      return parsed_glb.gltf["nodes"][zone_node]["mesh"].get<int>();
    };

    ParsedTestGlb parsed = parseTestGlb(glb);
    int mesh = zone_mesh(parsed);
    parsed.gltf["meshes"][mesh]["primitives"][0].erase("material");
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.scene.zones.size() == 1);

    parsed = parseTestGlb(glb);
    mesh = zone_mesh(parsed);
    int material = parsed.gltf["meshes"][mesh]["primitives"][0]["material"].get<int>();
    parsed.gltf["materials"][material]["name"] = "custom_zone_preview";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.scene.zones.size() == 1);

    parsed = parseTestGlb(glb);
    mesh = zone_mesh(parsed);
    material = parsed.gltf["meshes"][mesh]["primitives"][0]["material"].get<int>();
    parsed.gltf["materials"][material].erase("name");
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.scene.zones.size() == 1);
  }

  TEST_CASE("LevelGlbImportOrdersSparseZoneOrdinals") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone first_zone;
    first_zone.name = "first";
    first_zone.perimeter_xz = {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}};
    first_zone.reference_y = -1.0f;
    first_zone.height = 1.0f;
    src.scene.zones.push_back(first_zone);
    pistoris::Zone second_zone;
    second_zone.name = "second";
    second_zone.perimeter_xz = {{3.0f, 0.0f}, {5.0f, 0.0f}, {5.0f, 2.0f}};
    second_zone.reference_y = -1.0f;
    second_zone.height = 1.0f;
    src.scene.zones.push_back(second_zone);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t first = testNodeIndex(parsed, "arx_zone__000__first");
    std::size_t second = testNodeIndex(parsed, "arx_zone__001__second");
    REQUIRE(first < parsed.gltf["nodes"].size());
    REQUIRE(second < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][first]["name"] = "arx_zone__009__first";
    parsed.gltf["nodes"][second]["name"] = "arx_zone__002__second";

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.scene.zones.size() == 2);
    CHECK(dst.scene.zones[0].name == "second");
    CHECK(dst.scene.zones[1].name == "first");

    parsed = parseTestGlb(glb);
    first = testNodeIndex(parsed, "arx_zone__000__first");
    second = testNodeIndex(parsed, "arx_zone__001__second");
    parsed.gltf["nodes"][second]["name"] = "arx_zone__000__second";
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ZONE);
  }

  TEST_CASE("LevelGlbImportWeldsRenderSplitZoneVertices") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone zone;
    zone.name = "test";
    zone.perimeter_xz = {{0.0f, 0.0f}, {3.0f, 0.0f}, {3.0f, 2.0f}, {0.0f, 2.0f}};
    zone.reference_y = -1.0f;
    zone.height = 1.0f;
    src.scene.zones.push_back(zone);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t zone_node = testNodeIndex(parsed, "arx_zone__000__test");
    REQUIRE(zone_node < parsed.gltf["nodes"].size());
    splitNodePrimitivePositions(parsed, zone_node);
    int mesh_index = parsed.gltf["nodes"][zone_node]["mesh"].get<int>();
    int position_accessor_index =
        parsed.gltf["meshes"][mesh_index]["primitives"][0]["attributes"]["POSITION"].get<int>();
    CHECK(parsed.gltf["accessors"][position_accessor_index]["count"].get<std::size_t>() == 36);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.scene.zones.size() == 1);
    CHECK(dst.scene.zones[0].name == "test");
    CHECK(dst.scene.zones[0].perimeter_xz.size() == 4);
    CHECK(dst.scene.zones[0].height == doctest::Approx(1.0f));
  }

  TEST_CASE("LevelGlbImportFlattensZonePlanesAndRejectsMissingCaps") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Zone zone;
    zone.name = "hall";
    zone.perimeter_xz = {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
    zone.reference_y = -1.0f;
    zone.height = 2.0f;
    src.scene.zones.push_back(zone);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t zone_node = testNodeIndex(parsed, "arx_zone__000__hall");
    REQUIRE(zone_node < parsed.gltf["nodes"].size());
    int mesh = parsed.gltf["nodes"][zone_node]["mesh"].get<int>();
    int position_accessor = parsed.gltf["meshes"][mesh]["primitives"][0]["attributes"]["POSITION"].get<int>();
    const auto& position = parsed.gltf["accessors"][position_accessor];
    const auto& position_view = parsed.gltf["bufferViews"][position["bufferView"].get<int>()];
    std::size_t position_offset =
        position_view.value("byteOffset", 0U) + position.value("byteOffset", 0U) + sizeof(float);
    float moved_y = -0.99f;
    std::memcpy(parsed.bin.data() + position_offset, &moved_y, sizeof(moved_y));

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(logs.contains("zone 'hall' planes flattened"));

    int index_accessor = parsed.gltf["meshes"][mesh]["primitives"][0]["indices"].get<int>();
    parsed.gltf["accessors"][index_accessor]["count"] =
        parsed.gltf["accessors"][index_accessor]["count"].get<std::size_t>() - 3;
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ZONE);
  }

  TEST_CASE("LevelGlbImportCanonicalizesTheSourceQuadPortalDiagonal") {
    pistoris::LevelModules src;
    addPortalRooms(src);
    src.geometry.textures.push_back("graph/test.bmp");
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);
    addSecondRoomTriangle(src);
    pistoris::Portal portal;
    connectPortal(portal, "portal");
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
    src.rooms.portals.push_back(portal);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t portal_node = testNodeIndex(parsed, "arx_portal__room_1__room_2__portal");
    REQUIRE(portal_node < parsed.gltf["nodes"].size());
    int portal_mesh = parsed.gltf["nodes"][portal_node]["mesh"].get<int>();
    const auto& portal_primitive = parsed.gltf["meshes"][portal_mesh]["primitives"][0];
    const auto& accessor = parsed.gltf["accessors"][portal_primitive["indices"].get<int>()];
    const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
    std::size_t offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
    std::array<std::uint16_t, 6> alternate = {0, 1, 2, 0, 2, 3};
    std::memcpy(parsed.bin.data() + offset, alternate.data(), sizeof(alternate));

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.rooms.portals.size() == 1);
    CHECK(dst.rooms.portals[0].shape == pistoris::PortalShape::kQuad);
    CHECK(validateModules(dst) == ARX_OK);
  }

  TEST_CASE("LevelGlbExportSplitsRenderVerticesAndMaterialPrimitives") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures.push_back("graph/test.bmp");
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}}};
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {1, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.5f, 0.5f},
                                    {3, {0.0f, -1.0f, 0.0f}, 1.0f, 1.0f},
                                    {2, {0.0f, 1.0f, 0.0f}, 0.0f, 1.0f}}},
                                  0,
                                  pistoris::kFaceBitStone,
                                  0.25f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(parsed.gltf["meshes"][0]["primitives"].size() == 2);
    CHECK(testAttributeCount(parsed, "POSITION") == 6);
  }

  TEST_CASE("LevelGlbExportWritesPositionBounds") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    int position_accessor = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["POSITION"].get<int>();
    const auto& accessor = parsed.gltf["accessors"][position_accessor];
    CHECK(accessor["min"] == nlohmann::ordered_json::array({-0.5f, 0.0f, -0.5f}));
    CHECK(accessor["max"] == nlohmann::ordered_json::array({0.5f, 0.0f, 0.5f}));
  }

  TEST_CASE("LevelGlbExportUsesTransFlagForAlphaMode") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.faces[0].transval = 0.75f;
    LogCapture logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(logs.contains("alpha presence unknown for 1 external texture(s)"));

    ParsedTestGlb parsed = parseTestGlb(glb);
    const auto& opaque_material = parsed.gltf["materials"][0];
    CHECK(opaque_material.at("name") == "test");
    CHECK(opaque_material.value("alphaMode", std::string("OPAQUE")) == "OPAQUE");
    CHECK(testMaterialAlpha(opaque_material) == doctest::Approx(1.0f));

    src.geometry.faces[0].flags |= pistoris::kFaceBitTrans;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    const auto& transparent_material = parsed.gltf["materials"][0];
    REQUIRE_MESSAGE(transparent_material.contains("alphaMode"), parsed.gltf.dump());
    CHECK(transparent_material.at("name") == "test__TRANS__TRANSVAL_0.75");
    CHECK(transparent_material.at("alphaMode") == "BLEND");
    CHECK(testMaterialAlpha(transparent_material) == doctest::Approx(0.25f));
  }

  TEST_CASE("LevelGlbExportUsesTextureAlphaForCutout") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.textures[0].encoded_image = makeTestRgbaPng(255);
    REQUIRE_FALSE(src.geometry.textures[0].encoded_image.empty());

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const auto& cutout_material = parsed.gltf["materials"][0];
    CHECK(cutout_material.at("alphaMode") == "MASK");
    CHECK(cutout_material.value("alphaCutoff", 0.5f) == doctest::Approx(0.5f));
    CHECK(testMaterialAlpha(cutout_material) == doctest::Approx(1.0f));

    src.geometry.faces[0].flags = pistoris::kFaceBitTrans;
    src.geometry.faces[0].transval = 0.25f;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    const auto& blended_material = parsed.gltf["materials"][0];
    CHECK(blended_material.at("alphaMode") == "BLEND");
    CHECK(testMaterialAlpha(blended_material) == doctest::Approx(0.75f));

    src.geometry.faces[0].flags = 0;
    src.geometry.textures[0].encoded_image = makeTestBmp(0, 0, 0);
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    CHECK(parsed.gltf["materials"][0].at("alphaMode") == "MASK");
  }

  TEST_CASE("LevelGlbRoundtripPreservesActiveTransvalModes") {
    const std::array transvals = {std::numeric_limits<float>::lowest(),
                                  -1.0f,
                                  0.0f,
                                  std::numeric_limits<float>::denorm_min(),
                                  0.25f,
                                  1.0f,
                                  std::nextafter(1.0f, 2.0f),
                                  1.5f,
                                  2.0f,
                                  3.25f,
                                  std::numeric_limits<float>::max()};
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Face face = src.geometry.faces[0];
    face.flags = pistoris::kFaceBitTrans;
    src.geometry.faces.clear();
    src.rooms.face_rooms.clear();
    for (float transval : transvals) {
      face.transval = transval;
      src.geometry.faces.push_back(face);
      src.rooms.face_rooms.push_back(0);
    }

    LogCapture logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(logs.contains("9 transparent face(s) use nonstandard Arx blend modes"));

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t named_materials = 0;
    for (const nlohmann::json& material : parsed.gltf["materials"]) {
      std::string name = material.value("name", std::string{});
      if (!name.starts_with("test__TRANS__TRANSVAL_")) continue;
      ++named_materials;
      float alpha = testMaterialAlpha(material);
      if (name == "test__TRANS__TRANSVAL_0.25")
        CHECK(alpha == doctest::Approx(0.75f));
      else
        CHECK(alpha == doctest::Approx(1.0f));
    }
    CHECK(named_materials == transvals.size());

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == transvals.size());
    for (std::size_t i = 0; i < transvals.size(); ++i) CHECK(dst.geometry.faces[i].transval == transvals[i]);
  }

  TEST_CASE("LevelGlbRoundtripCanonicalizesNegativeZeroTransval") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.faces[0].flags = pistoris::kFaceBitTrans;
    src.geometry.faces[0].transval = -0.0f;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(parsed.gltf["materials"][0]["name"] == "test__TRANS__TRANSVAL_0");

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    CHECK(dst.geometry.faces[0].transval == 0.0f);
    CHECK_FALSE(std::signbit(dst.geometry.faces[0].transval));
  }

  TEST_CASE("LevelGlbImportUsesAlphaFallbackWithoutTransvalName") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    auto& material = parsed.gltf["materials"][0];
    material["name"] = "test__TRANS";
    material["alphaMode"] = "BLEND";
    material["pbrMetallicRoughness"]["baseColorFactor"][3] = 0.25f;

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    CHECK(dst.geometry.faces[0].transval == doctest::Approx(0.75f));
  }

  TEST_CASE("LevelGlbExportChecksOnlyReferencedTextureStems") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.textures.push_back("graph/no_tex.bmp");

    LogCapture logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(logs.contains("unused texture omitted: graph/no_tex.bmp"));

    src.geometry.faces[0].texture = 1;
    CHECK(exportLevelGlb(src, glb) == ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM);

    for (std::string_view reserved : {"arx_zone", "arx_nav_surface"}) {
      src.geometry.textures[1].path = std::string("graph/") + std::string(reserved) + ".bmp";
      CHECK(exportLevelGlb(src, glb) == ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM);
    }
  }

  TEST_CASE("LevelGlbImportRejectsPreviewMaterialsOnOrdinaryGeometry") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    for (std::string_view reserved : {"arx_zone", "arx_nav_surface"}) {
      ParsedTestGlb parsed = parseTestGlb(glb);
      parsed.gltf["materials"][0]["name"] = reserved;
      pistoris::LevelModules dst;
      CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM);
    }
  }

  TEST_CASE("LevelGlbExportEmbedsTextureImagesAndConvertsBmpToPng") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.textures[0].encoded_image = makeTestBmp();

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    REQUIRE(parsed.gltf["images"].size() == 1);
    const auto& image = parsed.gltf["images"][0];
    CHECK(image.at("name") == "test.png");
    CHECK(image.at("mimeType") == "image/png");
    CHECK_FALSE(image.contains("uri"));
    std::span<const std::uint8_t> embedded = testBufferView(parsed, image.at("bufferView").get<std::size_t>());
    pistoris::geometry::ImageInfo info;
    REQUIRE(pistoris::geometry::inspectImage(embedded, &info) == pistoris::geometry::ImageError::kNone);
    CHECK(info.format == pistoris::geometry::ImageFormat::kPng);

    pistoris::LevelModules imported;
    REQUIRE(importLevelGlb(glb, imported) == ARX_OK);
    REQUIRE(imported.geometry.textures.size() == 1);
    CHECK(imported.geometry.textures[0].path == "test.png");
    CHECK(imported.geometry.textures[0].encoded_image == std::vector<std::uint8_t>(embedded.begin(), embedded.end()));

    src.geometry.textures[0] = imported.geometry.textures[0];
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    const auto& pass_through_image = parsed.gltf["images"][0];
    std::span<const std::uint8_t> pass_through =
        testBufferView(parsed, pass_through_image.at("bufferView").get<std::size_t>());
    CHECK(std::vector<std::uint8_t>(pass_through.begin(), pass_through.end()) ==
          imported.geometry.textures[0].encoded_image);
  }

  TEST_CASE("LevelGlbImportAcceptsBase64ImageDataUris") {
    std::vector<std::uint8_t> png;
    REQUIRE(pistoris::geometry::transcodeImageToPng(makeTestBmp(), png) == pistoris::geometry::ImageError::kNone);

    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["images"][0].erase("bufferView");
    parsed.gltf["images"][0].erase("mimeType");
    parsed.gltf["images"][0]["uri"] = "data:image/png;base64," + testBase64(png);
    glb = writeTestGlb(std::move(parsed));

    pistoris::LevelModules imported;
    REQUIRE(importLevelGlb(glb, imported) == ARX_OK);
    REQUIRE(imported.geometry.textures.size() == 1);
    CHECK(imported.geometry.textures[0].path == "test.png");
    CHECK(imported.geometry.textures[0].encoded_image == png);
  }

  TEST_CASE("LevelGlbImportAcceptsPercentEncodedImagesAndRejectsMismatchedMediaTypes") {
    std::vector<std::uint8_t> png;
    REQUIRE(pistoris::geometry::transcodeImageToPng(makeTestBmp(), png) == pistoris::geometry::ImageError::kNone);

    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["images"][0]["name"] = "unrelated.png";
    parsed.gltf["images"][0].erase("bufferView");
    parsed.gltf["images"][0].erase("mimeType");
    parsed.gltf["images"][0]["uri"] = "data:image/png," + testPercentEncoding(png);
    glb = writeTestGlb(std::move(parsed));

    pistoris::LevelModules imported;
    REQUIRE(importLevelGlb(glb, imported) == ARX_OK);
    REQUIRE(imported.geometry.textures.size() == 1);
    CHECK(imported.geometry.textures[0].path == "unrelated.png");
    CHECK(imported.geometry.textures[0].encoded_image == png);

    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    parsed.gltf["images"][0].erase("bufferView");
    parsed.gltf["images"][0].erase("mimeType");
    parsed.gltf["images"][0]["uri"] = "data:image/jpeg;base64," + testBase64(png);
    glb = writeTestGlb(std::move(parsed));
    CHECK(importLevelGlb(glb, imported) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("LevelGlbImportAndExportPreserveJpegBytes") {
    constexpr std::array<std::uint8_t, 3> kRed = {255, 0, 0};
    std::vector<std::uint8_t> jpeg;
    REQUIRE(stbi_write_jpg_to_func(appendTestImage, &jpeg, 1, 1, 3, kRed.data(), 90) != 0);
    pistoris::geometry::ImageInfo info;
    REQUIRE(pistoris::geometry::inspectImage(jpeg, &info) == pistoris::geometry::ImageError::kNone);
    REQUIRE(info.format == pistoris::geometry::ImageFormat::kJpeg);

    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["images"][0]["name"] = "test.jpeg";
    parsed.gltf["images"][0]["uri"] = "data:image/jpeg;base64," + testBase64(jpeg);
    glb = writeTestGlb(std::move(parsed));

    pistoris::LevelModules imported;
    REQUIRE(importLevelGlb(glb, imported) == ARX_OK);
    REQUIRE(imported.geometry.textures.size() == 1);
    CHECK(imported.geometry.textures[0].path == "test.jpg");
    REQUIRE(pistoris::geometry::inspectImage(imported.geometry.textures[0].encoded_image, &info) ==
            pistoris::geometry::ImageError::kNone);
    CHECK(info.format == pistoris::geometry::ImageFormat::kJpeg);

    src.geometry.textures[0] = imported.geometry.textures[0];
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    const auto& image = parsed.gltf["images"][0];
    CHECK(image.at("name") == "test.jpg");
    CHECK(image.at("mimeType") == "image/jpeg");
    std::span<const std::uint8_t> embedded = testBufferView(parsed, image.at("bufferView").get<std::size_t>());
    CHECK(std::vector<std::uint8_t>(embedded.begin(), embedded.end()) == imported.geometry.textures[0].encoded_image);
  }

  TEST_CASE("LevelGlbExportSharesIdenticalImagesAndRejectsConflictingPayloads") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.textures.push_back(src.geometry.textures[0]);
    src.geometry.textures[0].encoded_image = makeTestBmp();
    src.geometry.textures[1].encoded_image = src.geometry.textures[0].encoded_image;
    src.geometry.faces.push_back(src.geometry.faces[0]);
    src.geometry.faces.back().texture = 1;
    src.geometry.faces.back().flags = pistoris::kFaceBitStone;
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(parsed.gltf["images"].size() == 1);
    CHECK(parsed.gltf["textures"].size() == 1);
    CHECK(parsed.gltf["materials"].size() == 3);

    src.geometry.textures[1].encoded_image = makeTestTga();
    CHECK(exportLevelGlb(src, glb) == ARX_GLB_BAD_LEVEL_MATERIAL_STEM_COLLISION);
  }

  TEST_CASE("LevelGlbExportGroupsMaterialsByEffectiveTextureIdentity") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.textures.push_back(src.geometry.textures[0]);
    src.geometry.faces.push_back(src.geometry.faces[0]);
    src.geometry.faces.back().texture = 1;
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    REQUIRE(parsed.gltf["meshes"][0]["primitives"].size() == 1);
    std::size_t matching_materials = 0;
    for (const nlohmann::json& material : parsed.gltf["materials"])
      if (material.value("name", std::string{}) == "test") ++matching_materials;
    CHECK(matching_materials == 1);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 2);
    REQUIRE(dst.geometry.textures.size() == 1);
    CHECK(dst.geometry.faces[0].texture == dst.geometry.faces[1].texture);
  }

  TEST_CASE("LevelGlbExportRejectsConflictingSanitizedTextureStems") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.textures = {"a/foo_.bmp", "b/foo.bmp"};
    src.geometry.vertices.push_back({{1.0f, 0.0f, 1.0f}});
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{1, normal, 0.0f, 0.0f}, {3, normal, 1.0f, 1.0f}, {2, normal, 0.0f, 1.0f}}}, 1, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    CHECK(exportLevelGlb(src, glb) == ARX_GLB_BAD_LEVEL_MATERIAL_STEM_COLLISION);
  }

  TEST_CASE("LevelGlbExportReportsNonstandardTransvalPreview") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.geometry.vertices.push_back({{1.0f, 0.0f, 1.0f}});
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces[0].flags = pistoris::kFaceBitTrans;
    src.geometry.faces[0].transval = -1.0f;
    src.geometry.faces.push_back({{{{1, normal, 0.0f, 0.0f}, {3, normal, 1.0f, 1.0f}, {2, normal, 0.0f, 1.0f}}},
                                  0,
                                  pistoris::kFaceBitTrans,
                                  -1.0f});
    src.rooms.face_rooms.push_back(0);

    LogCapture logs;
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(logs.contains("2 transparent face(s) use nonstandard Arx blend modes"));
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(parsed.gltf["materials"][0]["name"] == "test__TRANS__TRANSVAL_-1");
    CHECK(testMaterialAlpha(parsed.gltf["materials"][0]) == doctest::Approx(1.0f));
  }

  TEST_CASE("LevelRenderSplitsDebugGlbPreservesTopologyAndMarksRenderBoundaries") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"graph/a.bmp", "graph/b.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}},
                             {{1.0f, 0.0f, 0.0f}},
                             {{0.0f, 0.0f, 1.0f}},
                             {{1.0f, 0.0f, 1.0f}},
                             {{0.0f, 0.0f, 0.0f}},
                             {{2.0f, 0.0f, 0.0f}},
                             {{2.0f, 0.0f, 1.0f}}};
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {1, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back({{{{0, {1.0f, 0.0f, 0.0f}, 0.5f, 0.5f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f},
                                    {3, {0.0f, -1.0f, 0.0f}, 1.0f, 1.0f}}},
                                  0,
                                  pistoris::kFaceBitStone,
                                  0.25f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back({{{{1, {0.0f, -1.0f, 0.0f}, 0.75f, 0.75f},
                                    {3, {0.0f, -1.0f, 0.0f}, 1.0f, 1.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 1.0f}}},
                                  1,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back({{{{4, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {5, {0.0f, -1.0f, 0.0f}, 1.0f, 0.0f},
                                    {6, {0.0f, -1.0f, 0.0f}, 1.0f, 1.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportRenderSplitsDebugGlb(src, 0.0f, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const auto& attributes = parsed.gltf["meshes"][0]["primitives"][0]["attributes"];

    CHECK(parsed.gltf["meshes"].size() == 1);
    CHECK(parsed.gltf["meshes"][0]["primitives"].size() == 1);
    CHECK(parsed.gltf["accessors"][attributes["POSITION"].get<int>()]["count"] == src.geometry.vertices.size());
    CHECK(testIndices(parsed) == std::vector<std::uint32_t>{0, 1, 2, 0, 2, 3, 1, 3, 2, 4, 5, 6});

    auto position = testFloatAttribute(parsed, "_POSITION_SPLIT");
    auto normals = testFloatAttribute(parsed, "_NORMALS_SPLIT");
    auto uv = testFloatAttribute(parsed, "_UV_SPLIT");
    auto texture = testFloatAttribute(parsed, "_TEXTURE_BOUNDARY");
    auto flags = testFloatAttribute(parsed, "_FLAGS_BOUNDARY");
    auto transval = testFloatAttribute(parsed, "_TRANSVAL_BOUNDARY");

    checkColor(position, 0, 1.0f, 1.0f, 0.0f);
    checkColor(position, 4, 1.0f, 1.0f, 0.0f);
    checkColor(normals, 0, 1.0f, 0.0f, 0.0f);
    checkColor(uv, 0, 0.0f, 1.0f, 0.0f);
    checkColor(texture, 1, 0.0f, 0.0f, 1.0f);
    checkColor(uv, 1, 0.0f, 0.0f, 0.0f);
    checkColor(flags, 0, 1.0f, 0.0f, 1.0f);
    checkColor(transval, 0, 0.0f, 1.0f, 1.0f);

    auto display_normals = testFloatAttribute(parsed, "NORMAL");
    REQUIRE(display_normals.size() >= 3);
    CHECK(display_normals[0] == doctest::Approx(0.7071067f));
    CHECK(display_normals[1] == doctest::Approx(0.7071067f));
    CHECK(display_normals[2] == doctest::Approx(0.0f));
  }

  TEST_CASE("LevelRenderSplitsDebugGlbUsesFirstNormalWhenAverageCancels") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"graph/test.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}}};
    src.geometry.faces.push_back({{{{0, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {1, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f},
                                    {2, {0.0f, -1.0f, 0.0f}, 0.0f, 0.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back({{{{0, {0.0f, 1.0f, 0.0f}, 0.0f, 0.0f},
                                    {2, {0.0f, 1.0f, 0.0f}, 0.0f, 0.0f},
                                    {3, {0.0f, 1.0f, 0.0f}, 0.0f, 0.0f}}},
                                  0,
                                  0,
                                  0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportRenderSplitsDebugGlb(src, 0.0f, glb) == ARX_OK);
    auto normals = testFloatAttribute(parseTestGlb(glb), "NORMAL");
    CHECK(normals[0] == doctest::Approx(0.0f));
    CHECK(normals[1] == doctest::Approx(1.0f));
    CHECK(normals[2] == doctest::Approx(0.0f));
  }

  TEST_CASE("LevelRenderSplitsDebugGlbHandlesLargeFiniteYCoordinates") {
    constexpr float kLargeY = 1.0e15f;
    pistoris::LevelModules src = makeSimpleLevel();
    for (pistoris::Vertex& vertex : src.geometry.vertices) vertex.position.y = kLargeY;

    std::vector<std::uint8_t> glb;
    CHECK(exportRenderSplitsDebugGlb(src, 0.0f, glb) == ARX_OK);
  }

  TEST_CASE("LevelGlbExportPreservesExactCornerNormals") {
    std::array normals = {normalAtDegrees(0.0f), normalAtDegrees(2.0f), normalAtDegrees(4.0f)};
    pistoris::LevelModules src = makeNormalFan(normals);

    std::vector<std::uint8_t> regular_glb;
    REQUIRE(exportLevelGlb(src, regular_glb) == ARX_OK);
    ParsedTestGlb regular = parseTestGlb(regular_glb);
    CHECK(testAttributeCount(regular, "POSITION") == 9);
    auto regular_normals = testFloatAttribute(regular, "NORMAL");
    CHECK(regular_normals[0] == doctest::Approx(normalAtDegrees(0.0f).x).epsilon(1.0e-5));
    CHECK(regular_normals[1] == doctest::Approx(-normalAtDegrees(0.0f).y).epsilon(1.0e-5));

    std::vector<std::uint8_t> debug_glb;
    REQUIRE(exportRenderSplitsDebugGlb(src, 5.0f, debug_glb) == ARX_OK);
    checkColor(testFloatAttribute(parseTestGlb(debug_glb), "_NORMALS_SPLIT"), 0, 0.0f, 0.0f, 0.0f);
  }

  TEST_CASE("LevelRenderSplitsDebugDoesNotBridgePastFiveDegrees") {
    std::array normals = {normalAtDegrees(0.0f), normalAtDegrees(4.0f), normalAtDegrees(8.0f)};
    pistoris::LevelModules src = makeNormalFan(normals);

    std::vector<std::uint8_t> regular_glb;
    REQUIRE(exportLevelGlb(src, regular_glb) == ARX_OK);
    CHECK(testAttributeCount(parseTestGlb(regular_glb), "POSITION") == 9);

    std::vector<std::uint8_t> debug_glb;
    REQUIRE(exportRenderSplitsDebugGlb(src, 5.0f, debug_glb) == ARX_OK);
    checkColor(testFloatAttribute(parseTestGlb(debug_glb), "_NORMALS_SPLIT"), 0, 1.0f, 0.0f, 0.0f);
  }

  TEST_CASE("LevelGlbExportKeepsSeparatedCornerNormalGroups") {
    std::array normals = {normalAtDegrees(0.0f), normalAtDegrees(2.0f), normalAtDegrees(90.0f), normalAtDegrees(92.0f)};
    pistoris::LevelModules src = makeNormalFan(normals);

    std::vector<std::uint8_t> regular_glb;
    REQUIRE(exportLevelGlb(src, regular_glb) == ARX_OK);
    CHECK(testAttributeCount(parseTestGlb(regular_glb), "POSITION") == 12);
  }

  TEST_CASE("LevelGlbExportDoesNotAverageRepeatedCornerNormals") {
    std::array normals = {normalAtDegrees(0.0f), normalAtDegrees(4.0f), normalAtDegrees(4.0f), normalAtDegrees(4.0f)};
    pistoris::LevelModules src = makeNormalFan(normals);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    auto emitted = testFloatAttribute(parseTestGlb(glb), "NORMAL");
    CHECK(emitted[0] == doctest::Approx(normalAtDegrees(0.0f).x).epsilon(1.0e-5));
    CHECK(emitted[1] == doctest::Approx(-normalAtDegrees(0.0f).y).epsilon(1.0e-5));
  }

  TEST_CASE("LevelNormalClusterDebugDisplayIsOrderIndependent") {
    std::array first = {normalAtDegrees(0.0f), normalAtDegrees(4.0f), normalAtDegrees(8.0f)};
    std::array second = {normalAtDegrees(8.0f), normalAtDegrees(0.0f), normalAtDegrees(4.0f)};
    pistoris::LevelModules a = makeNormalFan(first);
    pistoris::LevelModules b = makeNormalFan(second);

    std::vector<std::uint8_t> a_glb;
    std::vector<std::uint8_t> b_glb;
    REQUIRE(exportRenderSplitsDebugGlb(a, 5.0f, a_glb) == ARX_OK);
    REQUIRE(exportRenderSplitsDebugGlb(b, 5.0f, b_glb) == ARX_OK);
    auto a_normals = testFloatAttribute(parseTestGlb(a_glb), "NORMAL");
    auto b_normals = testFloatAttribute(parseTestGlb(b_glb), "NORMAL");
    CHECK(a_normals[0] == doctest::Approx(b_normals[0]));
    CHECK(a_normals[1] == doctest::Approx(b_normals[1]));
    CHECK(a_normals[2] == doctest::Approx(b_normals[2]));
  }

  TEST_CASE("LevelValidationUsesSpecificGeometryErrorCodes") {
    pistoris::LevelModules empty_level;
    CHECK(validateModules(empty_level) == ARX_LEVEL_NO_GEOMETRY);

    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.vertices[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_VERTEX_POSITION);

    level = makeSimpleLevel();
    level.geometry.faces[0].texture = 1;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FACE_TEXTURE);

    level = makeSimpleLevel();
    level.geometry.faces[0].flags = 1U << 31;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FACE_TYPE);

    level = makeSimpleLevel();
    level.geometry.faces[0].transval = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FACE_TRANSVAL);

    level = makeSimpleLevel();
    level.geometry.faces[0].corners[2].vertex = 3;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FACE_VERTEX);

    level = makeSimpleLevel();
    level.geometry.faces[0].corners[0].normal = {};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FACE_NORMAL);

    level = makeSimpleLevel();
    level.geometry.faces[0].corners[0].u = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FACE_UV);

    level = makeSimpleLevel();
    level.geometry.vertices[2].position = level.geometry.vertices[1].position;
    CHECK(validateModules(level) == ARX_LEVEL_DEGENERATE_FACE);
  }

  TEST_CASE("LevelBakedColorsDefaultToNeutralGrayOnExport") {
    pistoris::LevelModules level = makeSimpleLevel();
    CHECK(level.lighting.corner_colors.empty());
    CHECK(validateModules(level) == ARX_OK);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(level, glb) == ARX_OK);
    std::vector<float> colors = testFloatAttribute(parseTestGlb(glb), "COLOR_0");
    REQUIRE(colors.size() == 9);
    for (float value : colors) CHECK(value == doctest::Approx(0.5f));
  }

  TEST_CASE("LevelValidationBoundsIgnoreUnreferencedVertices") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.geometry.vertices.push_back({{10000.0f, -5000.0f, 7000.0f}});

    pistoris::ArxAabb referenced_bounds;
    REQUIRE(validateModules(level, nullptr, &referenced_bounds) == ARX_OK);
    CHECK(referenced_bounds.min.x == 0.0f);
    CHECK(referenced_bounds.min.y == 0.0f);
    CHECK(referenced_bounds.min.z == 0.0f);
    CHECK(referenced_bounds.max.x == 1.0f);
    CHECK(referenced_bounds.max.y == 0.0f);
    CHECK(referenced_bounds.max.z == 1.0f);
  }

  TEST_CASE("LevelValidationClearsBoundsCacheOnFailure") {
    pistoris::Level level;
    REQUIRE(test::addRoom(level, {"room"}) == 0);

    test::MeshSnapshot mesh;
    const pistoris::LevelModules source = makeSimpleLevel();
    mesh.vertices = source.geometry.vertices;
    mesh.faces = source.geometry.faces;
    mesh.textures = source.geometry.textures;
    mesh.face_rooms = source.rooms.face_rooms;
    REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);

    REQUIRE(level.validate() == ARX_OK);
    REQUIRE(level.bounds().has_value());
    REQUIRE(level.referencedBounds().has_value());

    level.clearMesh();
    CHECK(level.validate() == ARX_LEVEL_NO_GEOMETRY);
    CHECK_FALSE(level.bounds().has_value());
    CHECK_FALSE(level.referencedBounds().has_value());
  }

  TEST_CASE("LevelValidationUsesSpecificPortalAndAnchorErrorCodes") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.rooms.definitions.push_back({"room_2"});
    level.rooms.portals.push_back({});
    connectPortal(level.rooms.portals[0], "portal");
    level.rooms.portals[0].shape = static_cast<pistoris::PortalShape>(5);
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PORTAL_SHAPE);

    level = makeSimpleLevel();
    level.rooms.definitions.push_back({"room_2"});
    level.rooms.portals.push_back({});
    connectPortal(level.rooms.portals[0], "portal");
    level.rooms.portals[0].vertices[1] = level.rooms.portals[0].vertices[0];
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PORTAL_VERTEX);

    level = makeSimpleLevel();
    level.rooms.definitions.push_back({"room_2"});
    level.rooms.portals.push_back({});
    connectPortal(level.rooms.portals[0], "portal");
    level.rooms.portals[0].shape = pistoris::PortalShape::kTriangle;
    level.rooms.portals[0].vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {}}};
    CHECK(validateModules(level) == ARX_LEVEL_DEGENERATE_PORTAL);

    level = makeSimpleLevel();
    level.rooms.definitions.push_back({"room_2"});
    level.rooms.portals.push_back({});
    connectPortal(level.rooms.portals[0], "portal");
    level.rooms.portals[0].vertices = {
        {{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 2.0f}, {1.0f, 0.0f, 0.0f}}};
    CHECK(validateModules(level) == ARX_LEVEL_SELF_INTERSECTING_PORTAL);

    level = makeSimpleLevel();
    level.rooms.definitions.push_back({"room_2"});
    level.rooms.portals.push_back({});
    connectPortal(level.rooms.portals[0], "portal");
    level.rooms.portals[0].vertices = {
        {{0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, {200.0f, 300.0f, 3.0f}, {0.0f, 300.0f, 0.0f}}};
    CHECK(validateModules(level) == ARX_OK);

    level.rooms.portals[0].vertices[2].z = 3.5f;
    CHECK(validateModules(level) == ARX_LEVEL_NON_PLANAR_PORTAL);

    level = makeSimpleLevel();
    level.navigation.anchors.push_back({{0.0f, 0.0f, 0.0f}, -1.0f, 1.0f, 0, {}});
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_RADIUS);

    level = makeSimpleLevel();
    level.navigation.anchors.push_back({{0.0f, 0.0f, 0.0f}, 1.0f, -2.0f, 0, {}});
    CHECK(validateModules(level) == ARX_OK);

    level.navigation.anchors[0].flags = pistoris::kAnchorFlagBlocked;
    CHECK(validateModules(level) == ARX_OK);

    level.navigation.anchors[0].flags = static_cast<std::int16_t>(1 << 2);
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_FLAGS);

    level = makeSimpleLevel();
    level.navigation.anchors.push_back({{0.0f, 0.0f, 0.0f}, 1.0f, 2.0f, 0, {}});
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_HEIGHT);

    level = makeSimpleLevel();
    level.navigation.anchors.push_back({{2.0f, 0.0f, 0.0f}, 0.0f, 0.0f, 0, {}});
    CHECK(validateModules(level) == ARX_OK);

    level.navigation.anchors[0].position.x = -0.01f;
    CHECK(validateModules(level) == ARX_LEVEL_ANCHOR_OUT_OF_BOUNDS);

    level = makeSimpleLevel();
    level.navigation.anchors.push_back({{0.0f, 0.0f, 0.0f}, 1.0f, -2.0f, 0, {}});
    level.navigation.anchors.push_back({{0.5f, 0.0f, 0.0f}, 1.0f, -2.0f, 0, {}});
    level.navigation.connections.push_back({0, 1});
    CHECK(validateModules(level) == ARX_OK);

    level.navigation.connections[0] = {1, 0};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER);

    level.navigation.connections[0] = {0, 0};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER);

    level.navigation.connections[0] = {0, 2};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_CONNECTION_INDEX);

    level.navigation.connections = {{0, 1}, {0, 1}};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER);

    level = makeSimpleLevel();
    level.navigation.surface = pistoris::NavSurface{
        {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}},
        {{{{0, 1, 2}}}},
    };
    CHECK(validateModules(level) == ARX_OK);

    level.navigation.surface->triangles[0].vertices = {{0, 1, 3}};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_NAV_SURFACE_TRIANGLE);
  }

  TEST_CASE("LevelValidationUsesSpecificLightErrorCodes") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_NAME);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_POSITION);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].color.r = -0.01f;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_COLOR);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].fallstart = 2.0f;
    level.lighting.lights[0].fallend = 1.0f;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_FALLOFF);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].intensity = -1.0f;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_INTENSITY);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].effect_speed = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_EFFECT);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].flags = pistoris::kLightFlagsAll | 0x1000U;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_FLAGS);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].fallstart = 1.0f;
    level.lighting.lights[0].fallend = 1.0f;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_LIGHT_FALLOFF);

    level = makeSimpleLevel();
    level.lighting.lights.push_back({});
    level.lighting.lights[0].name = "light";
    level.lighting.lights[0].fallstart = 0.0f;
    level.lighting.lights[0].fallend = 0.0f;
    CHECK(validateModules(level) == ARX_OK);

    level = makeSimpleLevel();
    level.lighting.corner_colors = {{1.01f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f}};
    CHECK(validateModules(level) == ARX_LEVEL_BAD_CORNER_COLOR);
  }

  TEST_CASE("LevelValidationChecksPlayerSpawnEntitiesAndFogs") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.scene.player_spawn = pistoris::PlayerSpawn{};
    CHECK(validateModules(level) == ARX_OK);

    level.scene.player_spawn.rotation.x = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PLAYER_SPAWN);

    level = makeSimpleLevel();
    level.scene.entities.push_back({"graph/obj3d/interactive/items/torch", -1, {}, {}, "torch"});
    CHECK(validateModules(level) == ARX_OK);

    level.scene.entities[0].class_path = "Graph\\Obj3D\\Interactive\\Items\\Torch.teo";
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ENTITY_CLASS_PATH);

    level.scene.entities[0].class_path = "graph/obj3d/interactive/items/torch";
    level.scene.entities[0].position.x = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ENTITY_POSITION);

    level.scene.entities[0].position = {};
    level.scene.entities[0].rotation.z = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ENTITY_ROTATION);

    level = makeSimpleLevel();
    level.scene.fogs.push_back({});
    level.scene.fogs[0].position.z = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FOG_POSITION);

    level.scene.fogs[0].position = {};
    level.scene.fogs[0].size = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_FOG_EFFECT);
  }

  TEST_CASE("LevelValidationChecksZones") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.scene.zones.push_back({"zone",
                                 {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}},
                                 0.0f,
                                 pistoris::ZoneHeightMode::kFinite,
                                 3.0f,
                                 pistoris::ArxColor3{0.5f, 0.6f, 0.7f},
                                 500.0f,
                                 pistoris::ZoneAmbiance{"ambient_cave_a", 80.0f}});
    CHECK(validateModules(level) == ARX_OK);

    level.scene.zones[0].name.clear();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_NAME);

    level.scene.zones[0].name = "zone";
    level.scene.zones[0].perimeter_xz[1] = level.scene.zones[0].perimeter_xz[0];
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_PERIMETER);

    level.scene.zones[0].perimeter_xz = {{0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 2.0f}, {0.0f, 2.0f}};
    level.scene.zones[0].height_mode = static_cast<pistoris::ZoneHeightMode>(10);
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_HEIGHT_MODE);

    level.scene.zones[0].height_mode = pistoris::ZoneHeightMode::kFinite;
    level.scene.zones[0].height = 0.0f;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_HEIGHT);

    level.scene.zones[0].height = 3.0f;
    level.scene.zones[0].color->r = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_COLOR);

    level.scene.zones[0].color = pistoris::ArxColor3{};
    level.scene.zones[0].farclip = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_FARCLIP);

    level.scene.zones[0].farclip = 500.0f;
    level.scene.zones[0].ambiance->name = "ambient_cave_a.amb";
    CHECK(validateModules(level) == ARX_LEVEL_BAD_ZONE_AMBIANCE);

    level.scene.zones[0].ambiance->name = "ambient_cave_a";
    level.scene.zones[0].height_mode = pistoris::ZoneHeightMode::kInfinite;
    level.scene.zones[0].height = std::numeric_limits<float>::quiet_NaN();
    CHECK(validateModules(level) == ARX_OK);
  }

  TEST_CASE("LevelValidationChecksPaths") {
    pistoris::LevelModules level = makeSimpleLevel();
    level.scene.paths.push_back(
        {"patrol",
         {},
         {{{}, pistoris::PathNodeType::kStandard, 0}, {{1.0f, 0.0f, 0.0f}, pistoris::PathNodeType::kBezier, 100}}});
    CHECK(validateModules(level) == ARX_OK);

    level.scene.paths[0].name.clear();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PATH_NAME);

    level.scene.paths[0].name = "patrol";
    level.scene.paths[0].position.y = std::numeric_limits<float>::infinity();
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PATH_POSITION);

    level.scene.paths[0].position = {};
    level.scene.paths[0].nodes.front().time_ms = 1;
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PATH_FIRST_NODE);

    level.scene.paths[0].nodes.front().time_ms = 0;
    level.scene.paths[0].nodes.back().type = static_cast<pistoris::PathNodeType>(10);
    CHECK(validateModules(level) == ARX_LEVEL_BAD_PATH_NODE_TYPE);

    level.scene.paths[0].nodes.back().type = pistoris::PathNodeType::kBezier;
    level.scene.paths.push_back(level.scene.paths.front());
    CHECK(validateModules(level) == ARX_LEVEL_DUPLICATE_PATH_NAME);
  }

  TEST_CASE("LevelExportRejectsZeroCornerNormals") {
    std::array normals = {pistoris::ArxVector3{}};
    pistoris::LevelModules src = makeNormalFan(normals);

    std::vector<std::uint8_t> glb;
    CHECK(exportLevelGlb(src, glb) == ARX_LEVEL_BAD_FACE_NORMAL);
  }

  TEST_CASE("LevelExportRejectsDegenerateFaces") {
    pistoris::LevelModules src;
    src.geometry.textures = {"graph/test.bmp"};
    src.geometry.vertices.resize(3);
    pistoris::ArxVector3 normal{0.0f, 1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 0.0f, 0.0f}, {2, normal, 0.0f, 0.0f}}}, 0, 0, 0.0f});

    std::vector<std::uint8_t> glb;
    CHECK(exportLevelGlb(src, glb) == ARX_LEVEL_DEGENERATE_FACE);
  }

  TEST_CASE("LevelValidationRejectsUnknownFaceBits") {
    pistoris::LevelModules src = makeNormalFan(std::array{pistoris::ArxVector3{0.0f, -1.0f, 0.0f}});
    src.geometry.faces[0].flags = 1U << 31;
    CHECK(validateModules(src) == ARX_LEVEL_BAD_FACE_TYPE);
  }

  TEST_CASE("FtsToLevelUsesTheLevelDegenerateFaceThreshold") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    auto& poly = src.cells[0].polygons[0];
    poly.v[2].ssz = 5.0e-4f;

    pistoris::LevelModules level;
    REQUIRE(buildLevelModules(level, src) == ARX_OK);
    REQUIRE(level.geometry.faces.size() == 1);
    CHECK(validateModules(level) == ARX_OK);
  }

  TEST_CASE("LevelGlbImportPreservesDistinctAccessorVertexIdentity") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"a.bmp", "b.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back(
        {{{{1, normal, 0.0f, 0.0f}, {3, normal, 1.0f, 1.0f}, {2, normal, 1.0f, 0.0f}}}, 1, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    REQUIRE(parsed.gltf["meshes"][0]["primitives"].size() == 2);
    int first_position = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["POSITION"].get<int>();
    parsed.gltf["accessors"].push_back(parsed.gltf["accessors"][first_position]);
    int duplicated_position = static_cast<int>(parsed.gltf["accessors"].size()) - 1;
    parsed.gltf["meshes"][0]["primitives"][1]["attributes"]["POSITION"] = duplicated_position;

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(dst.geometry.vertices.size() == 6);
    CHECK(dst.geometry.faces[0].corners[1].vertex != dst.geometry.faces[1].corners[0].vertex);
    CHECK(dst.geometry.faces[0].corners[2].vertex != dst.geometry.faces[1].corners[2].vertex);
  }

  TEST_CASE("LevelGlbImportDecodesNormalizedUnsignedTexcoords") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::array<std::uint16_t, 6> u16 = {0, 65535, 32768, 0, 0, 32768};
    replaceTexcoords(parsed, std::span<const std::uint16_t>(u16), 5123, true);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    CHECK(dst.geometry.faces[0].corners[0].u == doctest::Approx(0.0f));
    CHECK(dst.geometry.faces[0].corners[0].v == doctest::Approx(1.0f));
    CHECK(dst.geometry.faces[0].corners[1].u == doctest::Approx(32768.0f / 65535.0f));
    CHECK(dst.geometry.faces[0].corners[1].v == doctest::Approx(0.0f));

    parsed = parseTestGlb(glb);
    std::array<std::uint8_t, 6> u8 = {0, 255, 128, 0, 0, 128};
    replaceTexcoords(parsed, std::span<const std::uint8_t>(u8), 5121, true);
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.geometry.faces[0].corners[1].u == doctest::Approx(128.0f / 255.0f));
  }

  TEST_CASE("LevelGlbRoundtripPreservesBakedColors") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.lighting.corner_colors = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}, {0.7f, 0.8f, 0.9f}};

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const auto& attributes = parsed.gltf["meshes"][0]["primitives"][0]["attributes"];
    REQUIRE(attributes.contains("COLOR_0"));
    int color_accessor = attributes["COLOR_0"].get<int>();
    CHECK(parsed.gltf["accessors"][color_accessor]["componentType"] == 5126);
    CHECK(parsed.gltf["accessors"][color_accessor]["type"] == "VEC3");
    std::vector<float> colors = testFloatAttribute(parsed, "COLOR_0");
    REQUIRE(colors.size() == 9);
    CHECK(colors[0] == doctest::Approx(0.1f));
    CHECK(colors[4] == doctest::Approx(0.5f));
    CHECK(colors[8] == doctest::Approx(0.9f));

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    REQUIRE(dst.lighting.corner_colors.size() == 3);
    CHECK(bakedColor(dst, 0, 0).r == doctest::Approx(0.1f));
    CHECK(bakedColor(dst, 0, 1).g == doctest::Approx(0.5f));
    CHECK(bakedColor(dst, 0, 2).b == doctest::Approx(0.9f));
  }

  TEST_CASE("LevelGlbImportHandlesColorAccessorConventions") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::array<std::uint8_t, 12> rgba = {0, 127, 255, 19, 255, 0, 127, 23, 64, 128, 192, 29};
    replaceColors(parsed, std::span<const std::uint8_t>(rgba), 5121, true, 4);
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.lighting.corner_colors.size() == 3);
    CHECK(bakedColor(dst, 0, 0).g == doctest::Approx(127.0f / 255.0f));
    CHECK(bakedColor(dst, 0, 1).r == doctest::Approx(1.0f));
    CHECK(bakedColor(dst, 0, 2).b == doctest::Approx(192.0f / 255.0f));

    parsed = parseTestGlb(glb);
    replaceColors(parsed, std::span<const std::uint8_t>(rgba), 5121, false, 4);
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    replaceFirstAttributeFloat(parsed, "COLOR_0", 0, 1.1f);
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_GEOMETRY);

    parsed = parseTestGlb(glb);
    parsed.gltf["meshes"][0]["primitives"][0]["attributes"].erase("COLOR_0");
    LogCapture logs;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.lighting.corner_colors.empty());
    CHECK(logs.contains("3 baked color(s) defaulted"));
  }

  TEST_CASE("LevelGlbRoundtripPreservesPointLightConventions") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Hall_torch";
    light.position = {4.0f, 6.0f, 8.0f};
    light.color = {0.25f, 0.5f, 0.75f};
    light.fallstart = 3.0f;
    light.fallend = 10.0f;
    light.intensity = 2.0f;
    light.flags = pistoris::kLightFlagSemidynamic | pistoris::kLightFlagSpawnFire;
    light.flicker = {0.1f, 0.2f, 0.3f};
    light.effect_radius = 4.0f;
    light.effect_frequency = 0.5f;
    light.effect_size = 1.25f;
    light.effect_speed = 2.0f;
    light.flare_size = 80.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    REQUIRE(parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"].size() == 1);
    const auto& resource = parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"][0];
    CHECK(resource["name"] == "arx_light__FALLSTART_3__FALLEND_10__Hall_torch");
    CHECK(resource["type"] == "point");
    CHECK(resource["range"] == doctest::Approx(10.0f));
    CHECK(resource["intensity"] == doctest::Approx(2.0f));

    std::size_t parent_index = testNodeIndex(parsed, "lights_parent");
    std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLSTART_3__FALLEND_10__Hall_torch");
    REQUIRE(parent_index < parsed.gltf["nodes"].size());
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][parent_index]["translation"][1] == doctest::Approx(-30.0f));
    REQUIRE(parsed.gltf["nodes"][light_index]["children"].size() == 3);
    std::size_t settings_index = parsed.gltf["nodes"][light_index]["children"][0].get<std::size_t>();
    CHECK(parsed.gltf["nodes"][settings_index]["name"].get<std::string>().starts_with("SETTINGS__"));
    std::size_t helper_index = parsed.gltf["nodes"][light_index]["children"][1].get<std::size_t>();
    CHECK(parsed.gltf["nodes"][helper_index]["name"] == "FLAGS__SEMIDYNAMIC__SPAWNFIRE__Hall_torch");
    std::size_t effect_index = parsed.gltf["nodes"][light_index]["children"][2].get<std::size_t>();
    CHECK(parsed.gltf["nodes"][effect_index]["name"].get<std::string>().starts_with("EFFECT__"));

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].name == "Hall_torch");
    CHECK(dst.lighting.lights[0].position.x == doctest::Approx(4.0f));
    CHECK(dst.lighting.lights[0].position.y == doctest::Approx(6.0f));
    CHECK(dst.lighting.lights[0].position.z == doctest::Approx(8.0f));
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(3.0f));
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(10.0f));
    CHECK(dst.lighting.lights[0].color.r == doctest::Approx(0.25f));
    CHECK(dst.lighting.lights[0].color.g == doctest::Approx(0.5f));
    CHECK(dst.lighting.lights[0].color.b == doctest::Approx(0.75f));
    CHECK(dst.lighting.lights[0].intensity == doctest::Approx(2.0f));
    CHECK(dst.lighting.lights[0].flags == light.flags);
    CHECK(dst.lighting.lights[0].flicker.r == doctest::Approx(0.1f));
    CHECK(dst.lighting.lights[0].flicker.g == doctest::Approx(0.2f));
    CHECK(dst.lighting.lights[0].flicker.b == doctest::Approx(0.3f));
    CHECK(dst.lighting.lights[0].effect_radius == doctest::Approx(4.0f));
    CHECK(dst.lighting.lights[0].effect_frequency == doctest::Approx(0.5f));
    CHECK(dst.lighting.lights[0].effect_size == doctest::Approx(1.25f));
    CHECK(dst.lighting.lights[0].effect_speed == doctest::Approx(2.0f));
    CHECK(dst.lighting.lights[0].flare_size == doctest::Approx(80.0f));

    parsed.gltf["nodes"][light_index]["name"] = "arx_light__FALLSTART_3__FALLEND_10__Hall_torch.001";
    parsed.gltf["nodes"][settings_index]["name"] = "SETTINGS__RGB_0.25_0.5_0.75__INTENSITY_2__Hall_torch.001";
    parsed.gltf["nodes"][helper_index]["name"] = "FLAGS__SEMIDYNAMIC__SPAWNFIRE__Hall_torch.001";
    parsed.gltf["nodes"][effect_index]["name"] =
        "EFFECT__FLICKER_0.1_0.2_0.3__RADIUS_4__FREQUENCY_0.5__SIZE_1.25__SPEED_2__FLARESIZE_80__Hall_torch.001";
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].name == "Hall_torch.001");
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(3.0f));
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(10.0f));
    CHECK(dst.lighting.lights[0].flags == light.flags);
    CHECK(dst.lighting.lights[0].effect_radius == doctest::Approx(4.0f));
  }

  TEST_CASE("LevelGlbImportRejectsPayloadOnLightHelpers") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Torch";
    light.color = {0.25f, 0.5f, 0.75f};
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    light.intensity = 2.0f;
    light.flags = pistoris::kLightFlagOff;
    light.effect_radius = 4.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    REQUIRE(parsed.gltf["nodes"][light_index]["children"].size() == 3);

    pistoris::LevelModules dst;
    for (const nlohmann::json& helper : parsed.gltf["nodes"][light_index]["children"]) {
      ParsedTestGlb malformed = parsed;
      const std::size_t helper_index = helper.get<std::size_t>();
      malformed.gltf["nodes"][helper_index]["extensions"]["TEST_payload"] = nlohmann::json::object();
      CHECK(importLevelGlb(writeTestGlb(std::move(malformed)), dst) == ARX_GLB_BAD_LEVEL_LIGHT);
    }
  }

  TEST_CASE("LevelGlbImportKeepsNestedPointLightIndependentFromRoomGeometry") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Torch";
    light.color = {1.0f, 1.0f, 1.0f};
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    light.intensity = 1.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());

    std::optional<int> mesh_index;
    for (const nlohmann::json& node : parsed.gltf["nodes"]) {
      if (!node.contains("mesh")) continue;
      mesh_index = node["mesh"].get<int>();
      break;
    }
    REQUIRE(mesh_index.has_value());
    parsed.gltf["nodes"][light_index]["name"] = "generic_torch";
    parsed.gltf["nodes"][light_index]["mesh"] = *mesh_index;
    const std::size_t light_parent_index = testNodeIndex(parsed, "lights_parent");
    const std::size_t room_index = testNodeIndex(parsed, "arx_room__room");
    REQUIRE(light_parent_index < parsed.gltf["nodes"].size());
    REQUIRE(room_index < parsed.gltf["nodes"].size());
    reparentTestNode(parsed, light_parent_index, room_index);

    ParsedTestGlb with_child_mesh = parsed;
    ParsedTestGlb reserved_root_mesh = parsed;
    reserved_root_mesh.gltf["nodes"][light_index]["name"] = "arx_light__FALLEND_10__generic_torch";

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].name == "generic_torch");
    CHECK(dst.geometry.faces.size() == 1);
    CHECK(logs.contains("generic point light node"));
    CHECK(logs.contains("mesh discarded"));

    const std::size_t child_index = with_child_mesh.gltf["nodes"].size();
    with_child_mesh.gltf["nodes"].push_back({{"name", "discarded_light_mesh"}, {"mesh", *mesh_index}});
    with_child_mesh.gltf["nodes"][light_index]["children"].push_back(child_index);
    logs.messages.clear();
    REQUIRE(importLevelGlb(writeTestGlb(std::move(with_child_mesh)), dst) == ARX_OK);
    CHECK(dst.geometry.faces.size() == 1);
    CHECK(logs.contains("unexpected descendants"));
    CHECK(logs.contains("ignored 1 node(s), including 1 mesh node(s)"));

    logs.messages.clear();
    REQUIRE(importLevelGlb(writeTestGlb(std::move(reserved_root_mesh)), dst) == ARX_OK);
    CHECK(logs.contains("reserved light node"));
    CHECK(logs.contains("mesh discarded"));
  }

  TEST_CASE("LevelGlbImportIgnoresLevelObjectsBelowTerminalObjects") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Torch";
    light.color = {1.0f, 1.0f, 1.0f};
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    light.intensity = 1.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    REQUIRE(parsed.gltf["nodes"][light_index].contains("extensions"));

    const std::size_t nested_light = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back(
        {{"name", "nested_point_light"}, {"extensions", parsed.gltf["nodes"][light_index]["extensions"]}});
    const std::size_t wrapper = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "generic_wrapper"}, {"children", {nested_light}}});
    parsed.gltf["nodes"][light_index]["children"].push_back(wrapper);

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(dst.lighting.lights.size() == 1);
    CHECK(logs.contains("unexpected descendants"));
    CHECK(logs.contains("ignored 2 node(s), including 0 mesh node(s)"));
  }

  TEST_CASE("LevelGlbImportWarnsForUnrecognizedReachableArxNodes") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t unknown_index = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "arx_action__walk"}});
    const std::size_t scene_index = parsed.gltf.value("scene", 0U);
    parsed.gltf["scenes"][scene_index]["nodes"].push_back(unknown_index);

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(dst.geometry.faces.size() == 1);
    CHECK(logs.contains("arx_action__walk"));
    CHECK(logs.contains("reserved arx_ namespace but is not recognized"));
  }

  TEST_CASE("LevelGlbImportExcludesNonsemanticDiagnosticRoots") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);

    const std::size_t diagnostic_mesh = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "diagnostic_mesh"}, {"mesh", 0}});
    const std::size_t diagnostic_root = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "navigation_debug"}, {"children", {diagnostic_mesh}}});
    const std::size_t scene_index = parsed.gltf.value("scene", 0U);
    parsed.gltf["scenes"][scene_index]["nodes"].push_back(diagnostic_root);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(dst.geometry.faces.size() == 1);
  }

  TEST_CASE("LevelGlbImportValidatesEveryDuplicateLightHelperAndSelectsOne") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Torch";
    light.color = {1.0f, 0.5f, 0.25f};
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    light.intensity = 2.0f;
    light.flags = pistoris::kLightFlagOff;
    light.effect_radius = 4.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());

    const std::size_t settings = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "SETTINGS__RGB_0_1_0__INTENSITY_3__duplicate"}});
    const std::size_t flags = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "FLAGS__SPAWNFIRE__duplicate"}});
    const std::size_t effect = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "EFFECT__RADIUS_7__duplicate"}});
    parsed.gltf["nodes"][light_index]["children"].push_back(settings);
    parsed.gltf["nodes"][light_index]["children"].push_back(flags);
    parsed.gltf["nodes"][light_index]["children"].push_back(effect);
    ParsedTestGlb malformed = parsed;
    malformed.gltf["nodes"][effect]["name"] = "EFFECT__UNKNOWN_7__duplicate";

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK((dst.lighting.lights[0].intensity == doctest::Approx(2.0f) ||
           dst.lighting.lights[0].intensity == doctest::Approx(3.0f)));
    CHECK((dst.lighting.lights[0].flags == pistoris::kLightFlagOff ||
           dst.lighting.lights[0].flags == pistoris::kLightFlagSpawnFire));
    CHECK((dst.lighting.lights[0].effect_radius == doctest::Approx(4.0f) ||
           dst.lighting.lights[0].effect_radius == doctest::Approx(7.0f)));
    CHECK(importLevelGlb(writeTestGlb(std::move(malformed)), dst) == ARX_GLB_BAD_LEVEL_LIGHT);
  }

  TEST_CASE("LevelGlbImportRepairsLightNamesAndRejectsBadHelpers") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Torch";
    light.position = {0.0f, 0.0f, 0.0f};
    light.color = {1.0f, 1.0f, 1.0f};
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    light.intensity = 1.0f;
    light.flags = pistoris::kLightFlagOff;
    src.lighting.lights.push_back(light);
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][light_index]["name"] = "arx_light__FALLSTART_10__FALLEND_10__Torch";
    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(5.0f));
    CHECK(logs.contains("invalid FALLSTART"));

    parsed = parseTestGlb(glb);
    light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    std::size_t helper_index = parsed.gltf["nodes"][light_index]["children"][0].get<std::size_t>();
    parsed.gltf["nodes"][helper_index]["name"] = "FLAGS__UNKNOWN__Torch";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_LIGHT);

    parsed = parseTestGlb(glb);
    parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"][0].erase("range");
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(10.0f));

    parsed = parseTestGlb(glb);
    light_index = testNodeIndex(parsed, "arx_light__FALLEND_10__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][light_index]["name"] = "arx_light__Torch";
    parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"][0].erase("range");
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_LIGHT);
  }

  TEST_CASE("LevelGlbImportAcceptsReservedLightWithoutPointLightPayload") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Torch";
    light.position = {1.0f, 2.0f, 3.0f};
    light.color = {0.2f, 0.4f, 0.6f};
    light.fallstart = 2.0f;
    light.fallend = 8.0f;
    light.intensity = 3.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t light_index = testNodeIndex(parsed, "arx_light__FALLSTART_2__FALLEND_8__Torch");
    REQUIRE(light_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][light_index].erase("extensions");
    parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"].clear();
    parsed.gltf["extensionsUsed"] = nlohmann::json::array();

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].name == "Torch");
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(2.0f));
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(8.0f));
    CHECK(dst.lighting.lights[0].color.r == doctest::Approx(0.2f));
    CHECK(dst.lighting.lights[0].color.g == doctest::Approx(0.4f));
    CHECK(dst.lighting.lights[0].color.b == doctest::Approx(0.6f));
    CHECK(dst.lighting.lights[0].intensity == doctest::Approx(3.0f));
  }

  TEST_CASE("LevelGlbExportRejectsNonzeroEqualFalloff") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light light;
    light.name = "Equal_falloff";
    light.color = {1.0f, 1.0f, 1.0f};
    light.fallstart = 10.0f;
    light.fallend = 10.0f;
    light.intensity = 1.0f;
    src.lighting.lights.push_back(light);

    std::vector<std::uint8_t> glb;
    CHECK(exportLevelGlb(src, glb) == ARX_LEVEL_BAD_LIGHT_FALLOFF);
  }

  TEST_CASE("LevelGlbRoundtripPreservesZeroRangeLightEmitters") {
    pistoris::LevelModules src = makeSimpleLevel();
    pistoris::Light zero;
    zero.name = "Particle_candidate";
    zero.color = {1.0f, 0.5f, 0.25f};
    zero.fallstart = 0.0f;
    zero.fallend = 0.0f;
    zero.intensity = 10.0f;
    zero.flags = pistoris::kLightFlagSpawnSmoke;
    zero.effect_frequency = 0.75f;
    zero.effect_size = 2.0f;
    src.lighting.lights.push_back(zero);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    CHECK(!parsed.gltf.contains("extensions"));
    CHECK(testNodeIndex(parsed, "lights_parent") < parsed.gltf["nodes"].size());
    CHECK(testNodeIndex(parsed, "arx_light__Particle_candidate") < parsed.gltf["nodes"].size());

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    REQUIRE(dst.lighting.lights.size() == 1);
    CHECK(dst.lighting.lights[0].name == "Particle_candidate");
    CHECK(dst.lighting.lights[0].fallstart == doctest::Approx(0.0f));
    CHECK(dst.lighting.lights[0].fallend == doctest::Approx(0.0f));
    CHECK(dst.lighting.lights[0].flags == pistoris::kLightFlagSpawnSmoke);
    CHECK(dst.lighting.lights[0].effect_frequency == doctest::Approx(0.75f));
    CHECK(dst.lighting.lights[0].effect_size == doctest::Approx(2.0f));

    pistoris::Light point = zero;
    point.name = "Point";
    point.fallstart = 5.0f;
    point.fallend = 10.0f;
    src.lighting.lights.push_back(point);
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    parsed = parseTestGlb(glb);
    REQUIRE(parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"].size() == 1);
    CHECK(parsed.gltf["extensions"]["KHR_lights_punctual"]["lights"][0]["name"] == "arx_light__FALLEND_10__Point");
    CHECK(testNodeIndex(parsed, "arx_light__Particle_candidate") < parsed.gltf["nodes"].size());
  }

  TEST_CASE("LevelGlbImportRejectsInvalidAccessorSemantics") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::array<std::uint16_t, 6> u16 = {0, 65535, 32768, 0, 0, 32768};
    replaceTexcoords(parsed, std::span<const std::uint16_t>(u16), 5123, false);
    pistoris::LevelModules dst;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    int index_accessor = parsed.gltf["meshes"][0]["primitives"][0]["indices"].get<int>();
    parsed.gltf["accessors"][index_accessor]["normalized"] = true;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    int normal_accessor = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["NORMAL"].get<int>();
    parsed.gltf["accessors"][normal_accessor]["count"] = 2;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    int uv_accessor = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_0"].get<int>();
    parsed.gltf["accessors"][uv_accessor]["count"] = 2;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("LevelGlbImportEnforcesMaterialTextureConventions") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;

    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["name"] = "no_tex";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_MATERIAL);

    parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["name"] = "test__TRANS";
    parsed.gltf["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"][3] = 2.0f;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["name"] = "test__TRANSVAL_2";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_MATERIAL);

    parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["name"] = "test__TRANS__TRANSVAL_bad";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_MATERIAL);

    parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["name"] = "test__TRANS__TRANSVAL_inf";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_MATERIAL);

    parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["name"] = "test__TRANS__TRANSVAL_1__TRANSVAL_2";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_MATERIAL);

    parsed = parseTestGlb(glb);
    parsed.gltf["images"][0]["uri"] = "folder/test__variant.png";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_MATERIAL);

    parsed = parseTestGlb(glb);
    parsed.gltf["materials"][0]["pbrMetallicRoughness"]["metallicFactor"] = 0.0f;
    parsed.gltf["materials"][0]["pbrMetallicRoughness"]["roughnessFactor"] = 0.5f;
    parsed.gltf["materials"][0]["pbrMetallicRoughness"]["metallicRoughnessTexture"] = {{"index", 0}};
    parsed.gltf["materials"][0]["normalTexture"] = {{"index", 0}};
    parsed.gltf["materials"][0]["occlusionTexture"] = {{"index", 0}};
    parsed.gltf["materials"][0]["emissiveTexture"] = {{"index", 0}};
    parsed.gltf["materials"][0]["emissiveFactor"] = {0.2f, 0.3f, 0.4f};
    parsed.gltf["materials"][0]["extensions"]["KHR_materials_emissive_strength"] = {{"emissiveStrength", 2.0f}};
    parsed.gltf["materials"][0]["extensions"]["KHR_materials_unlit"] = nlohmann::json::object();
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);

    parsed = parseTestGlb(glb);
    parsed.gltf["extensionsUsed"] = {"EXT_unrelated"};
    parsed.gltf["extensionsRequired"] = {"EXT_unrelated"};
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_UNSUPPORTED_FEATURE);

    parsed = parseTestGlb(glb);
    int position_accessor = parsed.gltf["meshes"][0]["primitives"][0]["attributes"]["POSITION"].get<int>();
    parsed.gltf["meshes"][0]["primitives"][0]["targets"] = {{{"POSITION", position_accessor}}};
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_UNSUPPORTED_FEATURE);
  }

  TEST_CASE("LevelGlbImportTreatsMaskAsTextureCutout") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb cutout = parseTestGlb(glb);
    const int material_index = cutout.gltf["meshes"][0]["primitives"][0]["material"].get<int>();
    auto& material = cutout.gltf["materials"][material_index];
    material["alphaMode"] = "MASK";
    material["alphaCutoff"] = 0.5f;

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(cutout), dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    CHECK((dst.geometry.faces[0].flags & pistoris::kFaceBitTrans) == 0);
    CHECK(dst.geometry.faces[0].transval == 0.0f);

    LogCapture logs;
    ParsedTestGlb normalized = parseTestGlb(glb);
    auto& normalized_material = normalized.gltf["materials"][material_index];
    normalized_material["alphaMode"] = "MASK";
    normalized_material["alphaCutoff"] = 0.25f;
    normalized_material["pbrMetallicRoughness"]["baseColorFactor"][3] = 0.5f;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(normalized)), dst) == ARX_OK);
    CHECK(logs.contains("MASK base alpha 0.5 and cutoff 0.25 normalized to texture alpha with cutoff 0.5"));

    ParsedTestGlb named_trans = parseTestGlb(glb);
    auto& named_material = named_trans.gltf["materials"][material_index];
    named_material["name"] = "test__TRANS";
    named_material["alphaMode"] = "MASK";
    named_material["alphaCutoff"] = 0.5f;
    named_material["pbrMetallicRoughness"]["baseColorFactor"][3] = 0.25f;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(named_trans)), dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    CHECK((dst.geometry.faces[0].flags & pistoris::kFaceBitTrans) != 0);
    CHECK(dst.geometry.faces[0].transval == doctest::Approx(0.75f));
  }

  TEST_CASE("LevelGlbImportKeepsDistinctImagesDespiteMatchingMaterialStems") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"a/first.bmp", "b/second.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back(
        {{{{1, normal, 0.0f, 0.0f}, {3, normal, 1.0f, 1.0f}, {2, normal, 0.0f, 1.0f}}}, 1, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["materials"][1]["name"] = "first";

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.geometry.textures.size() == 2);
    CHECK(dst.geometry.faces[0].texture == 0);
    CHECK(dst.geometry.faces[1].texture == 1);
    CHECK(dst.geometry.textures[0].path == "a/first.bmp");
    CHECK(dst.geometry.textures[1].path == "b/second.bmp");
  }

  TEST_CASE("LevelGlbImportDeduplicatesMaterialsReferencingTheSameImage") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"first.bmp", "second.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back(
        {{{{1, normal, 0.0f, 0.0f}, {3, normal, 1.0f, 1.0f}, {2, normal, 0.0f, 1.0f}}}, 1, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["materials"][1]["name"] = "second__NO_SHADOW";
    parsed.gltf["materials"][1]["pbrMetallicRoughness"]["baseColorTexture"]["index"] =
        parsed.gltf["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]["index"];

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.geometry.textures.size() == 1);
    CHECK(dst.geometry.faces[0].texture == 0);
    CHECK(dst.geometry.faces[1].texture == 0);
    CHECK((dst.geometry.faces[1].flags & pistoris::kFaceBitNoShadow) != 0);
  }

  TEST_CASE("LevelGlbImportDisambiguatesDifferentImagesWithTheSameExtensionlessPath") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"first.bmp", "second.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}, {{1.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 1.0f, 0.0f}, {2, normal, 0.0f, 1.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back(
        {{{{1, normal, 0.0f, 0.0f}, {3, normal, 1.0f, 1.0f}, {2, normal, 0.0f, 1.0f}}}, 1, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["images"][0]["uri"] = "folder/shared.jpg";
    parsed.gltf["images"][1]["uri"] = "folder/shared.png";

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.geometry.textures.size() == 2);
    CHECK(dst.geometry.textures[0].path == "folder/shared.jpg");
    CHECK(dst.geometry.textures[1].path == "folder/shared_1.png");
  }

  TEST_CASE("LevelGlbImportReportsDiscardedAttributeBindings") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    auto& attributes = parsed.gltf["meshes"][0]["primitives"][0]["attributes"];
    attributes["TEXCOORD_1"] = attributes["TEXCOORD_0"];
    attributes["COLOR_1"] = attributes["COLOR_0"];
    attributes["_CUSTOM"] = attributes["NORMAL"];

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(logs.contains("1 unselected UV set binding(s) discarded"));
    CHECK(logs.contains("1 color attribute binding(s) discarded"));
    CHECK(logs.contains("1 custom attribute binding(s) discarded"));
  }

  TEST_CASE("LevelGlbImportRejectsNonFiniteAttributesAsBadGlb") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;

    ParsedTestGlb parsed = parseTestGlb(glb);
    replaceFirstAttributeFloat(parsed, "POSITION", 0, std::numeric_limits<float>::infinity());
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    replaceFirstAttributeFloat(parsed, "NORMAL", 1, std::numeric_limits<float>::quiet_NaN());
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    replaceFirstAttributeFloat(parsed, "TEXCOORD_0", 0, std::numeric_limits<float>::infinity());
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("LevelGlbImportPrunesAndReportsUnreferencedVertices") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);

    while (parsed.bin.size() % 4 != 0) parsed.bin.push_back(0);
    std::size_t position_offset = parsed.bin.size();
    std::array<float, 9> degenerate_positions = {10.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f, 10.0f, 0.0f, 0.0f};
    for (float value : degenerate_positions) appendTestPod(parsed.bin, value);
    int position_view = static_cast<int>(parsed.gltf["bufferViews"].size());
    parsed.gltf["bufferViews"].push_back(
        {{"buffer", 0}, {"byteOffset", position_offset}, {"byteLength", sizeof(degenerate_positions)}});
    int position_accessor = static_cast<int>(parsed.gltf["accessors"].size());
    parsed.gltf["accessors"].push_back(
        {{"bufferView", position_view}, {"componentType", 5126}, {"count", 3}, {"type", "VEC3"}});

    std::size_t index_offset = parsed.bin.size();
    std::array<std::uint32_t, 3> indices = {0, 1, 2};
    for (std::uint32_t value : indices) appendTestPod(parsed.bin, value);
    int index_view = static_cast<int>(parsed.gltf["bufferViews"].size());
    parsed.gltf["bufferViews"].push_back(
        {{"buffer", 0}, {"byteOffset", index_offset}, {"byteLength", sizeof(indices)}});
    int index_accessor = static_cast<int>(parsed.gltf["accessors"].size());
    parsed.gltf["accessors"].push_back(
        {{"bufferView", index_view}, {"componentType", 5125}, {"count", 3}, {"type", "SCALAR"}});

    nlohmann::json primitive = parsed.gltf["meshes"][0]["primitives"][0];
    primitive["attributes"]["POSITION"] = position_accessor;
    primitive["indices"] = index_accessor;
    parsed.gltf["meshes"][0]["primitives"].push_back(std::move(primitive));

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.geometry.vertices.size() == 3);
    CHECK(logs.contains("3 unreferenced vertex/vertices discarded"));
  }

  TEST_CASE("LevelGlbImportMapsSemanticJsonErrorsToBadFormat") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["accessors"][0]["bufferView"] = "invalid";
    pistoris::LevelModules dst;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("LevelGlbImportRejectsInvalidNodeGraphs") {
    pistoris::LevelModules src = makeSimpleLevel();
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;

    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][0]["children"] = {0};
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"].push_back({{"children", {0}}});
    parsed.gltf["nodes"].push_back({{"children", {0}}});
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][0]["matrix"] = {1.0f, 0.0f};
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_FORMAT);
  }

  TEST_CASE("LevelGlbImportWeldsSplitPortalPositions") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.rooms.definitions.push_back({"room_2"});
    addSecondRoomTriangle(src);
    pistoris::Portal portal;
    connectPortal(portal, "portal");
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
    src.rooms.portals.push_back(portal);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t portal_node_index = testNodeIndex(parsed, "arx_portal__room__room_2__portal");
    REQUIRE(portal_node_index < parsed.gltf["nodes"].size());
    splitNodePrimitivePositions(parsed, portal_node_index);

    pistoris::LevelModules dst;
    LogCapture logs;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.rooms.portals.size() == 1);
    CHECK(dst.rooms.portals[0].shape == pistoris::PortalShape::kQuad);
    CHECK(logs.contains("welded 6 referenced source vertices to 4 positions"));

    const int mesh_index = parsed.gltf["nodes"][portal_node_index]["mesh"].get<int>();
    const int position_accessor =
        parsed.gltf["meshes"][mesh_index]["primitives"][0]["attributes"]["POSITION"].get<int>();
    const auto& accessor = parsed.gltf["accessors"][position_accessor];
    const auto& view = parsed.gltf["bufferViews"][accessor["bufferView"].get<int>()];
    const std::size_t base_offset = view.value("byteOffset", 0U) + accessor.value("byteOffset", 0U);
    auto offset_position = [&](std::size_t vertex, std::size_t component, float delta) {
      const std::size_t offset = base_offset + (vertex * 3 + component) * sizeof(float);
      REQUIRE(offset + sizeof(float) <= parsed.bin.size());
      float value = 0.0f;
      std::memcpy(&value, parsed.bin.data() + offset, sizeof(value));
      value += delta;
      std::memcpy(parsed.bin.data() + offset, &value, sizeof(value));
    };
    offset_position(4, 0, 0.01f);
    offset_position(5, 2, 0.01f);
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_PORTAL);
  }

  TEST_CASE("LevelGlbImportValidatesPortalContracts") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.rooms.definitions.push_back({"room_2"});
    addSecondRoomTriangle(src);
    pistoris::Portal portal;
    connectPortal(portal, "portal");
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};
    src.rooms.portals.push_back(portal);
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;
    auto portal_mesh = [](const ParsedTestGlb& parsed_glb) {
      std::size_t portal_node_index = testNodeIndex(parsed_glb, "arx_portal__room__room_2__portal");
      REQUIRE(portal_node_index < parsed_glb.gltf["nodes"].size());
      return parsed_glb.gltf["nodes"][portal_node_index]["mesh"].get<int>();
    };

    ParsedTestGlb parsed = parseTestGlb(glb);
    int mesh = portal_mesh(parsed);
    parsed.gltf["meshes"][mesh]["primitives"].push_back(parsed.gltf["meshes"][mesh]["primitives"][0]);
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_PORTAL);

    parsed = parseTestGlb(glb);
    mesh = portal_mesh(parsed);
    parsed.gltf["meshes"][mesh]["primitives"][0]["mode"] = 5;
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_PORTAL);

    parsed = parseTestGlb(glb);
    std::size_t portal_node_index = testNodeIndex(parsed, "arx_portal__room__room_2__portal");
    REQUIRE(portal_node_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][portal_node_index].erase("mesh");
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_PORTAL);

    parsed = parseTestGlb(glb);
    mesh = portal_mesh(parsed);
    parsed.gltf["meshes"][mesh]["primitives"][0].erase("material");
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.rooms.portals.size() == 1);

    parsed = parseTestGlb(glb);
    mesh = portal_mesh(parsed);
    int portal_material = parsed.gltf["meshes"][mesh]["primitives"][0]["material"].get<int>();
    parsed.gltf["materials"][portal_material]["name"] = "custom_portal_preview";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.rooms.portals.size() == 1);

    parsed = parseTestGlb(glb);
    mesh = portal_mesh(parsed);
    portal_material = parsed.gltf["meshes"][mesh]["primitives"][0]["material"].get<int>();
    parsed.gltf["materials"][portal_material].erase("name");
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.rooms.portals.size() == 1);

    parsed = parseTestGlb(glb);
    mesh = portal_mesh(parsed);
    int position_accessor = parsed.gltf["meshes"][mesh]["primitives"][0]["attributes"]["POSITION"].get<int>();
    parsed.gltf["meshes"][mesh]["primitives"][0]["targets"] = {{{"POSITION", position_accessor}}};
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_UNSUPPORTED_FEATURE);

    parsed = parseTestGlb(glb);
    mesh = portal_mesh(parsed);
    parsed.gltf["extensionsUsed"] = {"EXT_portal_test"};
    parsed.gltf["meshes"][mesh]["primitives"][0]["extensions"]["EXT_portal_test"] = nlohmann::json::object();
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_UNSUPPORTED_FEATURE);
  }

  TEST_CASE("LevelGlbImportParsesReservedObjectNamesStrictly") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.anchors.push_back({{0.0f, 0.0f, 0.0f}, 1.0f, -1.0f, pistoris::kAnchorFlagBlocked, {}});
    pistoris::Light light;
    light.name = "torch";
    light.fallstart = 5.0f;
    light.fallend = 10.0f;
    src.lighting.lights.push_back(light);
    src.scene.entities.push_back({"graph/obj3d/interactive/fix_inter/door/door", -1, {}, {}, "door"});
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    pistoris::LevelModules dst;

    ParsedTestGlb parsed = parseTestGlb(glb);
    auto find_anchor = [](const ParsedTestGlb& parsed_glb) {
      for (std::size_t i = 0; i < parsed_glb.gltf["nodes"].size(); ++i) {
        if (parsed_glb.gltf["nodes"][i].value("name", std::string{}).starts_with("arx_anchor__")) return i;
      }
      return std::numeric_limits<std::size_t>::max();
    };
    std::size_t anchor_node = find_anchor(parsed);
    REQUIRE(anchor_node != std::numeric_limits<std::size_t>::max());

    parsed.gltf["nodes"][anchor_node]["name"] = "arx_anchor";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.navigation.anchors.empty());

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][anchor_node]["name"] = "arx_anchor_bad";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.navigation.anchors.empty());

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][anchor_node]["name"] = "arx_anchor__RADIUS_bad__anchor_0";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_ANCHOR);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][anchor_node]["name"] = "arx_anchor__BLOCKED__BLOCKED__anchor_0";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_ANCHOR);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][anchor_node]["name"] = "arx_anchor__";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_ANCHOR);

    parsed = parseTestGlb(glb);
    std::size_t room_node = testNodeIndex(parsed, "arx_room__room");
    REQUIRE(room_node < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][room_node]["name"] = "arx_room__room__invalid";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_ROOM);

    parsed = parseTestGlb(glb);
    std::size_t light_node = testNodeIndex(parsed, "arx_light__FALLEND_10__torch");
    REQUIRE(light_node < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][light_node]["name"] = "arx_light__";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_LIGHT);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][light_node]["name"] = "arx_light__UNKNOWN__torch";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_LIGHT);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][light_node]["name"] = "arx_light__FALLEND_10__FALLEND_20__torch";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_LIGHT);

    parsed = parseTestGlb(glb);
    std::size_t entity_node = testNodeIndex(parsed, "arx_entity__000__door");
    REQUIRE(entity_node < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][entity_node]["name"] = "arx_entity__000__door__extra";
    CHECK(importLevelGlb(writeTestGlb(parsed), dst) == ARX_GLB_BAD_LEVEL_ENTITY);

    parsed = parseTestGlb(glb);
    parsed.gltf["nodes"][anchor_node]["name"] = "arx_anchorish";
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    CHECK(dst.navigation.anchors.empty());
  }

  TEST_CASE("LevelGlbImportDoesNotWeldAcrossNodeInstances") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"graph/test.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 0.0f, 0.0f}, {2, normal, 0.0f, 0.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    parsed.gltf["nodes"].push_back({{"name", "arx_room__room_2"}, {"mesh", 0}, {"translation", {0.5f, 0.0f, -0.5f}}});
    const std::size_t level_space = testNodeIndex(parsed, "level_space");
    REQUIRE(level_space < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][level_space]["children"].push_back(parsed.gltf["nodes"].size() - 1);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_OK);
    CHECK(dst.geometry.faces.size() == 2);
    CHECK(dst.geometry.vertices.size() == 6);
  }

  TEST_CASE("LevelGlbImportReadsRoomChildMeshes") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"graph/test.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 0.0f, 0.0f}, {2, normal, 0.0f, 0.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t child_index = testNodeIndex(parsed, "arx_room__room");
    REQUIRE(child_index < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][child_index]["name"] = "room_mesh";
    std::size_t room_index = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "arx_room__room"}, {"children", {child_index}}});
    std::size_t room_parent = testNodeIndex(parsed, "rooms_parent");
    REQUIRE(room_parent < parsed.gltf["nodes"].size());
    parsed.gltf["nodes"][room_parent]["children"] = {room_index};
    parsed.gltf["cameras"] =
        nlohmann::json::array({{{"type", "perspective"}, {"perspective", {{"yfov", 1.0f}, {"znear", 0.1f}}}}});
    parsed.gltf["nodes"][child_index]["camera"] = 0;

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.rooms.definitions.size() == 1);
    CHECK(dst.rooms.definitions[0].name == "room");
    REQUIRE(dst.geometry.faces.size() == 1);
    CHECK(dst.rooms.face_rooms[0] == 0);

    parsed.gltf["nodes"][child_index]["name"] = "arx_anchor__RADIUS_1__HEIGHT_1__anchor_0";
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_ANCHOR);
  }

  TEST_CASE("LevelGlbImportKeepsSemanticObjectsNestedUnderRoomsIndependent") {
    pistoris::LevelModules src = makeSimpleLevel();
    src.navigation.anchors.push_back({{0.25f, 0.0f, 0.25f}, 50.0f, -165.0f, 0, "center"});
    src.navigation.surface =
        pistoris::NavSurface{{{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}}, {{{{0, 1, 2}}}}};
    pistoris::Fog fog;
    fog.position = {0.25f, 0.0f, 0.25f};
    fog.color = {0.25f, 0.5f, 0.75f};
    fog.size = 30.0f;
    fog.directional = true;
    fog.scale = 0.5f;
    fog.rotation = pistoris::math::angleToQuat({15.0f, 25.0f, 0.0f});
    fog.speed = 4.0f;
    fog.rotate_speed = 1.5f;
    fog.lifetime_ms = 2000;
    fog.frequency = 500.0f;
    fog.name = "room_mist";
    src.scene.fogs.push_back(fog);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    ParsedTestGlb parsed = parseTestGlb(glb);
    const std::size_t room_index = testNodeIndex(parsed, "arx_room__room");
    const std::size_t anchor_parent = testNodeIndex(parsed, "anchors_parent");
    const std::size_t fog_parent = testNodeIndex(parsed, "fogs_parent");
    const std::size_t nav_surface_index = testNodeIndex(parsed, "arx_nav_surface__surface");
    REQUIRE(room_index < parsed.gltf["nodes"].size());
    REQUIRE(anchor_parent < parsed.gltf["nodes"].size());
    REQUIRE(fog_parent < parsed.gltf["nodes"].size());
    REQUIRE(nav_surface_index < parsed.gltf["nodes"].size());
    reparentTestNode(parsed, anchor_parent, room_index);
    reparentTestNode(parsed, fog_parent, room_index);
    reparentTestNode(parsed, nav_surface_index, room_index);
    const std::size_t nav_surface_child = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "nav_surface_note"}});
    parsed.gltf["nodes"][nav_surface_index]["children"].push_back(nav_surface_child);

    LogCapture logs;
    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(writeTestGlb(parsed), dst) == ARX_OK);
    REQUIRE(dst.geometry.faces.size() == 1);
    REQUIRE(dst.navigation.anchors.size() == 1);
    CHECK(dst.navigation.anchors[0].name == "center");
    REQUIRE(dst.navigation.surface.has_value());
    CHECK(dst.navigation.surface->triangles.size() == 1);
    REQUIRE(dst.scene.fogs.size() == 1);
    CHECK(dst.scene.fogs[0].name == "room_mist");
    CHECK(logs.contains("nav surface"));
    CHECK(logs.contains("unexpected descendants"));
    CHECK(logs.contains("ignored 1 node(s), including 0 mesh node(s)"));

    const std::size_t nested_room_index = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "arx_room__nested"}});
    const std::size_t wrapper_index = parsed.gltf["nodes"].size();
    parsed.gltf["nodes"].push_back({{"name", "nested_room_wrapper"}, {"children", {nested_room_index}}});
    parsed.gltf["nodes"][room_index]["children"].push_back(wrapper_index);
    CHECK(importLevelGlb(writeTestGlb(std::move(parsed)), dst) == ARX_GLB_BAD_LEVEL_HIERARCHY);
  }

  TEST_CASE("LevelGlbPreservesDistinctLevelVertexIdentity") {
    pistoris::LevelModules src;
    addDefaultRoom(src);
    src.geometry.textures = {"graph/test.bmp"};
    src.geometry.vertices = {{{0.0f, 0.0f, 0.0f}},
                             {{1.0f, 0.0f, 0.0f}},
                             {{0.0f, 0.0f, 1.0f}},
                             {{0.0f, 0.0f, 0.0f}},
                             {{2.0f, 0.0f, 0.0f}},
                             {{2.0f, 0.0f, 1.0f}}};
    pistoris::ArxVector3 normal{0.0f, -1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 0.0f, 0.0f}, {2, normal, 0.0f, 0.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);
    src.geometry.faces.push_back(
        {{{{3, normal, 0.0f, 0.0f}, {4, normal, 0.0f, 0.0f}, {5, normal, 0.0f, 0.0f}}}, 0, 0, 0.0f});
    src.rooms.face_rooms.push_back(0);

    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(src, glb) == ARX_OK);
    CHECK(testAttributeCount(parseTestGlb(glb), "POSITION") == src.geometry.vertices.size());

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(dst.geometry.vertices.size() == src.geometry.vertices.size());
  }

  TEST_CASE("LevelRenderSplitsDebugGlbRejectsInvalidReferences") {
    pistoris::LevelModules src;
    src.geometry.textures = {"graph/test.bmp"};
    src.geometry.vertices.resize(3);
    pistoris::ArxVector3 normal{0.0f, 1.0f, 0.0f};
    src.geometry.faces.push_back(
        {{{{0, normal, 0.0f, 0.0f}, {1, normal, 0.0f, 0.0f}, {3, normal, 0.0f, 0.0f}}}, 0, 0, 0.0f});
    std::vector<std::uint8_t> glb = {1, 2, 3};
    CHECK(exportRenderSplitsDebugGlb(src, 0.0f, glb) == ARX_LEVEL_BAD_FACE_VERTEX);
    CHECK(glb == std::vector<std::uint8_t>{1, 2, 3});
  }

  TEST_CASE("FtsLevelGlbRoundtripTriangleScene") {
    pistoris::fts::Data src = makeTriangleFtsScene();

    pistoris::LevelModules source_level;
    REQUIRE(pistoris::arx_level_conversion::buildLevel({src}, source_level) == ARX_OK);
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(source_level, glb) == ARX_OK);

    pistoris::LevelModules dst;
    REQUIRE(importLevelGlb(glb, dst) == ARX_OK);
    CHECK(dst.geometry.faces.size() == 1);
    CHECK(dst.geometry.textures == source_level.geometry.textures);
    CHECK(dst.geometry.faces[0].texture == 0);
    CHECK((dst.geometry.faces[0].flags & pistoris::kFaceBitStone) != 0);
  }

  TEST_CASE("FtsGlbExportsAnchorsUnderCommonParent") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::fts::Anchor anchor{};
    anchor.data.pos = {0.25f, 0.0f, 0.25f};
    anchor.data.radius = 4.0f;
    anchor.data.height = -5.0f;
    src.anchors.push_back(anchor);
    src.scene.num_anchors = 1;

    pistoris::LevelModules level;
    REQUIRE(pistoris::arx_level_conversion::buildLevel({src}, level) == ARX_OK);
    REQUIRE(level.navigation.anchors.size() == 1);
    CHECK(level.navigation.anchors[0].height == -5.0f);
    std::vector<std::uint8_t> glb;
    REQUIRE(exportLevelGlb(level, glb) == ARX_OK);

    std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
    CHECK(text.find("anchors_parent") != std::string::npos);
    CHECK(text.find("portals_parent") == std::string::npos);
    CHECK(text.find("children") != std::string::npos);
    CHECK(text.find("arx_anchor") != std::string::npos);
  }

  TEST_CASE("FtsToLevelRetainsAnchorsOutsideRepairedReferencedBounds") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::fts::Poly room_0 = src.cells[0].polygons.front();
    room_0.room = 0;
    room_0.v[0].ssx = room_0.v[0].ssz = 2.0f;
    room_0.v[1].ssx = 3.0f;
    room_0.v[1].ssz = 2.0f;
    room_0.v[2].ssx = 2.0f;
    room_0.v[2].ssz = 3.0f;
    src.cells[0].polygons.push_back(room_0);
    src.scene.num_polys = 2;
    pistoris::fts::Anchor anchor{};
    anchor.data.pos = {2.0f, 0.0f, 2.0f};
    anchor.data.radius = 4.0f;
    anchor.data.height = -5.0f;
    src.anchors = {anchor};
    src.scene.num_anchors = 1;

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(pistoris::arx_level_conversion::buildLevel({src}, level) == ARX_OK);

    REQUIRE(level.navigation.anchors.size() == 1);
    CHECK(level.navigation.anchors[0].position.x == 2.0f);
    CHECK(level.navigation.anchors[0].position.z == 2.0f);
    CHECK(logs.contains("1 anchor(s) outside referenced geometry bounds retained"));
  }

  TEST_CASE("FtsToLevelDiscardsFiniteAnchorsOutsideNativeBounds") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::fts::Anchor anchor{};
    anchor.data.pos = {0.25f, 0.0f, -0.05f};
    anchor.data.radius = 4.0f;
    anchor.data.height = -5.0f;
    src.anchors = {anchor};
    src.scene.num_anchors = 1;

    LogCapture logs;
    pistoris::LevelModules level;
    REQUIRE(pistoris::arx_level_conversion::buildLevel({src}, level) == ARX_OK);

    CHECK(level.navigation.anchors.empty());
    CHECK(logs.contains("1 anchor(s) outside native X/Z bounds discarded"));
  }

  TEST_CASE("FtsToLevelRemapsConnectionsAfterDiscardingOutOfBoundsAnchors") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::fts::Anchor first{};
    first.data.pos = {0.25f, 0.0f, 0.25f};
    first.data.radius = 4.0f;
    first.data.height = -5.0f;
    first.linked = {1, 2};
    pistoris::fts::Anchor discarded = first;
    discarded.data.pos.z = -0.05f;
    discarded.linked = {0};
    pistoris::fts::Anchor second = first;
    second.data.pos = {0.75f, 0.0f, 0.25f};
    second.linked = {0};
    src.anchors = {first, discarded, second};
    src.scene.num_anchors = 3;

    pistoris::LevelModules level;
    REQUIRE(pistoris::arx_level_conversion::buildLevel({src}, level) == ARX_OK);
    REQUIRE(level.navigation.anchors.size() == 2);
    REQUIRE(level.navigation.connections.size() == 1);
    CHECK(level.navigation.connections[0].first == 0);
    CHECK(level.navigation.connections[0].second == 1);
  }

  TEST_CASE("FtsToLevelPreservesAnchorConnections") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::fts::Anchor first{};
    first.data.pos = {0.25f, 0.0f, 0.25f};
    first.data.radius = 4.0f;
    first.data.height = -5.0f;
    first.linked = {1};
    pistoris::fts::Anchor second{};
    second.data.pos = {0.75f, 0.0f, 0.25f};
    second.data.radius = 4.0f;
    second.data.height = -5.0f;
    second.linked = {0};
    src.anchors = {first, second};
    src.scene.num_anchors = 2;

    pistoris::LevelModules level;
    REQUIRE(pistoris::arx_level_conversion::buildLevel({src}, level) == ARX_OK);
    REQUIRE(level.navigation.anchors.size() == 2);
    REQUIRE(level.navigation.connections.size() == 1);
    CHECK(level.navigation.connections[0].first == 0);
    CHECK(level.navigation.connections[0].second == 1);
  }

  TEST_CASE("FtsDebugCellsGlbExportsCellNodes") {
    pistoris::fts::Data src = makeTriangleFtsScene();

    std::vector<std::uint8_t> glb;
    REQUIRE(pistoris::buildFtsCellsDebugGlb(src, glb) == ARX_OK);

    std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
    CHECK(text.find("fts_cell__x0_z0") != std::string::npos);
    CHECK(text.find("arx_level_debug_geometry") != std::string::npos);
    CHECK(text.find("graph/levels/test.bmp") == std::string::npos);
  }

  TEST_CASE("FtsDebugCellsGlbUsesLevelExportCoordinates") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    pistoris::Level::GlbExportOptions options;
    options.arx_units_per_glb_unit = 50.0f;
    options.arx_offset = {100.0f, -200.0f, 300.0f};

    std::vector<std::uint8_t> glb;
    REQUIRE(pistoris::buildFtsCellsDebugGlb(src, glb, options) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    std::size_t transform_index = testNodeIndex(parsed, "level_space");
    REQUIRE(transform_index < parsed.gltf["nodes"].size());
    CHECK(parsed.gltf["nodes"][transform_index]["scale"][0].get<float>() == doctest::Approx(0.02f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][0].get<float>() == doctest::Approx(-2.0f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][1].get<float>() == doctest::Approx(-4.0f));
    CHECK(parsed.gltf["nodes"][transform_index]["translation"][2].get<float>() == doctest::Approx(6.0f));
    CHECK_FALSE(parsed.gltf["nodes"][transform_index].contains("rotation"));
  }

  TEST_CASE("FtsDebugRoomsGlbExportsRoomAndPortalNodes") {
    pistoris::fts::Data src = makeTriangleFtsScene();
    src.scene.num_rooms = 1;
    src.rooms.resize(2);
    src.rooms[0].polygons = {{0, 0, 0, 0}};
    src.portals.resize(1);
    src.scene.num_portals = 1;
    src.portals[0].room_1 = 0;
    src.portals[0].room_2 = 1;
    src.portals[0].useportal = 1;
    src.portals[0].poly.v[0].pos = {0.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[1].pos = {1.0f, 0.0f, 0.0f};
    src.portals[0].poly.v[2].pos = {0.0f, 1.0f, 0.0f};
    src.room_distances.resize(4);

    std::vector<std::uint8_t> glb;
    REQUIRE(pistoris::buildFtsRoomsDebugGlb(src, glb) == ARX_OK);

    ParsedTestGlb parsed = parseTestGlb(glb);
    checkTestMaterial(parsed, "arx_portal", {0.10f, 0.45f, 1.00f, 0.35f}, "BLEND", true);
    std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
    CHECK(text.find("fts_room__r0") != std::string::npos);
    CHECK(text.find("fts_portal__r0_r1_u1") != std::string::npos);
    CHECK(text.find("graph/levels/test.bmp") == std::string::npos);
  }
}
