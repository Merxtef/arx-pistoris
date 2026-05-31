// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "arx/ftl.h"
#include "arx/tea.h"
#include "external/glb.h"
#include "external/mat_name.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/mat4.h"
#include "utils/math/quat.h"
#include "utils/math/vec3.h"
#include "utils/parse_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <functional>
#include <nlohmann/json.hpp>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {

// Selection names are positional; preserve attribute insertion order
using Json = nlohmann::ordered_json;

namespace {

// --- glTF constants ---

constexpr int kCompByte   = 5120;
constexpr int kCompUByte  = 5121;
constexpr int kCompShort  = 5122;
constexpr int kCompUShort = 5123;
constexpr int kCompUInt   = 5125;
constexpr int kCompFloat  = 5126;

constexpr int kModeTriangles     = 4;
constexpr int kModeTriangleStrip = 5;
constexpr int kModeTriangleFan   = 6;

constexpr uint32_t kGlbMagic      = 0x46546C67u;  // 'glTF'
constexpr uint32_t kGlbVersion    = 2u;
constexpr uint32_t kChunkTypeJson = 0x4E4F534Au;  // 'JSON'
constexpr uint32_t kChunkTypeBin  = 0x004E4942u;  // 'BIN\0'

struct BoneOrdinalName {
  bool has_ordinal = false;
  size_t ordinal   = 0;
  std::string_view stripped_name;
};

BoneOrdinalName parseBoneOrdinalName(std::string_view name) {
  BoneOrdinalName out{};
  out.stripped_name = name;

  size_t sep = name.find("__");
  if (sep == std::string_view::npos || sep == 0) return out;

  size_t ordinal = 0;
  for (size_t i = 0; i < sep; ++i) {
    unsigned char c = static_cast<unsigned char>(name[i]);
    if (!std::isdigit(c)) return out;
    ordinal = ordinal * 10 + static_cast<size_t>(c - '0');
  }

  out.has_ordinal  = true;
  out.ordinal      = ordinal;
  out.stripped_name = name.substr(sep + 2);
  return out;
}

using math::kIdentityMat4;
using math::Mat4;

Mat4 nodeLocalMat4(const Json& node) {
  if (node.contains("matrix")) {
    Mat4 out{};
    const auto& a = node.at("matrix");
    for (int i = 0; i < 16; ++i) out.m[i] = a.at(i).get<float>();
    return out;
  }
  ArxVector3 t{0, 0, 0};
  ArxQuat r = math::kIdentityQuat;
  ArxVector3 s{1, 1, 1};
  if (node.contains("translation")) {
    const auto& a = node.at("translation");
    t             = {a.at(0).get<float>(), a.at(1).get<float>(), a.at(2).get<float>()};
  }
  if (node.contains("rotation")) {
    const auto& a = node.at("rotation");
    // glTF quaternion order is [x, y, z, w]
    r = {a.at(3).get<float>(), a.at(0).get<float>(), a.at(1).get<float>(), a.at(2).get<float>()};
  }
  if (node.contains("scale")) {
    const auto& a = node.at("scale");
    s             = {a.at(0).get<float>(), a.at(1).get<float>(), a.at(2).get<float>()};
  }
  return math::fromTrs(t, r, s);
}

// --- GLB container parse ---

ArxReturnCode parseGlbContainer(std::span<const uint8_t> glb, std::span<const uint8_t>& json_out,
                                std::span<const uint8_t>& bin_out) {
  ReadCursor c(glb.data(), glb.size());
  uint32_t magic = 0, version = 0, total_len = 0;
  c.read(magic).read(version).read(total_len);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (magic != kGlbMagic) return ARX_INVALID_IDENTIFIER;
  if (version != kGlbVersion) {
    log(ARX_LOG_ERROR, std::format("GLB import: unsupported version {}", version));
    return ARX_GLB_BAD_FORMAT;
  }
  if (total_len > glb.size()) {
    log(ARX_LOG_ERROR, std::format("GLB import: declared length {} exceeds buffer {}", total_len, glb.size()));
    return ARX_GLB_BAD_FORMAT;
  }
  if (total_len < glb.size()) {
    log(ARX_LOG_ERROR, std::format("GLB import: declared length {} is shorter than buffer {}", total_len, glb.size()));
    return ARX_GLB_BAD_FORMAT;
  }

  uint32_t json_len = 0, json_type = 0;
  c.read(json_len).read(json_type);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (json_type != kChunkTypeJson) {
    log(ARX_LOG_ERROR, "GLB import: first chunk is not JSON");
    return ARX_GLB_BAD_FORMAT;
  }
  if (json_len > c.remaining()) return ARX_UNEXPECTED_EOF;
  size_t json_off = glb.size() - c.remaining();
  json_out        = std::span(glb.data() + json_off, json_len);
  c.skip(json_len);
  if (!c) return ARX_UNEXPECTED_EOF;

  bin_out = {};
  if (c.remaining() > 0) {
    uint32_t bin_len = 0, bin_type = 0;
    c.read(bin_len).read(bin_type);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (bin_len > c.remaining()) return ARX_UNEXPECTED_EOF;
    if (bin_type == kChunkTypeBin) {
      size_t bin_off = glb.size() - c.remaining();
      bin_out        = std::span(glb.data() + bin_off, bin_len);
    } else {
      log(ARX_LOG_INFO, std::format("GLB import: ignoring chunk type 0x{:x}", bin_type));
    }
  }
  return ARX_OK;
}

// --- Accessor resolution ---

struct AccessorView {
  int component_type = 0;
  std::string type;
  size_t count          = 0;
  bool normalized       = false;
  size_t byte_offset    = 0;  // absolute offset into bin
  size_t byte_stride    = 0;
  size_t component_size = 0;
  size_t components     = 0;
  std::span<const uint8_t> bin;
};

int compSize(int ct) {
  switch (ct) {
    case kCompByte:
    case kCompUByte:
      return 1;
    case kCompShort:
    case kCompUShort:
      return 2;
    case kCompUInt:
    case kCompFloat:
      return 4;
    default:
      return 0;
  }
}

int componentsPerType(const std::string& t) {
  if (t == "SCALAR") return 1;
  if (t == "VEC2") return 2;
  if (t == "VEC3") return 3;
  if (t == "VEC4") return 4;
  if (t == "MAT4") return 16;
  return 0;
}

bool checkedAdd(size_t a, size_t b, size_t& out) {
  if (a > SIZE_MAX - b) return false;
  out = a + b;
  return true;
}

bool checkedMul(size_t a, size_t b, size_t& out) {
  if (a != 0 && b > SIZE_MAX / a) return false;
  out = a * b;
  return true;
}

ArxReturnCode resolveAccessor(const Json& gltf, std::span<const uint8_t> bin, size_t acc_idx, AccessorView& out) {
  if (!gltf.contains("accessors") || acc_idx >= gltf["accessors"].size()) return ARX_GLB_BAD_FORMAT;
  const Json& acc = gltf["accessors"][acc_idx];
  if (acc.contains("sparse")) {
    log(ARX_LOG_ERROR, "GLB import: sparse accessors not supported");
    return ARX_GLB_UNSUPPORTED_FEATURE;
  }
  if (!acc.contains("bufferView")) {
    log(ARX_LOG_ERROR, "GLB import: accessor without bufferView not supported");
    return ARX_GLB_UNSUPPORTED_FEATURE;
  }

  out.component_type = acc.at("componentType").get<int>();
  out.type           = acc.at("type").get<std::string>();
  out.count          = acc.at("count").get<size_t>();
  out.normalized     = acc.value("normalized", false);
  out.component_size = compSize(out.component_type);
  out.components     = componentsPerType(out.type);
  if (out.component_size == 0 || out.components == 0) {
    log(ARX_LOG_ERROR, std::format("GLB import: bad componentType={} or type='{}'", out.component_type, out.type));
    return ARX_GLB_BAD_FORMAT;
  }

  size_t bv_idx = acc.at("bufferView").get<size_t>();
  if (!gltf.contains("bufferViews") || bv_idx >= gltf["bufferViews"].size()) return ARX_GLB_BAD_FORMAT;
  const Json& bv = gltf["bufferViews"][bv_idx];

  size_t buffer_idx = bv.at("buffer").get<size_t>();
  if (buffer_idx != 0) {
    log(ARX_LOG_ERROR, "GLB import: only buffer 0 (BIN chunk) supported");
    return ARX_GLB_UNSUPPORTED_FEATURE;
  }
  if (gltf.contains("buffers") && buffer_idx < gltf["buffers"].size()) {
    const Json& buf = gltf["buffers"][buffer_idx];
    if (buf.contains("uri")) {
      log(ARX_LOG_ERROR, "GLB import: external buffer URIs not supported");
      return ARX_GLB_UNSUPPORTED_FEATURE;
    }
  }

  size_t bv_off    = bv.value("byteOffset", size_t{0});
  size_t bv_len    = bv.at("byteLength").get<size_t>();
  size_t acc_off   = acc.value("byteOffset", size_t{0});
  size_t elem_pack = out.component_size * out.components;
  out.byte_stride  = bv.value("byteStride", elem_pack);

  if (out.byte_stride < elem_pack) return ARX_GLB_BAD_FORMAT;

  size_t bv_end = 0;
  if (!checkedAdd(bv_off, bv_len, bv_end) || bv_end > bin.size()) return ARX_GLB_BAD_FORMAT;
  if (!checkedAdd(bv_off, acc_off, out.byte_offset)) return ARX_GLB_BAD_FORMAT;
  if (out.count == 0) {
    out.bin = bin;
    return ARX_OK;
  }
  size_t last_stride = 0;
  size_t last_off    = 0;
  if (!checkedMul(out.count - 1, out.byte_stride, last_stride)) return ARX_GLB_BAD_FORMAT;
  if (!checkedAdd(out.byte_offset, last_stride, last_off)) return ARX_GLB_BAD_FORMAT;
  if (!checkedAdd(last_off, elem_pack, last_off)) return ARX_GLB_BAD_FORMAT;
  if (last_off > bv_end) return ARX_GLB_BAD_FORMAT;
  out.bin = bin;
  return ARX_OK;
}

// up to 16 floats; handles normalized int formats per glTF spec
void decodeFloats(const AccessorView& a, size_t i, float* out) {
  const uint8_t* p = a.bin.data() + a.byte_offset + i * a.byte_stride;
  for (size_t c = 0; c < a.components; ++c) {
    const uint8_t* cp = p + c * a.component_size;
    float v           = 0.0f;
    switch (a.component_type) {
      case kCompByte: {
        int8_t x;
        std::memcpy(&x, cp, 1);
        v = a.normalized ? std::max(x / 127.0f, -1.0f) : float(x);
        break;
      }
      case kCompUByte: {
        uint8_t x;
        std::memcpy(&x, cp, 1);
        v = a.normalized ? x / 255.0f : float(x);
        break;
      }
      case kCompShort: {
        int16_t x;
        std::memcpy(&x, cp, 2);
        v = a.normalized ? std::max(x / 32767.0f, -1.0f) : float(x);
        break;
      }
      case kCompUShort: {
        uint16_t x;
        std::memcpy(&x, cp, 2);
        v = a.normalized ? x / 65535.0f : float(x);
        break;
      }
      case kCompUInt: {
        uint32_t x;
        std::memcpy(&x, cp, 4);
        v = float(x);
        break;
      }
      case kCompFloat: {
        std::memcpy(&v, cp, 4);
        break;
      }
      default:
        break;  // SILENT: resolveAccessor validates component_type upstream
    }
    out[c] = v;
  }
}

uint32_t decodeIndex(const AccessorView& a, size_t i) {
  const uint8_t* cp = a.bin.data() + a.byte_offset + i * a.byte_stride;
  switch (a.component_type) {
    case kCompUByte: {
      uint8_t v;
      std::memcpy(&v, cp, 1);
      return v;
    }
    case kCompUShort: {
      uint16_t v;
      std::memcpy(&v, cp, 2);
      return v;
    }
    case kCompUInt: {
      uint32_t v;
      std::memcpy(&v, cp, 4);
      return v;
    }
    default:
      return 0;
  }
}

uint32_t decodeJointComponent(const AccessorView& a, size_t i, int corner) {
  const uint8_t* cp = a.bin.data() + a.byte_offset + i * a.byte_stride + corner * a.component_size;
  switch (a.component_type) {
    case kCompUByte: {
      uint8_t v;
      std::memcpy(&v, cp, 1);
      return v;
    }
    case kCompUShort: {
      uint16_t v;
      std::memcpy(&v, cp, 2);
      return v;
    }
    default:
      return 0;
  }
}

// --- Selection names ---

using SelectionRegistry = std::vector<ftl::Selection>;

ftl::Selection& findOrAddSelection(SelectionRegistry& selections, std::string_view name) {
  for (auto& sel : selections) {
    if (std::string_view(sel.name) == name) return sel;
  }

  ftl::Selection sel{};
  size_t n = std::min(name.size(), sizeof(sel.name) - 1);
  if (name.size() > n) log(ARX_LOG_WARN, std::format("GLB import: selection name '{}' truncated to {} chars", name, n));
  std::memcpy(sel.name, name.data(), n);
  selections.push_back(std::move(sel));
  return selections.back();
}

// Blender roundtrip writes COLOR_n as normalized integers
bool isStandardNonSelectionSemantic(std::string_view key) {
  return key == "TANGENT" || key.starts_with("WEIGHTS_") || key.starts_with("JOINTS_");
}

// Casing nuances only survive via extras
std::string decodeSelectionName(std::string_view key) {
  std::string out;
  size_t start = key.starts_with("_") ? 1u : 0u;
  out.reserve(key.size() - start);
  for (size_t i = start; i < key.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(key[i]);
    out.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c));
  }
  return out;
}

// --- Scene/node tree world transforms ---

struct NodeWorld {
  std::vector<Mat4> world;
  std::vector<int32_t> parent;  // -1 if root or unreached
  std::vector<bool> reached;
};

ArxReturnCode computeNodeWorlds(const Json& gltf, NodeWorld& nw) {
  size_t n_nodes = gltf.contains("nodes") ? gltf["nodes"].size() : 0;
  nw.world.assign(n_nodes, kIdentityMat4);
  nw.parent.assign(n_nodes, -1);
  nw.reached.assign(n_nodes, false);

  size_t scene_idx = gltf.value("scene", size_t{0});
  if (!gltf.contains("scenes") || scene_idx >= gltf["scenes"].size()) return ARX_GLB_BAD_FORMAT;
  const Json& scene = gltf["scenes"][scene_idx];

  std::vector<size_t> queue;
  if (scene.contains("nodes")) {
    for (const auto& v : scene["nodes"]) {
      size_t n = v.get<size_t>();
      if (n >= n_nodes) return ARX_GLB_BAD_FORMAT;
      if (nw.reached[n]) {
        log(ARX_LOG_ERROR, "GLB import: scene root listed twice");
        return ARX_GLB_BAD_FORMAT;
      }
      nw.world[n]   = nodeLocalMat4(gltf["nodes"][n]);
      nw.reached[n] = true;
      queue.push_back(n);
    }
  }

  for (size_t qhead = 0; qhead < queue.size(); ++qhead) {
    size_t parent_idx = queue[qhead];
    const Json& node  = gltf["nodes"][parent_idx];
    if (!node.contains("children")) continue;
    for (const auto& cv : node["children"]) {
      size_t child = cv.get<size_t>();
      if (child >= n_nodes) return ARX_GLB_BAD_FORMAT;
      if (nw.reached[child]) {
        log(ARX_LOG_ERROR, std::format("GLB import: node {} reached twice (cycle or DAG)", child));
        return ARX_GLB_BAD_FORMAT;
      }
      nw.parent[child]  = static_cast<int32_t>(parent_idx);
      nw.reached[child] = true;
      Mat4 local        = nodeLocalMat4(gltf["nodes"][child]);
      nw.world[child]   = nw.world[parent_idx] * local;
      queue.push_back(child);
    }
  }
  return ARX_OK;
}

// --- Material flags ---

struct MatFlags {
  FaceType extra = 0;  // FACE_BIT_TRANS, FACE_BIT_DOUBLESIDED, ...
  float transval = 0.0f;
};

// walks pbr.baseColorTexture -> textures -> images; '/' -> '\' for Arx
std::string resolveMaterialUri(const Json& gltf, const Json& mat) {
  if (!mat.contains("pbrMetallicRoughness")) return {};
  const auto& pbr = mat["pbrMetallicRoughness"];
  if (!pbr.contains("baseColorTexture")) return {};
  const auto& bct = pbr["baseColorTexture"];
  if (!bct.contains("index")) return {};
  int32_t tex_idx = bct.at("index").get<int32_t>();
  if (!gltf.contains("textures") || tex_idx < 0 || static_cast<size_t>(tex_idx) >= gltf["textures"].size()) return {};
  const auto& tex = gltf["textures"][tex_idx];
  if (!tex.contains("source")) return {};
  int32_t img_idx = tex.at("source").get<int32_t>();
  if (!gltf.contains("images") || img_idx < 0 || static_cast<size_t>(img_idx) >= gltf["images"].size()) return {};
  const auto& img = gltf["images"][img_idx];
  if (!img.contains("uri")) return {};
  std::string uri = img.at("uri").get<std::string>();
  for (auto& c : uri)
    if (c == '/') c = '\\';
  return uri;
}

void buildMaterialContainers(const Json& gltf, ftl::Data& out, std::vector<MatFlags>& flags_out,
                             std::vector<int16_t>& material_tex_ids) {
  if (!gltf.contains("materials")) return;
  size_t nm = gltf["materials"].size();
  out.texture_containers.reserve(nm);
  flags_out.reserve(nm);
  material_tex_ids.reserve(nm);
  std::unordered_map<std::string, int16_t> tex_key_to_idx;
  for (size_t i = 0; i < nm; ++i) {
    const Json& mat  = gltf["materials"][i];
    std::string name = mat.value("name", std::string{});

    auto [decoded_stem, decoded_flags] = decodeMatName(name);

    // prefer baseColorTexture URI for DCC-exported GLBs; fall back to the decoded stem
    std::string filename = resolveMaterialUri(gltf, mat);
    std::string key;
    if (!decoded_stem.empty()) {
      key = std::string(decoded_stem);
    } else if (!filename.empty()) {
      key = filename;
    } else if (name.empty()) {
      key = std::format("material_{}", i);
    }

    int16_t tex_id = kFtlTextureNone;
    if (!key.empty()) {
      auto it = tex_key_to_idx.find(key);
      if (it != tex_key_to_idx.end()) {
        tex_id = it->second;
      } else {
        if (filename.empty()) filename = std::string(decoded_stem);
        if (filename.empty()) filename = key;

        ftl::TextureContainer tc{};
        size_t copy_n = std::min(filename.size(), sizeof(tc.filename) - 1);
        std::memcpy(tc.filename, filename.data(), copy_n);
        tex_id              = static_cast<int16_t>(out.texture_containers.size());
        tex_key_to_idx[key] = tex_id;
        out.texture_containers.push_back(tc);
      }
    }
    material_tex_ids.push_back(tex_id);

    MatFlags f{};
    f.extra = decoded_flags;
    if (mat.value("doubleSided", false)) f.extra |= kFaceBitDoublesided;
    std::string mode = mat.value("alphaMode", std::string{"OPAQUE"});
    if (mode == "BLEND") {
      f.extra |= kFaceBitTrans;
      float a = 1.0f;
      if (mat.contains("pbrMetallicRoughness") && mat["pbrMetallicRoughness"].contains("baseColorFactor")) {
        const auto& bcf = mat["pbrMetallicRoughness"]["baseColorFactor"];
        if (bcf.size() >= 4) a = bcf.at(3).get<float>();
      }
      f.transval = 1.0f - a;
    } else if (mode == "MASK") {
      f.extra |= kFaceBitTrans;
      f.transval = mat.value("alphaCutoff", 0.5f);
    }
    flags_out.push_back(f);
  }
}

// --- Primitive triangulation ---

bool triangulateMode(int mode, const std::vector<uint32_t>& src, std::vector<uint32_t>& tris) {
  size_t n = src.size();
  switch (mode) {
    case kModeTriangles:
      if (n % 3 != 0) {
        log(ARX_LOG_WARN, "GLB import: TRIANGLES count not multiple of 3, truncating");
        n -= n % 3;
      }
      tris.insert(tris.end(), src.begin(), src.begin() + static_cast<std::ptrdiff_t>(n));
      return true;
    case kModeTriangleStrip:
      if (n < 3) return true;
      for (size_t i = 0; i + 2 < n; ++i) {
        if (i & 1u) {
          tris.push_back(src[i + 1]);
          tris.push_back(src[i]);
          tris.push_back(src[i + 2]);
        } else {
          tris.push_back(src[i]);
          tris.push_back(src[i + 1]);
          tris.push_back(src[i + 2]);
        }
      }
      return true;
    case kModeTriangleFan:
      if (n < 3) return true;
      for (size_t i = 1; i + 1 < n; ++i) {
        tris.push_back(src[0]);
        tris.push_back(src[i]);
        tris.push_back(src[i + 1]);
      }
      return true;
    default:
      log(ARX_LOG_WARN, std::format("GLB import: unsupported primitive mode {}, skipping", mode));
      return false;
  }
}

// --- Ingest mesh primitives ---

struct VertKey {
  size_t mesh_node;
  size_t prim;
  uint32_t glb_vert;
};
struct VertKeyHash {
  size_t operator()(const VertKey& k) const noexcept {
    size_t h = std::hash<size_t>{}(k.mesh_node);
    h ^= std::hash<size_t>{}(k.prim) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= std::hash<uint32_t>{}(k.glb_vert) + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
  }
};
struct VertKeyEq {
  bool operator()(const VertKey& a, const VertKey& b) const noexcept {
    return a.mesh_node == b.mesh_node && a.prim == b.prim && a.glb_vert == b.glb_vert;
  }
};

struct VertJointInfo {
  int32_t dominant_joint = -1;  // unified joint index; -1 = no skin influence
};

ArxReturnCode ingestPrimitives(const Json& gltf, std::span<const uint8_t> bin, const NodeWorld& nw,
                               const std::vector<std::vector<int32_t>>& skin_to_unified,
                               const std::vector<MatFlags>& mat_flags,
                               std::span<const int16_t> material_tex_ids,
                               std::span<const std::string> extras_names, ftl::Data& out,
                               std::vector<VertJointInfo>& vj_out, SelectionRegistry& sel_out) {
  size_t n_nodes = nw.world.size();
  std::unordered_map<VertKey, uint16_t, VertKeyHash, VertKeyEq> dedupe;

  for (size_t node_idx = 0; node_idx < n_nodes; ++node_idx) {
    if (!nw.reached[node_idx]) continue;
    const Json& node = gltf["nodes"][node_idx];
    if (!node.contains("mesh")) continue;

    size_t mesh_idx = node.at("mesh").get<size_t>();
    if (!gltf.contains("meshes") || mesh_idx >= gltf["meshes"].size()) return ARX_GLB_BAD_FORMAT;

    int32_t node_skin = node.contains("skin") ? node.at("skin").get<int32_t>() : -1;
    if (node_skin >= 0 && static_cast<size_t>(node_skin) >= skin_to_unified.size()) return ARX_GLB_BAD_FORMAT;

    // world transform applies to positions AND normals
    if (!math::isRotationUniformScale(nw.world[node_idx])) {
      log(ARX_LOG_ERROR,
          std::format("GLB import: mesh node {} world transform has non-uniform scale or shear", node_idx));
      return ARX_GLB_NON_UNIFORM_SCALE;
    }

    const Json& mesh = gltf["meshes"][mesh_idx];
    if (!mesh.contains("primitives")) continue;

    for (size_t prim_i = 0; prim_i < mesh["primitives"].size(); ++prim_i) {
      const Json& prim = mesh["primitives"][prim_i];
      int mode         = prim.value("mode", kModeTriangles);

      if (!prim.contains("attributes")) continue;
      const Json& attrs = prim["attributes"];
      if (!attrs.contains("POSITION")) continue;

      AccessorView pos_av;
      ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, attrs.at("POSITION").get<size_t>(), pos_av));
      if (pos_av.type != "VEC3" || pos_av.component_type != kCompFloat) {
        log(ARX_LOG_ERROR, "GLB import: POSITION must be float VEC3");
        return ARX_GLB_BAD_FORMAT;
      }

      AccessorView nrm_av;
      bool has_nrm = attrs.contains("NORMAL");
      if (has_nrm) {
        ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, attrs.at("NORMAL").get<size_t>(), nrm_av));
        if (nrm_av.type != "VEC3" || nrm_av.component_type != kCompFloat) {
          log(ARX_LOG_ERROR, "GLB import: NORMAL must be float VEC3");
          return ARX_GLB_BAD_FORMAT;
        }
      }

      AccessorView uv_av;
      bool has_uv = attrs.contains("TEXCOORD_0");
      if (has_uv) {
        ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, attrs.at("TEXCOORD_0").get<size_t>(), uv_av));
        if (uv_av.type != "VEC2") {
          log(ARX_LOG_ERROR, "GLB import: TEXCOORD_0 must be VEC2");
          return ARX_GLB_BAD_FORMAT;
        }
      }

      AccessorView j_av, w_av;
      bool has_skin_attrs = attrs.contains("JOINTS_0") && attrs.contains("WEIGHTS_0");
      if (has_skin_attrs) {
        ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, attrs.at("JOINTS_0").get<size_t>(), j_av));
        ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, attrs.at("WEIGHTS_0").get<size_t>(), w_av));
        bool good_joints =
            j_av.type == "VEC4" && (j_av.component_type == kCompUByte || j_av.component_type == kCompUShort);
        bool good_weights =
            w_av.type == "VEC4" &&
            (w_av.component_type == kCompFloat ||
             (w_av.normalized && (w_av.component_type == kCompUByte || w_av.component_type == kCompUShort)));
        if (!good_joints || !good_weights) {
          log(ARX_LOG_ERROR, "GLB import: JOINTS_0/WEIGHTS_0 must be VEC4");
          return ARX_GLB_BAD_FORMAT;
        }
      }

      // VEC4 masks; ordered_json keeps positional names aligned
      struct SelAttr {
        std::string ftl_name;
        AccessorView av;
      };
      std::vector<SelAttr> sel_attrs;
      size_t slot_index = 0;
      for (auto it = attrs.begin(); it != attrs.end(); ++it) {
        const std::string& key = it.key();
        AccessorView sel_av;
        if (resolveAccessor(gltf, bin, it.value().get<size_t>(), sel_av) != ARX_OK) continue;
        if (sel_av.type != "VEC4") continue;
        if (sel_av.component_type != kCompFloat && !sel_av.normalized) continue;
        if (isStandardNonSelectionSemantic(key)) continue;
        if (sel_av.count != pos_av.count) {
          log(ARX_LOG_WARN, std::format("GLB import: attribute '{}' count {} != POSITION count {}; skipping", key,
                                        sel_av.count, pos_av.count));
          continue;
        }
        std::string ftl_name;
        if (slot_index < extras_names.size() && !extras_names[slot_index].empty())
          ftl_name = extras_names[slot_index];
        else
          ftl_name = decodeSelectionName(key);
        if (!ftl_name.empty()) findOrAddSelection(sel_out, ftl_name);
        sel_attrs.push_back({std::move(ftl_name), sel_av});
        ++slot_index;
      }
      if (!extras_names.empty() && slot_index > extras_names.size()) {
        log(ARX_LOG_WARN, std::format("GLB import: {} selection slot(s) but extras list has {} entries; "
                                      "extra slots fall back to decoded key names",
                                      slot_index, extras_names.size()));
      }

      std::vector<uint32_t> src_indices;
      if (prim.contains("indices")) {
        AccessorView idx_av;
        ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, prim.at("indices").get<size_t>(), idx_av));
        if (idx_av.type != "SCALAR" || (idx_av.component_type != kCompUByte && idx_av.component_type != kCompUShort &&
                                        idx_av.component_type != kCompUInt)) {
          log(ARX_LOG_ERROR, "GLB import: indices must be unsigned integer SCALAR");
          return ARX_GLB_BAD_FORMAT;
        }
        src_indices.reserve(idx_av.count);
        for (size_t i = 0; i < idx_av.count; ++i) src_indices.push_back(decodeIndex(idx_av, i));
      } else {
        src_indices.reserve(pos_av.count);
        for (size_t i = 0; i < pos_av.count; ++i) src_indices.push_back(static_cast<uint32_t>(i));
      }

      std::vector<uint32_t> tris;
      if (!triangulateMode(mode, src_indices, tris)) continue;
      if (tris.size() % 3 != 0) continue;

      int32_t prim_mat = prim.contains("material") ? prim.at("material").get<int32_t>() : -1;
      int16_t face_tex = (prim_mat >= 0 && static_cast<size_t>(prim_mat) < material_tex_ids.size())
                             ? material_tex_ids[prim_mat]
                             : kFtlTextureNone;
      MatFlags mf{};
      if (prim_mat >= 0 && static_cast<size_t>(prim_mat) < mat_flags.size()) mf = mat_flags[prim_mat];

      const Mat4& world = nw.world[node_idx];

      auto get_or_make_vertex = [&](uint32_t glb_v, uint16_t& ftl_idx) -> ArxReturnCode {
        VertKey key{node_idx, prim_i, glb_v};
        auto it = dedupe.find(key);
        if (it != dedupe.end()) {
          ftl_idx = it->second;
          return ARX_OK;
        }
        if (out.vertices.size() + 1 >= kFtlMaxVertices) {
          log(ARX_LOG_ERROR, "GLB import: vertex count exceeds 16-bit ceiling");
          return ARX_GLB_TOO_MANY_VERTICES;
        }
        if (glb_v >= pos_av.count) return ARX_GLB_BAD_FORMAT;

        float p[3]{};
        decodeFloats(pos_av, glb_v, p);
        ArxVector3 pos = math::xformPoint(world, ArxVector3{p[0], p[1], p[2]});

        ArxVector3 nrm{0, 0, 1};
        if (has_nrm) {
          float n[3]{};
          decodeFloats(nrm_av, glb_v, n);
          nrm = math::normalizeOr(math::xformDir(world, ArxVector3{n[0], n[1], n[2]}), {0, 0, 1});
        }

        ftl::Vertex v{};
        v.position = pos;
        v.normal   = nrm;
        ftl_idx    = static_cast<uint16_t>(out.vertices.size());
        out.vertices.push_back(v);

        VertJointInfo vji{};
        if (has_skin_attrs) {
          if (glb_v >= w_av.count) return ARX_GLB_BAD_FORMAT;
          float w[4]{};
          decodeFloats(w_av, glb_v, w);
          int best_c   = -1;
          float best_w = 0.0f;
          for (int c = 0; c < 4; ++c)
            if (w[c] > best_w) {
              best_w = w[c];
              best_c = c;
            }
          if (best_c >= 0 && best_w > 0.0f && node_skin >= 0) {
            uint32_t j_local  = decodeJointComponent(j_av, glb_v, best_c);
            const auto& remap = skin_to_unified[node_skin];
            if (j_local >= remap.size()) return ARX_GLB_BAD_FORMAT;
            vji.dominant_joint = remap[j_local];
            if (best_w < 0.7f)
              log(ARX_LOG_WARN, std::format("GLB import: vertex {} dominant weight {:.3f} < 0.7; secondary "
                                            "influences discarded",
                                            ftl_idx, best_w));
          }
        }
        vj_out.push_back(vji);

        // R > 0.5 tolerates paint slop
        for (const auto& s : sel_attrs) {
          if (s.ftl_name.empty()) continue;
          float rgba[4]{};
          decodeFloats(s.av, glb_v, rgba);
          if (rgba[0] > 0.5f) findOrAddSelection(sel_out, s.ftl_name).selected.push_back(static_cast<int32_t>(ftl_idx));
        }

        dedupe[key] = ftl_idx;
        return ARX_OK;
      };

      auto fetch_uv = [&](uint32_t glb_v) -> ArxVector3 {
        if (!has_uv || glb_v >= uv_av.count) return {0, 0, 0};
        float t[2]{};
        decodeFloats(uv_av, glb_v, t);
        // FTL (DDS/DirectX) and glTF 2.0 share V=0-top; no flip needed
        return {t[0], t[1], 0.0f};
      };

      for (size_t t = 0; t + 2 < tris.size(); t += 3) {
        uint32_t g0 = tris[t], g1 = tris[t + 1], g2 = tris[t + 2];
        if (g0 == g1 || g1 == g2 || g0 == g2) continue;

        uint16_t v0, v1, v2;
        ARX_RETURN_IF_ERR(get_or_make_vertex(g0, v0));
        ARX_RETURN_IF_ERR(get_or_make_vertex(g1, v1));
        ARX_RETURN_IF_ERR(get_or_make_vertex(g2, v2));

        ArxVector3 uv0 = fetch_uv(g0);
        ArxVector3 uv1 = fetch_uv(g1);
        ArxVector3 uv2 = fetch_uv(g2);

        const ArxVector3& p0 = out.vertices[v0].position;
        const ArxVector3& p1 = out.vertices[v1].position;
        const ArxVector3& p2 = out.vertices[v2].position;
        ArxVector3 e1{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        ArxVector3 e2{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        ArxVector3 n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        float l = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (l == 0.0f) continue;
        n.x /= l;
        n.y /= l;
        n.z /= l;

        if (out.faces.size() >= kFtlMaxFaces) {
          log(ARX_LOG_ERROR, "GLB import: face count exceeds limit");
          return ARX_GLB_BAD_FORMAT;
        }
        ftl::Face f{};
        f.vertex_idx = {v0, v1, v2};
        f.texture_id = face_tex;
        f.type       = mf.extra;
        f.transval   = mf.transval;
        f.u          = {uv0.x, uv1.x, uv2.x};
        f.v          = {uv0.y, uv1.y, uv2.y};
        f.norm       = n;
        out.faces.push_back(f);
      }
    }
  }
  return ARX_OK;
}

// --- Skeleton: merge joints across skins, topo order, parent map, bind world positions ---

struct BoneInfo {
  size_t joint_node;
  int32_t parent_topo;        // -1 if root
  ArxVector3 world_bind_pos;  // Y-flipped pivot vertex position
  Mat4 bind_world_matrix;     // M_owner * inverse(IBM_owner), pre-Y-flip
  std::string name;           // lowercase; defaults to "bone_<i>"
};

struct UnifiedJoints {
  std::vector<size_t> joint_nodes;         // unified -> glTF node
  std::vector<Mat4> joint_ibm;             // first-skin IBM
  std::vector<int32_t> joint_owner_skin;   // first skin that contributed
  std::vector<size_t> mesh_node_for_skin;  // SIZE_MAX if none
  std::unordered_map<size_t, int32_t> node_to_unified;
};

// first-wins; needed to cancel Blender's per-skin mesh-transform bake
static void pickMeshNodesForSkins(const Json& gltf, const NodeWorld& nw, size_t n_skins,
                                  std::vector<size_t>& mesh_node_for_skin) {
  mesh_node_for_skin.assign(n_skins, SIZE_MAX);
  for (size_t n = 0; n < nw.world.size(); ++n) {
    if (!nw.reached[n]) continue;
    const Json& node = gltf["nodes"][n];
    if (!node.contains("mesh") || !node.contains("skin")) continue;
    int32_t s = node.at("skin").get<int32_t>();
    if (s < 0 || static_cast<size_t>(s) >= n_skins) continue;
    if (mesh_node_for_skin[s] == SIZE_MAX) {
      mesh_node_for_skin[s] = n;
    } else {
      log(ARX_LOG_WARN, std::format("GLB import: skin {} referenced by multiple mesh nodes; using first", s));
    }
  }
}

static ArxReturnCode mergeSkinJoints(const Json& gltf, std::span<const uint8_t> bin, const NodeWorld& nw,
                                     UnifiedJoints& unified, std::vector<std::vector<int32_t>>& skin_to_unified_out) {
  size_t n_skins = gltf.contains("skins") ? gltf["skins"].size() : 0;
  skin_to_unified_out.assign(n_skins, {});
  pickMeshNodesForSkins(gltf, nw, n_skins, unified.mesh_node_for_skin);

  for (size_t s = 0; s < n_skins; ++s) {
    const Json& skin = gltf["skins"][s];
    if (!skin.contains("joints")) return ARX_GLB_BAD_FORMAT;
    const Json& joints = skin["joints"];
    size_t nj          = joints.size();
    if (nj == 0) continue;

    if (unified.mesh_node_for_skin[s] == SIZE_MAX) {
      log(ARX_LOG_ERROR, std::format("GLB import: skin {} has joints but no mesh node references it", s));
      return ARX_GLB_BAD_FORMAT;
    }

    std::vector<Mat4> ibm(nj, kIdentityMat4);
    if (skin.contains("inverseBindMatrices")) {
      AccessorView av;
      ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, skin.at("inverseBindMatrices").get<size_t>(), av));
      if (av.type != "MAT4" || av.component_type != kCompFloat || av.count < nj) {
        log(ARX_LOG_ERROR, "GLB import: inverseBindMatrices must be float MAT4 of >= joint count");
        return ARX_GLB_BAD_FORMAT;
      }
      for (size_t j = 0; j < nj; ++j) decodeFloats(av, j, ibm[j].m);
    }

    skin_to_unified_out[s].assign(nj, -1);

    for (size_t j = 0; j < nj; ++j) {
      size_t n = joints[j].get<size_t>();
      if (n >= nw.world.size()) return ARX_GLB_BAD_FORMAT;
      auto it = unified.node_to_unified.find(n);
      int32_t u;
      if (it == unified.node_to_unified.end()) {
        u = static_cast<int32_t>(unified.joint_nodes.size());
        unified.joint_nodes.push_back(n);
        unified.joint_ibm.push_back(ibm[j]);
        unified.joint_owner_skin.push_back(static_cast<int32_t>(s));
        unified.node_to_unified[n] = u;
      } else {
        u = it->second;
      }
      skin_to_unified_out[s][j] = u;

      // diagnostic: at bind, BFS world == M.world * inverse(IBM).translation
      // disagreement => file posed at frame 0, not bind
      if (auto inv = math::inverseAffine(ibm[j])) {
        ArxVector3 expected = math::xformPoint(nw.world[unified.mesh_node_for_skin[s]], math::translation(*inv));
        ArxVector3 jw_t     = math::translation(nw.world[n]);
        ArxVector3 delta    = jw_t - expected;
        if (math::lengthSquared(delta) > 1e-6f) {
          log(ARX_LOG_DEBUG, std::format("GLB import: joint node {} (skin {}) BFS world ({},{},{}) != "
                                         "M.world * inverse(IBM) ({},{},{})",
                                         n, s, jw_t.x, jw_t.y, jw_t.z, expected.x, expected.y, expected.z));
        }
      }
    }
  }
  return ARX_OK;
}

// bind transform = M_owner * inverse(IBM); rejects non-uniform scale (can't be cancelled)
static ArxReturnCode computeBindWorlds(const UnifiedJoints& unified, const NodeWorld& nw,
                                       std::vector<ArxVector3>& bind_world, std::vector<Mat4>& bind_world_matrix) {
  size_t nj_unified = unified.joint_nodes.size();
  bind_world.assign(nj_unified, {});
  bind_world_matrix.assign(nj_unified, {});
  for (size_t u = 0; u < nj_unified; ++u) {
    if (!math::isRotationUniformScale(unified.joint_ibm[u])) {
      log(ARX_LOG_ERROR,
          std::format("GLB import: IBM for joint {} has non-uniform scale or shear", unified.joint_nodes[u]));
      return ARX_GLB_NON_UNIFORM_SCALE;
    }
    auto inv = math::inverseAffine(unified.joint_ibm[u]);
    if (!inv) {
      log(ARX_LOG_ERROR, std::format("GLB import: IBM for joint {} is singular", unified.joint_nodes[u]));
      return ARX_GLB_BAD_FORMAT;
    }
    const Mat4& m        = nw.world[unified.mesh_node_for_skin[unified.joint_owner_skin[u]]];
    bind_world_matrix[u] = m * *inv;
    bind_world[u]        = math::translation(bind_world_matrix[u]);
  }
  return ARX_OK;
}

// nearest-joint-parent walk; enforces single root. Multi-skin roots -> MULTIPLE_SKINS
static ArxReturnCode computeJointParents(const UnifiedJoints& unified, const NodeWorld& nw, size_t n_skins,
                                         std::vector<int32_t>& joint_parent) {
  size_t nj_unified = unified.joint_nodes.size();
  joint_parent.assign(nj_unified, -1);
  for (size_t u = 0; u < nj_unified; ++u) {
    int32_t cur = nw.parent[unified.joint_nodes[u]];
    while (cur >= 0) {
      auto it = unified.node_to_unified.find(static_cast<size_t>(cur));
      if (it != unified.node_to_unified.end()) {
        joint_parent[u] = it->second;
        break;
      }
      cur = nw.parent[cur];
    }
  }

  int root_count = 0;
  for (int32_t p : joint_parent)
    if (p < 0) ++root_count;
  bool from_multiple_skins = (n_skins > 1);
  if (root_count == 0) {
    log(ARX_LOG_ERROR, "GLB import: skeleton has no root joint");
    return from_multiple_skins ? ARX_GLB_MULTIPLE_SKINS : ARX_FTL_MULTIPLE_ROOTS;
  }
  if (root_count > 1) {
    log(ARX_LOG_ERROR, std::format("GLB import: unified skeleton has {} root joints, only one allowed", root_count));
    return from_multiple_skins ? ARX_GLB_MULTIPLE_SKINS : ARX_FTL_MULTIPLE_ROOTS;
  }
  return ARX_OK;
}

// Parent precedes children. Ordinal prefixes from arx-pistoris exports win when complete
// and valid; otherwise trusts merged unified order when valid, then BFS fallback.
// Disconnected components -> MULTIPLE_SKINS/ROOTS.
static ArxReturnCode orderJointsTopologically(const Json& gltf, const UnifiedJoints& unified,
                                              const std::vector<int32_t>& joint_parent, size_t n_skins,
                                              std::vector<int32_t>& topo, std::vector<int32_t>& unified_to_topo_out) {
  size_t nj_unified = joint_parent.size();
  topo.clear();
  topo.reserve(nj_unified);
  unified_to_topo_out.assign(nj_unified, -1);

  std::vector<int32_t> ordinal_topo(nj_unified, -1);
  std::vector<int32_t> ordinal_owner(nj_unified, -1);
  std::vector<int32_t> unified_ordinal(nj_unified, -1);
  bool saw_ordinal      = false;
  bool missing_ordinal  = false;
  bool duplicate        = false;
  bool out_of_range     = false;
  bool ordinal_topology = true;
  for (size_t u = 0; u < nj_unified; ++u) {
    size_t node_idx             = unified.joint_nodes[u];
    std::string name            = gltf["nodes"][node_idx].value("name", std::string{});
    BoneOrdinalName parsed_name = parseBoneOrdinalName(name);
    if (!parsed_name.has_ordinal) {
      missing_ordinal = true;
      continue;
    }

    saw_ordinal = true;
    if (parsed_name.ordinal >= nj_unified) {
      out_of_range = true;
      continue;
    }

    if (ordinal_owner[parsed_name.ordinal] >= 0) {
      duplicate = true;
      continue;
    }

    ordinal_owner[parsed_name.ordinal] = static_cast<int32_t>(u);
    unified_ordinal[u]                 = static_cast<int32_t>(parsed_name.ordinal);
  }

  if (saw_ordinal && !missing_ordinal && !duplicate && !out_of_range) {
    for (size_t ordinal = 0; ordinal < nj_unified; ++ordinal) {
      if (ordinal_owner[ordinal] < 0) {
        missing_ordinal = true;
        break;
      }
      ordinal_topo[ordinal] = ordinal_owner[ordinal];
    }

    if (!missing_ordinal) {
      for (size_t ordinal = 0; ordinal < nj_unified; ++ordinal) {
        int32_t u  = ordinal_topo[ordinal];
        int32_t pu = joint_parent[u];
        if (pu >= 0 && unified_ordinal[static_cast<size_t>(pu)] >= static_cast<int32_t>(ordinal)) {
          ordinal_topology = false;
          break;
        }
      }
    }

    if (!missing_ordinal && ordinal_topology) {
      topo = std::move(ordinal_topo);
      for (size_t t = 0; t < topo.size(); ++t) unified_to_topo_out[topo[t]] = static_cast<int32_t>(t);
      log(ARX_LOG_INFO, std::format("GLB import: using ordinal bone name prefixes to restore {} bone(s) to FTL order",
                                    nj_unified));
      return ARX_OK;
    }
  }

  if (saw_ordinal) {
    if (missing_ordinal)
      log(ARX_LOG_WARN,
          "GLB import: ignoring ordinal bone name prefixes because some bones miss ordinals or the ordinal set has gaps");
    if (duplicate)
      log(ARX_LOG_WARN, "GLB import: ignoring ordinal bone name prefixes because duplicate ordinals were found");
    if (out_of_range)
      log(ARX_LOG_WARN,
          std::format("GLB import: ignoring ordinal bone name prefixes because at least one ordinal exceeds bone count {}",
                      nj_unified));
    if (!ordinal_topology)
      log(ARX_LOG_WARN,
          "GLB import: ignoring ordinal bone name prefixes because they put a child before its parent");
  }

  bool linear_valid = true;
  for (size_t u = 0; u < nj_unified; ++u) {
    int32_t pu = joint_parent[u];
    if (pu >= 0 && static_cast<size_t>(pu) >= u) {
      linear_valid = false;
      break;
    }
  }

  if (linear_valid) {
    for (size_t u = 0; u < nj_unified; ++u) {
      topo.push_back(static_cast<int32_t>(u));
      unified_to_topo_out[u] = static_cast<int32_t>(u);
    }
  } else {
    log(ARX_LOG_WARN,
        "GLB import: unified joint order is not topological (parent appears after child); "
        "falling back to BFS (bone order may differ from glTF)");
    for (size_t u = 0; u < nj_unified; ++u) {
      if (joint_parent[u] < 0) {
        topo.push_back(static_cast<int32_t>(u));
        unified_to_topo_out[u] = 0;
        break;
      }
    }
    for (size_t qhead = 0; qhead < topo.size(); ++qhead) {
      int32_t parent_u = topo[qhead];
      for (size_t u = 0; u < nj_unified; ++u) {
        if (joint_parent[u] == parent_u) {
          unified_to_topo_out[u] = static_cast<int32_t>(topo.size());
          topo.push_back(static_cast<int32_t>(u));
        }
      }
    }
  }

  if (topo.size() != nj_unified) {
    log(ARX_LOG_ERROR, "GLB import: skeleton has unreachable joints (disconnected component)");
    bool from_multiple_skins = (n_skins > 1);
    return from_multiple_skins ? ARX_GLB_MULTIPLE_SKINS : ARX_FTL_MULTIPLE_ROOTS;
  }
  return ARX_OK;
}

ArxReturnCode buildSkeleton(const Json& gltf, std::span<const uint8_t> bin, const NodeWorld& nw,
                            std::vector<BoneInfo>& bones_out, std::vector<int32_t>& unified_to_topo_out,
                            std::vector<std::vector<int32_t>>& skin_to_unified_out) {
  bones_out.clear();
  unified_to_topo_out.clear();
  skin_to_unified_out.clear();
  size_t n_skins = gltf.contains("skins") ? gltf["skins"].size() : 0;
  if (n_skins == 0) return ARX_OK;

  UnifiedJoints unified;
  ARX_RETURN_IF_ERR(mergeSkinJoints(gltf, bin, nw, unified, skin_to_unified_out));

  size_t nj_unified = unified.joint_nodes.size();
  if (nj_unified == 0) return ARX_OK;

  std::vector<ArxVector3> bind_world;
  std::vector<Mat4> bind_world_matrix;
  ARX_RETURN_IF_ERR(computeBindWorlds(unified, nw, bind_world, bind_world_matrix));

  std::vector<int32_t> joint_parent;
  ARX_RETURN_IF_ERR(computeJointParents(unified, nw, n_skins, joint_parent));

  std::vector<int32_t> topo;
  ARX_RETURN_IF_ERR(orderJointsTopologically(gltf, unified, joint_parent, n_skins, topo, unified_to_topo_out));

  bones_out.resize(nj_unified);
  for (size_t t = 0; t < nj_unified; ++t) {
    int32_t u            = topo[t];
    BoneInfo& bi         = bones_out[t];
    bi.joint_node        = unified.joint_nodes[u];
    bi.parent_topo       = (joint_parent[u] >= 0) ? unified_to_topo_out[joint_parent[u]] : -1;
    bi.world_bind_pos    = bind_world[u];
    bi.bind_world_matrix = bind_world_matrix[u];
    std::string nm       = gltf["nodes"][unified.joint_nodes[u]].value("name", std::string{});
    auto parsed_name     = parseBoneOrdinalName(nm);
    if (parsed_name.has_ordinal) nm = std::string(parsed_name.stripped_name);
    if (nm.empty()) nm = std::format("bone_{}", t);
    for (auto& ch : nm) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    bi.name = std::move(nm);
  }
  return ARX_OK;
}

// --- Synthetic vertices and groups ---

void emitGroups(const Json& gltf, const std::vector<BoneInfo>& bones, const std::vector<int32_t>& joint_to_topo,
                const std::vector<VertJointInfo>& vj, ftl::Data& out) {
  size_t nb = bones.size();
  if (nb == 0) return;

  std::vector<std::vector<int32_t>> children(nb);
  for (int32_t t = 0; t < static_cast<int32_t>(nb); ++t)
    if (bones[t].parent_topo >= 0) children[bones[t].parent_topo].push_back(t);

  // one synthetic pivot vertex per bone
  size_t pivot_base = out.vertices.size();
  for (size_t t = 0; t < nb; ++t) {
    ftl::Vertex pv{};
    pv.position = bones[t].world_bind_pos;
    pv.normal   = {0.0f, 1.0f, 0.0f};
    out.vertices.push_back(pv);
  }

  std::vector<std::vector<int32_t>> assigned(nb);
  for (size_t v = 0; v < vj.size(); ++v) {
    int32_t j = vj[v].dominant_joint;
    if (j < 0 || static_cast<size_t>(j) >= joint_to_topo.size()) continue;
    int32_t topo = joint_to_topo[j];
    if (topo < 0) continue;
    assigned[topo].push_back(static_cast<int32_t>(v));
  }

  std::vector<std::vector<int32_t>> descendants(nb);
  for (int32_t t = static_cast<int32_t>(nb) - 1; t >= 0; --t) {
    for (int32_t c : children[t]) {
      descendants[t].push_back(c);
      descendants[t].insert(descendants[t].end(), descendants[c].begin(), descendants[c].end());
    }
  }

  out.groups.reserve(nb);
  for (size_t t = 0; t < nb; ++t) {
    ftl::Group g{};
    size_t copy_n = std::min(bones[t].name.size(), sizeof(g.name) - 1);
    std::memcpy(g.name, bones[t].name.data(), copy_n);
    g.origin           = static_cast<uint32_t>(pivot_base + t);
    g.blob_shadow_size = 0.0f;
    const Json& jnode  = gltf["nodes"][bones[t].joint_node];
    if (jnode.contains("extras") && jnode["extras"].contains("arx_blob_shadow_size")) {
      g.blob_shadow_size = jnode["extras"]["arx_blob_shadow_size"].get<float>();
    }
    g.indices = assigned[t];
    g.indices.push_back(static_cast<int32_t>(pivot_base + t));
    for (int32_t d : descendants[t]) g.indices.push_back(static_cast<int32_t>(pivot_base + d));
    out.groups.push_back(std::move(g));
  }
}

// --- Action point import ---

// "arx_action__<name>" empties. Position derived at bind:
//   action_world = parent_joint.bind_world_matrix * chain_TRS * origin
// Action vertex goes into parent group's indices so validateFtl maps it back on roundtrip
ArxReturnCode importActionPoints(const Json& gltf, const NodeWorld& nw, const std::vector<BoneInfo>& bones,
                                 ftl::Data& out) {
  if (!gltf.contains("nodes")) return ARX_OK;

  std::vector<bool> is_joint(nw.world.size(), false);
  if (gltf.contains("skins")) {
    for (const auto& skin : gltf["skins"]) {
      if (!skin.contains("joints")) continue;
      for (const auto& jv : skin["joints"]) {
        size_t n = jv.get<size_t>();
        if (n < is_joint.size()) is_joint[n] = true;
      }
    }
  }

  std::unordered_map<size_t, int32_t> joint_node_to_topo;
  for (size_t t = 0; t < bones.size(); ++t) joint_node_to_topo[bones[t].joint_node] = static_cast<int32_t>(t);

  constexpr std::string_view kActionPrefix = "arx_action__";

  for (size_t n = 0; n < nw.world.size(); ++n) {
    if (!nw.reached[n] || is_joint[n]) continue;
    const Json& node = gltf["nodes"][n];
    if (node.contains("mesh") || node.contains("skin")) continue;

    std::string name = node.value("name", std::string{});
    if (!name.starts_with(kActionPrefix)) {
      if (!name.empty())
        log(ARX_LOG_DEBUG, std::format("GLB import: empty node '{}' lacks arx_action__ prefix, discarding", name));
      continue;
    }
    std::string action_name = name.substr(kActionPrefix.size());
    if (action_name.empty()) {
      log(ARX_LOG_WARN, "GLB import: empty action name after stripping arx_action__ prefix");
      continue;
    }

    // accumulate local TRS up to the nearest joint; BFS-world fallback if no joint is found
    Mat4 chain          = nodeLocalMat4(node);
    int32_t parent_topo = -1;
    int32_t cur_parent  = nw.parent[n];
    while (cur_parent >= 0) {
      auto it = joint_node_to_topo.find(static_cast<size_t>(cur_parent));
      if (it != joint_node_to_topo.end()) {
        parent_topo = it->second;
        break;
      }
      chain      = nodeLocalMat4(gltf["nodes"][cur_parent]) * chain;
      cur_parent = nw.parent[cur_parent];
    }

    if (out.vertices.size() >= kFtlMaxVertices) {
      log(ARX_LOG_ERROR, "GLB import: vertex count exceeds 16-bit ceiling (action points)");
      return ARX_GLB_TOO_MANY_VERTICES;
    }
    if (out.actions.size() >= kFtlMaxActions) {
      log(ARX_LOG_WARN, "GLB import: action count limit reached, dropping remaining actions");
      return ARX_OK;
    }

    ArxVector3 wpos;
    if (parent_topo >= 0) {
      const Mat4& bind = bones[parent_topo].bind_world_matrix;
      wpos             = math::xformPoint(bind, math::translation(chain));
    } else {
      wpos = math::translation(nw.world[n]);
    }

    ftl::Vertex av{};
    av.position        = wpos;
    av.normal          = {0.0f, 1.0f, 0.0f};
    int32_t vertex_idx = static_cast<int32_t>(out.vertices.size());
    out.vertices.push_back(av);

    if (parent_topo >= 0 && static_cast<size_t>(parent_topo) < out.groups.size())
      out.groups[parent_topo].indices.push_back(vertex_idx);

    ftl::Action act{};
    size_t copy_n = std::min(action_name.size(), sizeof(act.name) - 1);
    std::memcpy(act.name, action_name.data(), copy_n);
    act.vertex_idx = vertex_idx;
    out.actions.push_back(act);
  }
  return ARX_OK;
}

void warnUnselectedSyntheticVertices(const ftl::Data& ftl) {
  std::set<int32_t> synthetic;
  if (ftl.header.origin < ftl.vertices.size()) synthetic.insert(static_cast<int32_t>(ftl.header.origin));
  for (const auto& group : ftl.groups)
    if (group.origin < ftl.vertices.size()) synthetic.insert(static_cast<int32_t>(group.origin));
  for (const auto& action : ftl.actions)
    if (action.vertex_idx >= 0 && static_cast<size_t>(action.vertex_idx) < ftl.vertices.size())
      synthetic.insert(action.vertex_idx);

  if (synthetic.empty()) return;

  std::set<int32_t> selected;
  for (const auto& selection : ftl.selections)
    for (int32_t idx : selection.selected) selected.insert(idx);

  size_t missing = 0;
  for (int32_t idx : synthetic)
    if (!selected.contains(idx)) ++missing;

  if (missing == 0) return;

  log(ARX_LOG_WARN,
      std::format("GLB import: {} synthetic origin/action vertices are not in any selection.",
                  missing));
}

std::set<int32_t> collectSyntheticVertices(const ftl::Data& ftl) {
  std::set<int32_t> synthetic;
  if (ftl.header.origin < ftl.vertices.size()) synthetic.insert(static_cast<int32_t>(ftl.header.origin));
  for (const auto& group : ftl.groups)
    if (group.origin < ftl.vertices.size()) synthetic.insert(static_cast<int32_t>(group.origin));
  for (const auto& action : ftl.actions)
    if (action.vertex_idx >= 0 && static_cast<size_t>(action.vertex_idx) < ftl.vertices.size())
      synthetic.insert(action.vertex_idx);
  return synthetic;
}

bool selectionContains(const ftl::Selection& selection, int32_t idx) {
  return std::find(selection.selected.begin(), selection.selected.end(), idx) != selection.selected.end();
}

void addSelectionVertex(ftl::Selection& selection, int32_t idx) {
  if (!selectionContains(selection, idx)) selection.selected.push_back(idx);
}

std::string lowerAscii(std::string_view value) {
  std::string out(value);
  for (char& ch : out) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return out;
}

int32_t owningGroupForVertex(const ftl::Data& ftl, int32_t vertex_idx) {
  int32_t owner = -1;
  for (size_t gi = 0; gi < ftl.groups.size(); ++gi) {
    const auto& indices = ftl.groups[gi].indices;
    if (std::find(indices.begin(), indices.end(), vertex_idx) != indices.end()) owner = static_cast<int32_t>(gi);
  }
  return owner;
}

void guessSyntheticSelectionAffiliations(ftl::Data& ftl) {
  if (ftl.selections.empty()) return;

  const std::set<int32_t> synthetic = collectSyntheticVertices(ftl);
  std::vector<std::vector<size_t>> group_origin_selection_slots(ftl.groups.size());

  for (size_t gi = 0; gi < ftl.groups.size(); ++gi) {
    const auto& group = ftl.groups[gi];
    if (group.origin >= ftl.vertices.size()) continue;
    std::vector<int32_t> mesh_vertices;
    for (int32_t idx : group.indices) {
      if (idx < 0 || static_cast<size_t>(idx) >= ftl.vertices.size()) continue;
      if (synthetic.contains(idx)) continue;
      mesh_vertices.push_back(idx);
    }
    if (mesh_vertices.empty()) continue;

    std::vector<std::string> weak;
    for (size_t si = 0; si < ftl.selections.size(); ++si) {
      size_t selected = 0;
      for (int32_t idx : mesh_vertices)
        if (selectionContains(ftl.selections[si], idx)) ++selected;

      float pct = 100.0f * static_cast<float>(selected) / static_cast<float>(mesh_vertices.size());
      if (pct <= 50.0f) continue;

      int32_t origin_idx = static_cast<int32_t>(group.origin);
      addSelectionVertex(ftl.selections[si], origin_idx);
      group_origin_selection_slots[gi].push_back(si);
      if (pct < 70.0f) weak.push_back(std::format("{} ({:.1f}%)", ftl.selections[si].name, pct));
    }

    if (!weak.empty()) {
      std::string joined = weak.front();
      for (size_t i = 1; i < weak.size(); ++i) {
        joined += ", ";
        joined += weak[i];
      }
      log(ARX_LOG_WARN, std::format("GLB import: group origin '{}' selection guesses were applied with low support: {}",
                                    group.name, joined));
    }
  }

  for (const auto& action : ftl.actions) {
    if (action.vertex_idx < 0 || static_cast<size_t>(action.vertex_idx) >= ftl.vertices.size()) continue;
    int32_t owner = owningGroupForVertex(ftl, action.vertex_idx);
    if (owner < 0 || static_cast<size_t>(owner) >= group_origin_selection_slots.size()) continue;
    for (size_t si : group_origin_selection_slots[owner]) addSelectionVertex(ftl.selections[si], action.vertex_idx);
  }

  if (ftl.header.origin < ftl.vertices.size()) {
    for (auto& selection : ftl.selections) {
      if (lowerAscii(selection.name) == "leggings") {
        addSelectionVertex(selection, static_cast<int32_t>(ftl.header.origin));
        break;
      }
    }
  }
}

// --- Animation import (GLB -> TEA, linear interpolation only) ---

struct RestTRS {
  ArxVector3 t;
  ArxQuat r;  // w,x,y,z
  ArxVector3 s;
};

// assumes rotation + uniform scale + translation. Rejects near-zero scale and reflections
bool decomposeRestTrs(const Mat4& m, std::string_view bone_name, RestTRS& out) {
  out.t = {m.m[12], m.m[13], m.m[14]};

  float c0[3] = {m.m[0], m.m[1], m.m[2]};
  float c1[3] = {m.m[4], m.m[5], m.m[6]};
  float c2[3] = {m.m[8], m.m[9], m.m[10]};
  float l0    = std::sqrt(c0[0] * c0[0] + c0[1] * c0[1] + c0[2] * c0[2]);
  float l1    = std::sqrt(c1[0] * c1[0] + c1[1] * c1[1] + c1[2] * c1[2]);
  float l2    = std::sqrt(c2[0] * c2[0] + c2[1] * c2[1] + c2[2] * c2[2]);
  float s     = (l0 + l1 + l2) / 3.0f;

  if (s < 1e-6f) {
    log(ARX_LOG_ERROR, std::format("GLB import: bone '{}' rest matrix has near-zero scale", bone_name));
    out.r = ArxQuat{};
    out.s = {1.0f, 1.0f, 1.0f};
    return false;
  }

  out.s = {s, s, s};

  float r00 = c0[0] / s, r10 = c0[1] / s, r20 = c0[2] / s;
  float r01 = c1[0] / s, r11 = c1[1] / s, r21 = c1[2] / s;
  float r02 = c2[0] / s, r12 = c2[1] / s, r22 = c2[2] / s;

  float det = r00 * (r11 * r22 - r12 * r21) - r01 * (r10 * r22 - r12 * r20) + r02 * (r10 * r21 - r11 * r20);
  if (det < 0.0f) {
    log(ARX_LOG_ERROR, std::format("GLB import: bone '{}' rest matrix has reflection (det<0)", bone_name));
    return false;
  }

  // Shoemake
  float trace = r00 + r11 + r22;
  ArxQuat q;
  if (trace > 0.0f) {
    float ss = std::sqrt(trace + 1.0f) * 2.0f;
    q.w      = 0.25f * ss;
    q.x      = (r21 - r12) / ss;
    q.y      = (r02 - r20) / ss;
    q.z      = (r10 - r01) / ss;
  } else if (r00 > r11 && r00 > r22) {
    float ss = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
    q.w      = (r21 - r12) / ss;
    q.x      = 0.25f * ss;
    q.y      = (r01 + r10) / ss;
    q.z      = (r02 + r20) / ss;
  } else if (r11 > r22) {
    float ss = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
    q.w      = (r02 - r20) / ss;
    q.x      = (r01 + r10) / ss;
    q.y      = 0.25f * ss;
    q.z      = (r12 + r21) / ss;
  } else {
    float ss = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
    q.w      = (r10 - r01) / ss;
    q.x      = (r02 + r20) / ss;
    q.y      = (r12 + r21) / ss;
    q.z      = 0.25f * ss;
  }
  out.r = q;
  return true;
}

// the unique non-joint parent of root joint(s); -1 if none or ambiguous
int32_t identifyWrapperNode(const NodeWorld& nw, const std::vector<BoneInfo>& bones) {
  if (bones.empty()) return -1;
  std::set<int32_t> candidates;
  for (const auto& bi : bones) {
    if (bi.parent_topo >= 0) continue;
    int32_t p = nw.parent[bi.joint_node];
    if (p >= 0) candidates.insert(p);
  }
  if (candidates.size() == 1) return *candidates.begin();
  if (candidates.size() > 1) {
    log(ARX_LOG_WARN, std::format("GLB import: {} distinct wrapper nodes found for root joints; root entity "
                                  "translation/rotation channels disabled",
                                  candidates.size()));
  }
  return -1;
}

struct Sampler {
  std::vector<float> in_times;
  AccessorView output;
  bool step = false;  // false = LINEAR
};

ArxReturnCode resolveSampler(const Json& gltf, std::span<const uint8_t> bin, const Json& sampler_json, Sampler& out) {
  if (!sampler_json.contains("input") || !sampler_json.contains("output")) return ARX_GLB_BAD_FORMAT;

  std::string interp = sampler_json.value("interpolation", std::string{"LINEAR"});
  if (interp == "CUBICSPLINE") {
    log(ARX_LOG_ERROR, "GLB import: CUBICSPLINE interpolation not supported");
    return ARX_GLB_UNSUPPORTED_FEATURE;
  }
  out.step = (interp == "STEP");

  AccessorView in_av;
  ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, sampler_json.at("input").get<size_t>(), in_av));
  if (in_av.type != "SCALAR" || in_av.component_type != kCompFloat) {
    log(ARX_LOG_ERROR, "GLB import: animation input must be float SCALAR");
    return ARX_GLB_BAD_FORMAT;
  }
  out.in_times.resize(in_av.count);
  for (size_t i = 0; i < in_av.count; ++i) decodeFloats(in_av, i, &out.in_times[i]);

  ARX_RETURN_IF_ERR(resolveAccessor(gltf, bin, sampler_json.at("output").get<size_t>(), out.output));
  if (out.output.component_type != kCompFloat) {
    log(ARX_LOG_ERROR, "GLB import: animation output must be float-typed");
    return ARX_GLB_BAD_FORMAT;
  }
  if (out.output.count != out.in_times.size()) {
    log(ARX_LOG_ERROR,
        std::format("GLB import: sampler input count {} != output count {}", out.in_times.size(), out.output.count));
    return ARX_GLB_BAD_FORMAT;
  }
  return ARX_OK;
}

void normalizeQuat4(float* q) {
  float l = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (l > 1e-8f) {
    q[0] /= l;
    q[1] /= l;
    q[2] /= l;
    q[3] /= l;
  } else {
    q[0] = 0.0f;
    q[1] = 0.0f;
    q[2] = 0.0f;
    q[3] = 1.0f;
  }
}

ArxQuat quatConj(ArxQuat q) { return {q.w, -q.x, -q.y, -q.z}; }

ArxQuat quatMul(ArxQuat a, ArxQuat b) {
  return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

// v' = q * (0,v) * conj(q), imaginary part
ArxVector3 quatRotate(ArxQuat q, ArxVector3 v) {
  ArxQuat vq{0.0f, v.x, v.y, v.z};
  ArxQuat r = quatMul(quatMul(q, vq), quatConj(q));
  return {r.x, r.y, r.z};
}

// clamps outside the range; is_quat renormalizes post-lerp per glTF spec
void sampleAt(const Sampler& sm, float t, int components, bool is_quat, float* out) {
  size_t n = sm.in_times.size();
  if (n == 0) {
    for (int c = 0; c < components; ++c) out[c] = 0.0f;
    return;
  }
  if (t <= sm.in_times.front()) {
    decodeFloats(sm.output, 0, out);
    if (is_quat) normalizeQuat4(out);
    return;
  }
  if (t >= sm.in_times.back()) {
    decodeFloats(sm.output, n - 1, out);
    if (is_quat) normalizeQuat4(out);
    return;
  }
  size_t i = 0;
  while (i + 1 < n && sm.in_times[i + 1] < t) ++i;
  if (sm.step) {
    decodeFloats(sm.output, i, out);
    return;
  }
  float dt    = sm.in_times[i + 1] - sm.in_times[i];
  float alpha = dt > 0.0f ? (t - sm.in_times[i]) / dt : 0.0f;
  float a[4]{}, b[4]{};
  decodeFloats(sm.output, i, a);
  decodeFloats(sm.output, i + 1, b);
  for (int c = 0; c < components; ++c) out[c] = (1.0f - alpha) * a[c] + alpha * b[c];
  if (is_quat) normalizeQuat4(out);
}

struct AnimChannelMaps {
  std::vector<Sampler> samplers;
  std::vector<bool> resolved;
  std::unordered_map<size_t, int32_t> node_to_tsi, node_to_rsi, node_to_ssi;
  int32_t root_t_si = -1;
  int32_t root_r_si = -1;
};

// node-keyed (not joint-keyed) because the chain walk samples any channel-bearing ancestor
static ArxReturnCode collectAnimChannels(const Json& gltf, std::span<const uint8_t> bin, const Json& anim,
                                         int32_t wrapper_node, AnimChannelMaps& maps) {
  if (!anim.contains("samplers") || !anim.contains("channels")) return ARX_GLB_BAD_FORMAT;
  const Json& samplers_json = anim["samplers"];
  const Json& channels_json = anim["channels"];

  maps.samplers.assign(samplers_json.size(), {});
  maps.resolved.assign(samplers_json.size(), false);
  auto ensure = [&](int32_t si) -> ArxReturnCode {
    if (si < 0 || static_cast<size_t>(si) >= maps.samplers.size()) return ARX_GLB_BAD_FORMAT;
    if (maps.resolved[si]) return ARX_OK;
    ARX_RETURN_IF_ERR(resolveSampler(gltf, bin, samplers_json[si], maps.samplers[si]));
    maps.resolved[si] = true;
    return ARX_OK;
  };

  for (const auto& ch : channels_json) {
    if (!ch.contains("sampler") || !ch.contains("target")) return ARX_GLB_BAD_FORMAT;
    const auto& tgt = ch["target"];
    if (!tgt.contains("path")) return ARX_GLB_BAD_FORMAT;

    std::string path = tgt.at("path").get<std::string>();
    if (path == "weights") {
      log(ARX_LOG_ERROR, "GLB import: morph target weights animation not supported");
      return ARX_GLB_UNSUPPORTED_FEATURE;
    }
    if (path != "translation" && path != "rotation" && path != "scale") {
      log(ARX_LOG_DEBUG, std::format("GLB import: ignoring channel path '{}'", path));
      continue;
    }
    if (!tgt.contains("node")) {
      log(ARX_LOG_DEBUG, "GLB import: channel missing target node, skipping");
      continue;
    }
    int32_t target_node = tgt.at("node").get<int32_t>();
    int32_t si          = ch.at("sampler").get<int32_t>();

    ARX_RETURN_IF_ERR(ensure(si));
    const auto& sm = maps.samplers[si];
    bool want_vec3 = (path == "translation" || path == "scale");
    if (want_vec3 && sm.output.type != "VEC3") {
      log(ARX_LOG_ERROR, std::format("GLB import: {} channel must have VEC3 output", path));
      return ARX_GLB_BAD_FORMAT;
    }
    if (!want_vec3 && sm.output.type != "VEC4") {
      log(ARX_LOG_ERROR, "GLB import: rotation channel must have VEC4 output");
      return ARX_GLB_BAD_FORMAT;
    }

    auto& map = (path == "translation") ? maps.node_to_tsi : (path == "rotation") ? maps.node_to_rsi : maps.node_to_ssi;
    auto [it, inserted] = map.insert({static_cast<size_t>(target_node), si});
    if (!inserted) {
      log(ARX_LOG_ERROR, std::format("GLB import: duplicate channel for node {} path '{}'", target_node, path));
      return ARX_GLB_BAD_FORMAT;
    }
  }

  // root entity channels come from the wrapper; it must NOT feed chain-walk sampling
  // or its animation doubles up (Arx applies the entity transform separately)
  if (wrapper_node >= 0) {
    auto it = maps.node_to_tsi.find(static_cast<size_t>(wrapper_node));
    if (it != maps.node_to_tsi.end()) maps.root_t_si = it->second;
    auto it2 = maps.node_to_rsi.find(static_cast<size_t>(wrapper_node));
    if (it2 != maps.node_to_rsi.end()) maps.root_r_si = it2->second;
  }
  return ARX_OK;
}

// shifts sampler input times forward if any sample is negative so t=0 sits at or before the grid
static ArxReturnCode buildTimeGrid(AnimChannelMaps& maps, size_t anim_idx, std::vector<float>& grid_kept,
                                   std::vector<int32_t>& frames) {
  frames.clear();
  grid_kept.clear();

  std::vector<float> grid;
  for (size_t si = 0; si < maps.samplers.size(); ++si)
    if (maps.resolved[si])
      for (float v : maps.samplers[si].in_times) grid.push_back(v);

  if (grid.empty()) {
    log(ARX_LOG_WARN, std::format("GLB import: animation {} has no usable channels, skipping", anim_idx));
    return ARX_OK;
  }
  std::sort(grid.begin(), grid.end());
  grid.erase(std::unique(grid.begin(), grid.end()), grid.end());

  if (grid.front() < 0.0f) {
    float shift = -grid.front();
    log(ARX_LOG_WARN,
        std::format("GLB import: animation {} starts at t={}, shifting by {}s", anim_idx, grid.front(), shift));
    for (auto& g : grid) g += shift;
    for (size_t si = 0; si < maps.samplers.size(); ++si)
      if (maps.resolved[si])
        for (auto& v : maps.samplers[si].in_times) v += shift;
  }

  frames.reserve(grid.size());
  grid_kept.reserve(grid.size());
  for (float t : grid) {
    int32_t fr = static_cast<int32_t>(std::lround(t * kTeaFps));
    if (fr < 0) continue;  // defensive; shift should have cleared this
    if (!frames.empty() && fr == frames.back()) {
      log(ARX_LOG_WARN,
          std::format("GLB import: animation {} has two samples collapsing to frame {}, dropping", anim_idx, fr));
      continue;
    }
    frames.push_back(fr);
    grid_kept.push_back(t);
  }

  if (frames.empty()) {
    log(ARX_LOG_WARN, std::format("GLB import: animation {} produced no positive-time frames, skipping", anim_idx));
  }
  return ARX_OK;
}

static ArxReturnCode derivePerBoneKeyframes(const NodeWorld& nw, const std::vector<BoneInfo>& bones,
                                            const AnimChannelMaps& maps, const std::vector<RestTRS>& node_bind_local,
                                            const std::vector<RestTRS>& bone_bind_world, int32_t wrapper_node, float t,
                                            std::vector<Mat4>& world_at_t, std::vector<bool>& world_t_done,
                                            tea::Keyframe& kf) {
  std::fill(world_t_done.begin(), world_t_done.end(), false);
  std::function<void(size_t)> compute_world_t = [&](size_t n) {
    if (world_t_done[n]) return;
    ArxVector3 lt   = node_bind_local[n].t;
    ArxQuat lr      = node_bind_local[n].r;
    ArxVector3 ls   = node_bind_local[n].s;
    bool is_wrapper = (static_cast<int32_t>(n) == wrapper_node);
    if (!is_wrapper) {
      auto tit = maps.node_to_tsi.find(n);
      if (tit != maps.node_to_tsi.end()) {
        float v[3];
        sampleAt(maps.samplers[tit->second], t, 3, false, v);
        lt = {v[0], v[1], v[2]};
      }
      auto rit = maps.node_to_rsi.find(n);
      if (rit != maps.node_to_rsi.end()) {
        float v[4];
        sampleAt(maps.samplers[rit->second], t, 4, true, v);
        lr = ArxQuat{v[3], v[0], v[1], v[2]};
      }
      auto sit = maps.node_to_ssi.find(n);
      if (sit != maps.node_to_ssi.end()) {
        float v[3];
        sampleAt(maps.samplers[sit->second], t, 3, false, v);
        ls = {v[0], v[1], v[2]};
      }
    }
    Mat4 local        = math::fromTrs(lt, lr, ls);
    Mat4 parent_world = kIdentityMat4;
    if (nw.parent[n] >= 0) {
      compute_world_t(static_cast<size_t>(nw.parent[n]));
      parent_world = world_at_t[static_cast<size_t>(nw.parent[n])];
    }
    world_at_t[n]   = parent_world * local;
    world_t_done[n] = true;
  };

  size_t ng = bones.size();
  for (size_t gi = 0; gi < ng; ++gi) compute_world_t(bones[gi].joint_node);

  // Arx recursion: anim.quat[i] = anim.quat[parent] * ga.quat[i].
  // solve for ga.quat/translate so anim.quat[i] = world_R[i](t) * inv(world_R[i](bind))
  std::vector<ArxQuat> bone_anim_r(ng, ArxQuat{1.0f, 0.0f, 0.0f, 0.0f});
  std::vector<ArxVector3> bone_anim_t(ng, ArxVector3{0.0f, 0.0f, 0.0f});

  for (size_t gi = 0; gi < ng; ++gi) {
    auto& ga     = kf.groups[gi];
    ga.key_group = 0;

    RestTRS bw_t;
    if (!decomposeRestTrs(world_at_t[bones[gi].joint_node], bones[gi].name, bw_t)) return ARX_GLB_BAD_FORMAT;

    ArxQuat target_r    = quatMul(bw_t.r, quatConj(bone_bind_world[gi].r));
    ArxVector3 target_t = bw_t.t;

    ArxQuat parent_anim_r    = ArxQuat{1.0f, 0.0f, 0.0f, 0.0f};
    ArxVector3 parent_anim_t = ArxVector3{0.0f, 0.0f, 0.0f};
    if (bones[gi].parent_topo >= 0) {
      parent_anim_r = bone_anim_r[bones[gi].parent_topo];
      parent_anim_t = bone_anim_t[bones[gi].parent_topo];
    }

    ga.quat = quatMul(quatConj(parent_anim_r), target_r);

    // solves anim.trans[i] = parent.anim.quat * (transinit_global + ga.translate) + parent.anim.trans
    // (parent.anim.scale assumed 1; scale-animated files would need a chained anim.scale tracker)
    ArxVector3 transinit_global;
    if (bones[gi].parent_topo >= 0) {
      const auto& pp = bone_bind_world[bones[gi].parent_topo].t;
      transinit_global =
          ArxVector3{bone_bind_world[gi].t.x - pp.x, bone_bind_world[gi].t.y - pp.y, bone_bind_world[gi].t.z - pp.z};
    } else {
      transinit_global = bone_bind_world[gi].t;
    }
    ArxVector3 displacement{target_t.x - parent_anim_t.x, target_t.y - parent_anim_t.y, target_t.z - parent_anim_t.z};
    ArxVector3 needed = quatRotate(quatConj(parent_anim_r), displacement);
    ga.translate =
        ArxVector3{needed.x - transinit_global.x, needed.y - transinit_global.y, needed.z - transinit_global.z};

    bone_anim_r[gi] = target_r;
    bone_anim_t[gi] = target_t;

    ArxVector3 local_s = node_bind_local[bones[gi].joint_node].s;
    auto sit           = maps.node_to_ssi.find(bones[gi].joint_node);
    if (sit != maps.node_to_ssi.end()) {
      float v[3];
      sampleAt(maps.samplers[sit->second], t, 3, false, v);
      local_s = {v[0], v[1], v[2]};
    }
    ga.zoom = ArxVector3{local_s.x - 1.0f, local_s.y - 1.0f, local_s.z - 1.0f};
  }
  return ARX_OK;
}

// Root translation is sampled onto every TEA keyframe when the wrapper has movement.
// Root rotation stays sparse and is restored only on exact authored keys.
static void extractRootEntityChannels(const AnimChannelMaps& maps, float t, tea::Keyframe& kf) {
  auto translation_moves = [&](int32_t si) -> bool {
    if (si < 0) return false;
    const auto& sm = maps.samplers[si];
    for (size_t i = 0; i < sm.in_times.size(); ++i) {
      float v[3];
      decodeFloats(sm.output, i, v);
      if (std::abs(v[0]) > 1e-6f || std::abs(v[1]) > 1e-6f || std::abs(v[2]) > 1e-6f) return true;
    }
    return false;
  };

  auto exact_index = [&](int32_t si) -> int32_t {
    if (si < 0) return -1;
    const auto& sm = maps.samplers[si];
    for (size_t i = 0; i < sm.in_times.size(); ++i)
      if (std::abs(sm.in_times[i] - t) < 1e-5f) return static_cast<int32_t>(i);
    return -1;
  };

  if (translation_moves(maps.root_t_si)) {
    float v[3];
    sampleAt(maps.samplers[maps.root_t_si], t, 3, false, v);
    kf.translate = ArxVector3{v[0], v[1], v[2]};
  }
  int32_t rri = exact_index(maps.root_r_si);
  if (rri >= 0) {
    float v[4];
    decodeFloats(maps.samplers[maps.root_r_si].output, rri, v);
    normalizeQuat4(v);
    kf.quat = ArxQuat{v[3], v[0], v[1], v[2]};
  }
}

static void decodeAnimationExtras(const Json& anim, tea::Data& out) {
  if (!anim.contains("extras")) return;
  const auto& ex = anim["extras"];
  if (ex.contains("arx_footstep_frames") && ex["arx_footstep_frames"].is_array()) {
    for (const auto& fv : ex["arx_footstep_frames"]) {
      int32_t fi = fv.get<int32_t>();
      if (fi >= 0 && static_cast<size_t>(fi) < out.keyframes.size())
        out.keyframes[fi].flag_frame = kTeaFlagFrameStep;
      else
        log(ARX_LOG_WARN, std::format("GLB import: footstep frame {} out of range [0,{})", fi, out.keyframes.size()));
    }
  }
  if (ex.contains("arx_audio_keyframes") && ex["arx_audio_keyframes"].is_object()) {
    for (auto it = ex["arx_audio_keyframes"].begin(); it != ex["arx_audio_keyframes"].end(); ++it) {
      int32_t fi = -1;
      try {
        fi = std::stoi(it.key());
      } catch (...) {
        log(ARX_LOG_WARN, std::format("GLB import: audio key '{}' is not an integer", it.key()));
        continue;
      }
      if (fi < 0 || static_cast<size_t>(fi) >= out.keyframes.size()) {
        log(ARX_LOG_WARN, std::format("GLB import: audio frame {} out of range", fi));
        continue;
      }
      tea::Sample s{};
      std::string nm = it.value().get<std::string>();
      size_t copy_n  = std::min(nm.size(), sizeof(s.name) - 1);
      std::memcpy(s.name, nm.data(), copy_n);
      out.keyframes[fi].sample = s;
    }
  }
}

// must run after decodeAnimationExtras; extras index the pre-pop keyframe count
static void resolveAnimNameAndHold(const Json& anim, tea::Data& out) {
  std::string anim_name              = anim.value("name", std::string{});
  constexpr std::string_view kHoldSx = "__h";
  bool had_hold = anim_name.size() >= kHoldSx.size() &&
                  anim_name.compare(anim_name.size() - kHoldSx.size(), kHoldSx.size(), kHoldSx) == 0;
  if (had_hold) anim_name.resize(anim_name.size() - kHoldSx.size());

  if (had_hold && out.keyframes.size() >= 2) {
    out.num_frames = out.keyframes.back().num_frame;
    out.keyframes.pop_back();
  } else {
    if (had_hold) log(ARX_LOG_WARN, "GLB import: animation has '__h' suffix but <2 keyframes; ignoring suffix");
    out.num_frames = out.keyframes.back().num_frame;
  }

  size_t copy_n = std::min(anim_name.size(), sizeof(out.name) - 1);
  std::memcpy(out.name, anim_name.data(), copy_n);
  out.name[copy_n] = '\0';
}

ArxReturnCode importOneAnimation(const Json& gltf, std::span<const uint8_t> bin, const Json& anim,
                                 const std::vector<BoneInfo>& bones, const NodeWorld& nw, int32_t wrapper_node,
                                 size_t anim_idx, tea::Data& out) {
  AnimChannelMaps maps;
  ARX_RETURN_IF_ERR(collectAnimChannels(gltf, bin, anim, wrapper_node, maps));

  std::vector<float> grid_kept;
  std::vector<int32_t> frames;
  ARX_RETURN_IF_ERR(buildTimeGrid(maps, anim_idx, grid_kept, frames));
  if (frames.empty()) return ARX_OK;

  size_t ng      = bones.size();
  size_t n_nodes = nw.world.size();

  // node bind local TRS: chain-walk fallback; bone bind world TRS: inv(world_R(bind)) math
  std::vector<RestTRS> node_bind_local(n_nodes);
  for (size_t n = 0; n < n_nodes; ++n) {
    if (!nw.reached[n]) continue;
    Mat4 local = nodeLocalMat4(gltf["nodes"][n]);
    if (!decomposeRestTrs(local, "<node>", node_bind_local[n])) {
      node_bind_local[n] = RestTRS{ArxVector3{0, 0, 0}, ArxQuat{1, 0, 0, 0}, ArxVector3{1, 1, 1}};
    }
  }
  std::vector<RestTRS> bone_bind_world(ng);
  for (size_t gi = 0; gi < ng; ++gi) {
    if (!decomposeRestTrs(bones[gi].bind_world_matrix, bones[gi].name, bone_bind_world[gi])) return ARX_GLB_BAD_FORMAT;
  }

  out            = {};
  out.num_groups = static_cast<int32_t>(ng);
  out.keyframes.resize(frames.size());

  std::vector<bool> world_t_done(n_nodes);
  std::vector<Mat4> world_at_t(n_nodes);

  for (size_t k = 0; k < frames.size(); ++k) {
    auto& kf      = out.keyframes[k];
    kf.num_frame  = frames[k];
    kf.flag_frame = kTeaFlagFrameNone;
    kf.groups.resize(ng);
    ARX_RETURN_IF_ERR(derivePerBoneKeyframes(nw, bones, maps, node_bind_local, bone_bind_world, wrapper_node,
                                             grid_kept[k], world_at_t, world_t_done, kf));
    extractRootEntityChannels(maps, grid_kept[k], kf);
  }

  decodeAnimationExtras(anim, out);
  resolveAnimNameAndHold(anim, out);
  return ARX_OK;
}

ArxReturnCode importAnimations(const Json& gltf, std::span<const uint8_t> bin, const NodeWorld& nw,
                               const std::vector<BoneInfo>& bones, int32_t wrapper_node,
                               std::vector<tea::Data>& out_teas) {
  if (!gltf.contains("animations") || gltf["animations"].empty()) return ARX_OK;
  if (bones.empty()) {
    log(ARX_LOG_WARN, "GLB import: animations present but no skeleton; skipping all");
    return ARX_OK;
  }

  for (size_t ai = 0; ai < gltf["animations"].size(); ++ai) {
    tea::Data tea;
    ARX_RETURN_IF_ERR(importOneAnimation(gltf, bin, gltf["animations"][ai], bones, nw, wrapper_node, ai, tea));
    if (tea.keyframes.empty()) continue;
    ARX_RETURN_IF_ERR(validateTea(&tea));
    out_teas.push_back(std::move(tea));
  }
  return ARX_OK;
}

// --- header.name from filename ---

std::string_view basenameOf(std::string_view path) {
  size_t p = path.find_last_of("/\\");
  return (p == std::string_view::npos) ? path : path.substr(p + 1);
}

void setHeaderName(ftl::Data& d, std::string_view glb_filename) {
  if (glb_filename.empty()) {
    d.header.name[0] = '\0';
    return;
  }
  std::string h = "arx_pistoris\\";
  h += basenameOf(glb_filename);
  size_t copy_n = std::min(h.size(), sizeof(d.header.name) - 1);
  std::memcpy(d.header.name, h.data(), copy_n);
  d.header.name[copy_n] = '\0';
}

}  // namespace

// --- Entry point ---

static ArxReturnCode importGlbToFtlTeaImpl(std::span<const uint8_t> glb, std::string_view glb_filename,
                                           ftl::Data& out_ftl, std::vector<tea::Data>& out_teas,
                                           std::vector<std::pair<std::string, std::string>>* out_extras) {
  out_ftl = {};
  out_teas.clear();
  if (out_extras) out_extras->clear();

  std::span<const uint8_t> json_bytes, bin_bytes;
  ARX_RETURN_IF_ERR(parseGlbContainer(glb, json_bytes, bin_bytes));

  Json gltf = Json::parse(json_bytes.begin(), json_bytes.end(), nullptr, false);
  if (gltf.is_discarded()) {
    log(ARX_LOG_ERROR, "GLB import: JSON parse failed");
    return ARX_GLB_BAD_FORMAT;
  }

  NodeWorld nw;
  ARX_RETURN_IF_ERR(computeNodeWorlds(gltf, nw));

  std::vector<BoneInfo> bones;
  std::vector<int32_t> joint_to_topo;
  std::vector<std::vector<int32_t>> skin_to_unified;
  ARX_RETURN_IF_ERR(buildSkeleton(gltf, bin_bytes, nw, bones, joint_to_topo, skin_to_unified));

  // our exporter targets root T/R channels at the wrapper node
  int32_t wrapper_node = identifyWrapperNode(nw, bones);

  std::vector<MatFlags> mat_flags;
  std::vector<int16_t> material_tex_ids;
  buildMaterialContainers(gltf, out_ftl, mat_flags, material_tex_ids);

  // arx_selection_names is library-owned; other string extras surface to caller
  std::vector<std::string> extras_names;
  if (gltf.contains("meshes") && !gltf["meshes"].empty()) {
    const auto& mesh0 = gltf["meshes"][0];
    if (mesh0.contains("extras") && mesh0["extras"].is_object()) {
      const auto& extras = mesh0["extras"];
      for (auto it = extras.begin(); it != extras.end(); ++it) {
        const std::string& k = it.key();
        if (k == "arx_selection_names") {
          if (it.value().is_array()) {
            try {
              for (const auto& v : it.value())
                if (v.is_string()) extras_names.push_back(v.get<std::string>());
            } catch (const Json::exception&) {
              // SILENT: malformed optional selection names are ignored.
              extras_names.clear();
            }
          }
          continue;
        }
        if (!out_extras) continue;
        if (it.value().is_string()) {
          try {
            out_extras->emplace_back(k, it.value().get<std::string>());
          } catch (const Json::exception&) {
            // SILENT: bad value for one optional caller extra
            continue;
          }
        } else {
          log(ARX_LOG_INFO,
              std::format("GLB import: extras key '{}' has non-string type; skipped from extras vector", k));
        }
      }
    }
  }

  std::vector<VertJointInfo> vj;
  SelectionRegistry sel_registry;
  for (const auto& name : extras_names)
    if (!name.empty()) findOrAddSelection(sel_registry, name);
  ARX_RETURN_IF_ERR(
      ingestPrimitives(gltf, bin_bytes, nw, skin_to_unified, mat_flags, material_tex_ids, extras_names, out_ftl, vj,
                       sel_registry));

  if (out_ftl.vertices.size() + 1 + bones.size() > kFtlMaxVertices) {
    log(ARX_LOG_ERROR, "GLB import: vertex count with synthetic pivots exceeds 16-bit ceiling");
    return ARX_GLB_TOO_MANY_VERTICES;
  }
  out_ftl.header.origin = static_cast<uint32_t>(out_ftl.vertices.size());
  ftl::Vertex origin_v{};
  origin_v.position = {0.0f, 0.0f, 0.0f};
  origin_v.normal   = {0.0f, 1.0f, 0.0f};
  out_ftl.vertices.push_back(origin_v);

  emitGroups(gltf, bones, joint_to_topo, vj, out_ftl);

  ARX_RETURN_IF_ERR(importActionPoints(gltf, nw, bones, out_ftl));

  // Synthetic verts append after mesh verts; selection indices stay valid
  for (auto& sel : sel_registry) {
    if (sel.selected.empty()) continue;
    if (out_ftl.selections.size() >= kFtlMaxSelections) {
      log(ARX_LOG_WARN, "GLB import: selection count exceeds limit; dropping rest");
      break;
    }
    out_ftl.selections.push_back(std::move(sel));
  }
  guessSyntheticSelectionAffiliations(out_ftl);
  warnUnselectedSyntheticVertices(out_ftl);

  setHeaderName(out_ftl, glb_filename);

  ArxReturnCode rc = validateFtl(&out_ftl);
  if (rc != ARX_OK) return rc;

  ARX_RETURN_IF_ERR(importAnimations(gltf, bin_bytes, nw, bones, wrapper_node, out_teas));

  log(ARX_LOG_INFO,
      std::format("GLB import: {} vertices, {} faces, {} texture containers, {} bones, {} action points, "
                  "{} selection VEC4 attributes, {} animations",
                  out_ftl.vertices.size(), out_ftl.faces.size(), out_ftl.texture_containers.size(),
                  out_ftl.groups.size(), out_ftl.actions.size(), out_ftl.selections.size(), out_teas.size()));

  return ARX_OK;
}

ArxReturnCode importGlbToFtlTea(std::span<const uint8_t> glb, std::string_view glb_filename, ftl::Data& out_ftl,
                                std::vector<tea::Data>& out_teas,
                                std::vector<std::pair<std::string, std::string>>* out_extras) {
  try {
    return importGlbToFtlTeaImpl(glb, glb_filename, out_ftl, out_teas, out_extras);
  } catch (const Json::exception& e) {
    log(ARX_LOG_ERROR, std::format("GLB import: malformed GLTF JSON: {}", e.what()));
    out_ftl = {};
    out_teas.clear();
    if (out_extras) out_extras->clear();
    return ARX_GLB_BAD_FORMAT;
  }
}

}  // namespace pistoris
