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
#include "utils/math/quat.h"
#include "utils/math/vec3.h"
#include "utils/parse_utils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {

// Selection names are positional; preserve attribute insertion order
using Json = nlohmann::ordered_json;

static constexpr int kCompFloat  = 5126;
static constexpr int kCompUShort = 5123;

static constexpr int kTargetArray   = 34962;
static constexpr int kTargetElement = 34963;

static constexpr uint32_t kGlbMagic      = 0x46546C67u;  // 'glTF'
static constexpr uint32_t kGlbVersion    = 2u;
static constexpr uint32_t kChunkTypeJson = 0x4E4F534Au;  // 'JSON'
static constexpr uint32_t kChunkTypeBin  = 0x004E4942u;  // 'BIN\0'

static std::string boneOrdinalName(int32_t index, std::string_view name) {
  return std::format("{:03}__{}", index, name);
}

static void align4(WriteCursor& c) {
  size_t rem = c.size() & 3u;
  if (rem) c.pad(4u - rem);
}

static void appendVec3(std::vector<float>& buf, const ArxVector3& v) {
  buf.push_back(v.x);
  buf.push_back(v.y);
  buf.push_back(v.z);
}

static void appendQuatGltf(std::vector<float>& buf, const ArxQuat& q) {
  buf.push_back(q.x);
  buf.push_back(q.y);
  buf.push_back(q.z);
  buf.push_back(q.w);
}

static void repeatLastN(std::vector<float>& buf, size_t n) {
  size_t size = buf.size();
  for (size_t i = 0; i < n; ++i) buf.push_back(buf[size - n + i]);
}

// --- Accessor/bufferView tracker ---

struct GlbAcc {
  WriteCursor bin;
  Json bvs  = Json::array();
  Json accs = Json::array();

  size_t addBufferView(size_t offset, size_t len, int target = 0) {
    Json bv;
    bv["buffer"]     = 0;
    bv["byteOffset"] = offset;
    bv["byteLength"] = len;
    if (target != 0) bv["target"] = target;
    bvs.push_back(std::move(bv));
    return bvs.size() - 1;
  }

  size_t addAccessor(size_t bv_idx, size_t count, const char* type, int comp, size_t byte_off = 0) {
    Json acc;
    acc["bufferView"]    = bv_idx;
    acc["byteOffset"]    = byte_off;
    acc["componentType"] = comp;
    acc["count"]         = count;
    acc["type"]          = type;
    accs.push_back(std::move(acc));
    return accs.size() - 1;
  }

  template <typename T>
  size_t writeAccessor(const T* data, size_t count, int components, const char* type, int comp, int target = 0) {
    align4(bin);
    size_t off       = bin.size();
    size_t n_scalars = count * static_cast<size_t>(components);
    bin.writeN(data, n_scalars);
    size_t bv = addBufferView(off, sizeof(T) * n_scalars, target);
    return addAccessor(bv, count, type, comp);
  }

  size_t writeTimeAccessor(const std::vector<float>& times) {
    align4(bin);
    size_t off = bin.size();
    bin.writeN(times.data(), times.size());
    size_t bv = addBufferView(off, sizeof(float) * times.size());
    Json acc;
    acc["bufferView"]    = bv;
    acc["byteOffset"]    = 0;
    acc["componentType"] = kCompFloat;
    acc["count"]         = times.size();
    acc["type"]          = "SCALAR";
    acc["min"]           = Json::array({times.front()});
    acc["max"]           = Json::array({times.back()});
    accs.push_back(std::move(acc));
    return accs.size() - 1;
  }

  size_t writePositionAccessor(const std::vector<float>& xyz) {
    size_t count = xyz.size() / 3;
    align4(bin);
    size_t off = bin.size();
    bin.writeN(xyz.data(), xyz.size());
    size_t bv  = addBufferView(off, sizeof(float) * xyz.size(), kTargetArray);
    float fmax = std::numeric_limits<float>::max();
    float flow = std::numeric_limits<float>::lowest();
    ArxVector3 min_v{fmax, fmax, fmax};
    ArxVector3 max_v{flow, flow, flow};
    for (size_t i = 0; i < count; ++i) {
      ArxVector3 p{xyz[i * 3 + 0], xyz[i * 3 + 1], xyz[i * 3 + 2]};
      min_v = math::componentMin(min_v, p);
      max_v = math::componentMax(max_v, p);
    }
    Json acc;
    acc["bufferView"]    = bv;
    acc["byteOffset"]    = 0;
    acc["componentType"] = kCompFloat;
    acc["count"]         = count;
    acc["type"]          = "VEC3";
    acc["min"]           = Json::array({min_v.x, min_v.y, min_v.z});
    acc["max"]           = Json::array({max_v.x, max_v.y, max_v.z});
    accs.push_back(std::move(acc));
    return accs.size() - 1;
  }
};

// --- Texture URI ---

// Arx uses backslashes; glTF requires forward
static std::string texUri(std::string_view filename) {
  std::string s(filename);
  for (auto& c : s)
    if (c == '\\') c = '/';
  return s;
}

// --- Primitive building ---

struct PrimData {
  std::vector<float> positions;  // vec3 flat
  std::vector<float> normals;    // vec3 flat
  std::vector<float> texcoords;  // vec2 flat
  std::vector<uint16_t> joints;  // vec4 flat (only [0] used, rest 0)
  std::vector<float> weights;    // vec4 flat ([0]=1.0, rest 0.0)
  std::vector<uint16_t> indices;
  std::vector<uint16_t> source_vi;  // ftl vertex index per expanded vertex
  size_t mat_idx = 0;
};

// (ftl_vi, u_bits, v_bits)
using ExpandKey = std::tuple<uint16_t, uint32_t, uint32_t>;

static uint32_t floatBits(float f) {
  uint32_t b;
  std::memcpy(&b, &f, 4);
  return b;
}

static ArxReturnCode buildPrim(const ftl::Data& d, int16_t tex_id, FaceType type, size_t mat_idx,
                               const std::vector<int32_t>& vtb, ArxVector3 origin_shift, PrimData& out) {
  out.mat_idx = mat_idx;

  std::map<ExpandKey, uint16_t> seen;
  for (size_t fi = 0; fi < d.faces.size(); ++fi) {
    const auto& face = d.faces[fi];
    if (face.texture_id != tex_id || face.type != type) continue;

    const float us[3]    = {face.u.x, face.u.y, face.u.z};
    const float vs[3]    = {face.v.x, face.v.y, face.v.z};
    const uint16_t vi[3] = {face.vertex_idx.x, face.vertex_idx.y, face.vertex_idx.z};

    for (int c = 0; c < 3; ++c) {
      ExpandKey key{vi[c], floatBits(us[c]), floatBits(vs[c])};
      auto it = seen.find(key);
      if (it != seen.end()) {
        out.indices.push_back(it->second);
        continue;
      }

      if (out.positions.size() / 3 > std::numeric_limits<uint16_t>::max()) return ARX_GLB_TOO_MANY_VERTICES;

      auto new_idx = static_cast<uint16_t>(out.positions.size() / 3);
      seen[key]    = new_idx;
      out.indices.push_back(new_idx);

      const auto& v = d.vertices[vi[c]];
      appendVec3(out.positions, v.position - origin_shift);
      appendVec3(out.normals, v.normal);
      out.texcoords.push_back(us[c]);
      out.texcoords.push_back(vs[c]);
      out.source_vi.push_back(vi[c]);

      // SILENT: ungrouped vertex assigned to bone 0 as a fallback
      // vtb is sized to vertices.size() and all -1 when groups is empty
      int32_t bone = vtb[vi[c]];
      if (bone < 0) bone = 0;

      uint16_t j[4] = {static_cast<uint16_t>(bone), 0, 0, 0};
      out.joints.insert(out.joints.end(), j, j + 4);
      float w[4] = {1.0f, 0.0f, 0.0f, 0.0f};
      out.weights.insert(out.weights.end(), w, w + 4);
    }
  }
  return ARX_OK;
}

// --- Transval averaging ---

static float averageTransval(const ftl::Data& d, int16_t tex_id, FaceType type) {
  float sum = 0.0f;
  float mn  = std::numeric_limits<float>::max();
  float mx  = std::numeric_limits<float>::lowest();
  int cnt   = 0;
  for (const auto& face : d.faces) {
    if (face.texture_id != tex_id || face.type != type) continue;
    sum += face.transval;
    mn = std::min(mn, face.transval);
    mx = std::max(mx, face.transval);
    ++cnt;
  }
  if (cnt == 0) return 0.0f;
  if (mn != mx) {
    std::string mat = matName(tex_id != kFtlTextureNone ? pathStem(d.texture_containers[tex_id].filename) : "", type);
    log(ARX_LOG_WARN, std::format("GLB export: material '{}' has varying transval [{}, {}], averaging", mat, mn, mx));
  }
  return sum / cnt;
}

// --- MAT4 helpers (column-major) ---

static void writeInvTranslationMat4(WriteCursor& bin, float px, float py, float pz) {
  float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -px, -py, -pz, 1};
  bin.writeN(m, 16);
}

// nodes: 0 = mesh, 1 = skeleton wrapper, 2..2+ng-1 = bones, 2+ng.. = actions
struct ExportCtx {
  const ftl::Data& ftl;
  std::span<const tea::Data* const> teas;
  int32_t ng    = 0;
  int32_t na    = 0;
  bool has_skin = false;
  ArxVector3 origin_shift{0.0f, 0.0f, 0.0f};
  std::vector<ArxVector3> bone_world_pos{};
  std::vector<std::vector<int32_t>> bone_children{};
  std::vector<int32_t> action_bone{};
  std::vector<ArxVector3> bone_rest_local{};
  GlbAcc acc{};
};

static constexpr int32_t kSkelWrapperNode = 1;
static int32_t boneNode(int32_t gi) noexcept { return 2 + gi; }
static int32_t actionNode(int32_t ng, int32_t ai) noexcept { return 2 + ng + ai; }

static std::string selectionAttrName(std::string_view name) {
  std::string out = "_";
  for (unsigned char ch : name) {
    if (ch >= 'a' && ch <= 'z')
      out.push_back(static_cast<char>(ch - 'a' + 'A'));
    else if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_')
      out.push_back(static_cast<char>(ch));
    else
      out.push_back('_');
  }
  return out;
}

// Empty buckets still advance mat_idx so material indices stay aligned
static ArxReturnCode buildPrimitivesAndMaterials(ExportCtx& cx, Json& primitives, Json& materials, Json& textures,
                                                 Json& images, size_t& n_mat_keys,
                                                 std::vector<std::string>& sel_names_ordered) {
  primitives = Json::array();
  materials  = Json::array();
  textures   = Json::array();
  images     = Json::array();

  auto mat_keys = collectMaterials(cx.ftl);
  n_mat_keys    = mat_keys.size();
  std::unordered_map<std::string, int32_t> image_idx_map;
  size_t mat_idx  = 0;
  const auto& vtb = cx.ftl.extras.vertex_to_bone;

  const auto& sels = cx.ftl.selections;
  std::vector<std::vector<uint8_t>> sel_mask(sels.size());
  std::vector<std::string> sel_attr_name(sels.size());
  for (size_t si = 0; si < sels.size(); ++si) {
    sel_mask[si].assign(cx.ftl.vertices.size(), 0u);
    for (int32_t v : sels[si].selected)
      if (v >= 0 && static_cast<size_t>(v) < cx.ftl.vertices.size()) sel_mask[si][v] = 1u;
    sel_attr_name[si] = selectionAttrName(sels[si].name);
  }

  sel_names_ordered.clear();
  sel_names_ordered.reserve(sels.size());
  for (const auto& s : sels) sel_names_ordered.emplace_back(s.name);

  for (const auto& [tex_id, type] : mat_keys) {
    PrimData prim;
    ARX_RETURN_IF_ERR(buildPrim(cx.ftl, tex_id, type, mat_idx, vtb, cx.origin_shift, prim));

    if (prim.positions.empty()) {
      ++mat_idx;
      continue;
    }

    size_t pos_acc = cx.acc.writePositionAccessor(prim.positions);
    size_t nrm_acc =
        cx.acc.writeAccessor(prim.normals.data(), prim.normals.size() / 3, 3, "VEC3", kCompFloat, kTargetArray);
    size_t uv_acc =
        cx.acc.writeAccessor(prim.texcoords.data(), prim.texcoords.size() / 2, 2, "VEC2", kCompFloat, kTargetArray);
    size_t idx_acc =
        cx.acc.writeAccessor(prim.indices.data(), prim.indices.size(), 1, "SCALAR", kCompUShort, kTargetElement);

    Json prim_json;
    prim_json["attributes"]["POSITION"]   = pos_acc;
    prim_json["attributes"]["NORMAL"]     = nrm_acc;
    prim_json["attributes"]["TEXCOORD_0"] = uv_acc;

    if (cx.has_skin) {
      size_t jnt_acc =
          cx.acc.writeAccessor(prim.joints.data(), prim.joints.size() / 4, 4, "VEC4", kCompUShort, kTargetArray);
      size_t wgt_acc =
          cx.acc.writeAccessor(prim.weights.data(), prim.weights.size() / 4, 4, "VEC4", kCompFloat, kTargetArray);
      prim_json["attributes"]["JOINTS_0"]  = jnt_acc;
      prim_json["attributes"]["WEIGHTS_0"] = wgt_acc;
    }

    // VEC4 FLOAT imports as Blender Float Color; names are positional in mesh.extras
    for (size_t si = 0; si < sels.size(); ++si) {
      std::vector<float> mask(prim.source_vi.size() * 4);
      for (size_t i = 0; i < prim.source_vi.size(); ++i) {
        float v         = sel_mask[si][prim.source_vi[i]] ? 1.0f : 0.0f;
        mask[i * 4 + 0] = v;
        mask[i * 4 + 1] = v;
        mask[i * 4 + 2] = v;
        mask[i * 4 + 3] = 1.0f;
      }
      size_t sel_acc = cx.acc.writeAccessor(mask.data(), prim.source_vi.size(), 4, "VEC4", kCompFloat, kTargetArray);
      prim_json["attributes"][sel_attr_name[si]] = sel_acc;
    }

    prim_json["indices"]  = idx_acc;
    prim_json["material"] = mat_idx;
    prim_json["mode"]     = 4;  // TRIANGLES
    primitives.push_back(std::move(prim_json));

    std::string_view tex_stem;
    std::string tex_uri;
    if (tex_id != kFtlTextureNone) {
      const auto& filename = cx.ftl.texture_containers[tex_id].filename;
      tex_stem             = pathStem(filename);
      tex_uri              = texUri(filename);
    }

    float transval = (type & kFaceBitTrans) ? averageTransval(cx.ftl, tex_id, type) : 0.0f;

    Json mat;
    mat["name"] = matName(tex_stem, type);

    Json pbr;
    pbr["metallicFactor"]  = 0.0f;
    pbr["roughnessFactor"] = 1.0f;
    if (!tex_uri.empty()) {
      auto it = image_idx_map.find(tex_uri);
      int32_t img_idx;
      if (it != image_idx_map.end()) {
        img_idx = it->second;
      } else {
        img_idx                = static_cast<int32_t>(images.size());
        image_idx_map[tex_uri] = img_idx;
        Json img;
        img["uri"] = tex_uri;
        images.push_back(std::move(img));
        Json tex;
        tex["source"] = img_idx;
        textures.push_back(std::move(tex));
      }
      pbr["baseColorTexture"]["index"] = img_idx;  // textures[] == images[] 1:1
    }
    if (type & kFaceBitTrans) {
      pbr["baseColorFactor"] = Json::array({1.0f, 1.0f, 1.0f, 1.0f - transval});
    }
    mat["pbrMetallicRoughness"] = std::move(pbr);

    if (type & kFaceBitDoublesided) mat["doubleSided"] = true;
    if (type & kFaceBitTrans) mat["alphaMode"] = "BLEND";

    materials.push_back(std::move(mat));
    ++mat_idx;
  }
  return ARX_OK;
}

static void buildSkinAndNodes(ExportCtx& cx, Json& skin_json, Json& nodes) {
  skin_json = Json();
  nodes     = Json::array();

  const auto& parent = cx.ftl.extras.parent_bone;

  if (cx.has_skin) {
    // IBM = T(-world_pos); rotation is baked into the bone chain
    align4(cx.acc.bin);
    size_t ibm_off = cx.acc.bin.size();
    for (int32_t gi = 0; gi < cx.ng; ++gi) {
      const auto& p = cx.bone_world_pos[gi];
      writeInvTranslationMat4(cx.acc.bin, p.x, p.y, p.z);
    }
    size_t ibm_bv  = cx.acc.addBufferView(ibm_off, sizeof(float) * 16 * static_cast<size_t>(cx.ng));
    size_t ibm_acc = cx.acc.addAccessor(ibm_bv, cx.ng, "MAT4", kCompFloat);

    Json joints = Json::array();
    for (int32_t gi = 0; gi < cx.ng; ++gi) joints.push_back(boneNode(gi));
    // Single-root enforced by validateFtl (ARX_FTL_ORPHAN_BONE)
    skin_json["skeleton"]            = boneNode(0);
    skin_json["joints"]              = std::move(joints);
    skin_json["inverseBindMatrices"] = ibm_acc;
  }

  // owning bone = anchor vertex's group; -1 = unbound (child of wrapper)
  const auto& vtb = cx.ftl.extras.vertex_to_bone;
  cx.action_bone.assign(cx.na, -1);
  for (int32_t ai = 0; ai < cx.na; ++ai) cx.action_bone[ai] = vtb[cx.ftl.actions[ai].vertex_idx];

  // node 0: mesh
  {
    Json mesh_node;
    mesh_node["mesh"] = 0;
    if (cx.has_skin) mesh_node["skin"] = 0;
    nodes.push_back(std::move(mesh_node));
  }

  // node 1: wrapper; carries TEA root anim, holds root bones and unbound actions
  {
    Json skel     = Json::object();
    Json children = Json::array();
    for (int32_t gi = 0; gi < cx.ng; ++gi)
      if (parent[gi] < 0) children.push_back(boneNode(gi));
    for (int32_t ai = 0; ai < cx.na; ++ai)
      if (cx.action_bone[ai] < 0) children.push_back(actionNode(cx.ng, ai));
    if (!children.empty()) skel["children"] = std::move(children);
    nodes.push_back(std::move(skel));
  }

  cx.bone_rest_local.assign(cx.ng, {});
  for (int32_t gi = 0; gi < cx.ng; ++gi) {
    Json node;
    node["name"] = boneOrdinalName(gi, cx.ftl.groups[gi].name);
    Json ch      = Json::array();
    for (auto c : cx.bone_children[gi]) ch.push_back(boneNode(c));
    for (int32_t ai = 0; ai < cx.na; ++ai)
      if (cx.action_bone[ai] == gi) ch.push_back(actionNode(cx.ng, ai));
    if (!ch.empty()) node["children"] = std::move(ch);
    ArxVector3 local = cx.bone_world_pos[gi];
    if (parent[gi] >= 0) local = local - cx.bone_world_pos[parent[gi]];
    cx.bone_rest_local[gi] = local;
    node["translation"]    = Json::array({local.x, local.y, local.z});
    if (cx.ftl.groups[gi].blob_shadow_size != 0.0f)
      node["extras"]["arx_blob_shadow_size"] = cx.ftl.groups[gi].blob_shadow_size;
    nodes.push_back(std::move(node));
  }

  // anchored: subtract bone world; unbound: subtract origin_shift to match shifted mesh
  for (int32_t ai = 0; ai < cx.na; ++ai) {
    const auto& action = cx.ftl.actions[ai];
    Json node;
    node["name"]        = "arx_action__" + std::string(action.name);
    ArxVector3 anchor   = cx.action_bone[ai] >= 0 ? cx.bone_world_pos[cx.action_bone[ai]] : cx.origin_shift;
    ArxVector3 local    = cx.ftl.vertices[action.vertex_idx].position - anchor;
    node["translation"] = Json::array({local.x, local.y, local.z});
    nodes.push_back(std::move(node));
  }
}

// if last keyframe < num_frames, append hold + "__h" name suffix so importer recovers duration
static void buildAnimations(ExportCtx& cx, const tea::Data& tea, size_t ti, Json& anim) {
  int32_t nkf = static_cast<int32_t>(tea.keyframes.size());
  int32_t ng  = cx.ng;

  std::vector<float> all_times(nkf);
  for (int32_t i = 0; i < nkf; ++i) all_times[i] = tea.keyframes[i].num_frame / kTeaFps;
  float anim_end = tea.num_frames / kTeaFps;
  bool need_hold = all_times.back() < anim_end;
  if (need_hold) all_times.push_back(anim_end);

  int32_t nout = static_cast<int32_t>(all_times.size());
  std::vector<std::vector<float>> bone_t(ng), bone_r(ng), bone_s(ng);
  for (int32_t gi = 0; gi < ng; ++gi) {
    bone_t[gi].reserve(nout * 3);
    bone_r[gi].reserve(nout * 4);
    bone_s[gi].reserve(nout * 3);
    for (int32_t ki = 0; ki < nkf; ++ki) {
      const auto& ga = tea.keyframes[ki].groups[gi];
      // TEA translate is delta from rest local; glTF needs full local translation
      appendVec3(bone_t[gi], cx.bone_rest_local[gi] + ga.translate);
      appendQuatGltf(bone_r[gi], ga.quat);
      // TEA zoom is additive from 1.0; glTF scale is multiplicative
      appendVec3(bone_s[gi], ArxVector3{1.0f, 1.0f, 1.0f} + ga.zoom);
    }
    if (need_hold) {
      repeatLastN(bone_t[gi], 3);  // vec3 translation
      repeatLastN(bone_r[gi], 4);  // vec4 quat
      repeatLastN(bone_s[gi], 3);  // vec3 scale
    }
  }

  std::vector<float> root_t_times, root_r_times;
  std::vector<float> root_t_vals, root_r_vals;
  for (int32_t ki = 0; ki < nkf; ++ki) {
    const auto& kf = tea.keyframes[ki];
    if (kf.translate) {
      root_t_times.push_back(all_times[ki]);
      appendVec3(root_t_vals, *kf.translate);
    }
    if (kf.quat) {
      root_r_times.push_back(all_times[ki]);
      appendQuatGltf(root_r_vals, *kf.quat);
    }
  }

  Json anim_extras;
  {
    Json footsteps = Json::array();
    Json audio;
    for (int32_t ki = 0; ki < nkf; ++ki) {
      const auto& kf = tea.keyframes[ki];
      if (kf.flag_frame == kTeaFlagFrameStep) footsteps.push_back(ki);
      if (kf.sample) audio[std::to_string(ki)] = std::string(kf.sample->name);
    }
    if (!footsteps.empty()) anim_extras["arx_footstep_frames"] = std::move(footsteps);
    if (!audio.empty()) anim_extras["arx_audio_keyframes"] = std::move(audio);
  }

  Json samplers    = Json::array();
  Json channels    = Json::array();
  size_t all_t_acc = cx.acc.writeTimeAccessor(all_times);

  for (int32_t gi = 0; gi < ng; ++gi) {
    auto add_channel = [&](size_t out_acc, const char* path) {
      size_t si = samplers.size();
      Json samp;
      samp["input"]         = all_t_acc;
      samp["output"]        = out_acc;
      samp["interpolation"] = "LINEAR";
      samplers.push_back(std::move(samp));
      Json ch;
      ch["sampler"]        = si;
      ch["target"]["node"] = boneNode(gi);
      ch["target"]["path"] = path;
      channels.push_back(std::move(ch));
    };

    size_t t_acc = cx.acc.writeAccessor(bone_t[gi].data(), nout, 3, "VEC3", kCompFloat);
    size_t r_acc = cx.acc.writeAccessor(bone_r[gi].data(), nout, 4, "VEC4", kCompFloat);
    size_t s_acc = cx.acc.writeAccessor(bone_s[gi].data(), nout, 3, "VEC3", kCompFloat);
    add_channel(t_acc, "translation");
    add_channel(r_acc, "rotation");
    add_channel(s_acc, "scale");
  }

  auto add_root_channel = [&](std::vector<float>& times_v, std::vector<float>& vals, int components, const char* type,
                              const char* path) {
    if (times_v.empty()) return;
    size_t in_acc  = cx.acc.writeTimeAccessor(times_v);
    size_t out_acc = cx.acc.writeAccessor(vals.data(), times_v.size(), components, type, kCompFloat);
    size_t si      = samplers.size();
    Json samp;
    samp["input"]         = in_acc;
    samp["output"]        = out_acc;
    samp["interpolation"] = "LINEAR";
    samplers.push_back(std::move(samp));
    Json ch;
    ch["sampler"]        = si;
    ch["target"]["node"] = kSkelWrapperNode;
    ch["target"]["path"] = path;
    channels.push_back(std::move(ch));
  };

  add_root_channel(root_t_times, root_t_vals, 3, "VEC3", "translation");
  add_root_channel(root_r_times, root_r_vals, 4, "VEC4", "rotation");

  anim                  = Json();
  std::string anim_name = (tea.name[0] != '\0') ? std::string(tea.name) : std::format("animation_{}", ti);

  if (need_hold) anim_name += "__h";
  anim["name"]     = std::move(anim_name);
  anim["samplers"] = std::move(samplers);
  anim["channels"] = std::move(channels);
  if (!anim_extras.empty()) anim["extras"] = std::move(anim_extras);
}

static ArxReturnCode assembleGlbContainer(Json&& gltf, GlbAcc& acc, std::vector<uint8_t>& out) {
  std::string json_str = gltf.dump();
  size_t json_pad      = (4 - json_str.size() % 4) % 4;
  json_str.append(json_pad, ' ');

  align4(acc.bin);
  ARX_RETURN_IF_ERR(acc.bin);
  auto bin_data        = acc.bin.take();
  size_t bin_data_size = bin_data.size();

  if (json_str.size() > std::numeric_limits<uint32_t>::max() || bin_data_size > std::numeric_limits<uint32_t>::max()) {
    log(ARX_LOG_ERROR, "GLB export: JSON or BIN chunk exceeds uint32 length limit");
    return ARX_GLB_BAD_FORMAT;
  }

  size_t total_len_size = 12u + 8u + json_str.size() + (bin_data_size > 0 ? 8u + bin_data_size : 0u);
  if (total_len_size > std::numeric_limits<uint32_t>::max()) {
    log(ARX_LOG_ERROR, "GLB export: total length exceeds uint32 limit");
    return ARX_GLB_BAD_FORMAT;
  }

  uint32_t json_chunk_len = static_cast<uint32_t>(json_str.size());
  uint32_t total_len      = static_cast<uint32_t>(total_len_size);

  out.clear();
  out.reserve(total_len);

  auto write32 = [&](uint32_t v) {
    const auto* p = reinterpret_cast<const uint8_t*>(&v);
    out.insert(out.end(), p, p + 4);
  };
  write32(kGlbMagic);
  write32(kGlbVersion);
  write32(total_len);

  write32(json_chunk_len);
  write32(kChunkTypeJson);
  out.insert(out.end(), json_str.begin(), json_str.end());

  if (bin_data_size > 0) {
    write32(static_cast<uint32_t>(bin_data_size));
    write32(kChunkTypeBin);
    out.insert(out.end(), bin_data.begin(), bin_data.end());
  }
  return ARX_OK;
}

static bool isStandardPrimitiveAttribute(std::string_view name) {
  return name == "POSITION" || name == "NORMAL" || name == "TEXCOORD_0" || name == "JOINTS_0" || name == "WEIGHTS_0";
}

static size_t countEmittedSelectionAttributes(const Json& primitives,
                                              const std::vector<std::string>& sel_names_ordered) {
  std::set<std::string> expected;
  for (const auto& name : sel_names_ordered) expected.insert(selectionAttrName(name));

  std::set<std::string> emitted;
  for (const auto& prim : primitives) {
    const auto it = prim.find("attributes");
    if (it == prim.end() || !it->is_object()) continue;
    for (auto attr = it->begin(); attr != it->end(); ++attr) {
      std::string_view key = attr.key();
      if (!isStandardPrimitiveAttribute(key) && expected.contains(std::string(key))) emitted.insert(attr.key());
    }
  }
  return emitted.size();
}

static size_t countEmittedBones(const Json& skin_json) {
  const auto it = skin_json.find("joints");
  return (it != skin_json.end() && it->is_array()) ? it->size() : 0u;
}

static size_t countEmittedActionPoints(const Json& nodes) {
  size_t count = 0;
  for (const auto& node : nodes) {
    const auto it = node.find("name");
    if (it == node.end() || !it->is_string()) continue;
    if (it->get<std::string>().starts_with("arx_action__")) ++count;
  }
  return count;
}

// --- Entry point ---

ArxReturnCode exportFtlTeaToGlb(const ftl::Data& ftl, std::span<const tea::Data> teas, std::vector<uint8_t>& out,
                                std::span<const std::pair<std::string, std::string>> extras) {
  std::vector<const tea::Data*> tea_ptrs;
  tea_ptrs.reserve(teas.size());
  for (const tea::Data& tea : teas) tea_ptrs.push_back(&tea);
  return exportFtlTeaToGlb(ftl, tea_ptrs, out, extras);
}

ArxReturnCode exportFtlTeaToGlb(const ftl::Data& ftl, std::initializer_list<const tea::Data*> teas,
                                std::vector<uint8_t>& out,
                                std::span<const std::pair<std::string, std::string>> extras) {
  return exportFtlTeaToGlb(ftl, std::span<const tea::Data* const>(teas.begin(), teas.size()), out, extras);
}

ArxReturnCode exportFtlTeaToGlb(const ftl::Data& ftl, std::span<const tea::Data* const> teas, std::vector<uint8_t>& out,
                                std::span<const std::pair<std::string, std::string>> extras) {
  ARX_RETURN_IF_ERR(validateFtl(&ftl));

  bool any_tea = false;
  for (size_t ti = 0; ti < teas.size(); ++ti) {
    if (!teas[ti]) continue;
    any_tea = true;
    ARX_RETURN_IF_ERR(validateTea(teas[ti]));
    if (static_cast<size_t>(teas[ti]->num_groups) != ftl.groups.size()) {
      log(ARX_LOG_ERROR, std::format("GLB export: TEA[{}] num_groups={} != FTL group count={}", ti,
                                     teas[ti]->num_groups, ftl.groups.size()));
      return ARX_GLB_TEA_GROUP_MISMATCH;
    }
  }
  if (any_tea && ftl.groups.empty()) {
    log(ARX_LOG_ERROR, "GLB export: TEA animations supplied but FTL has no bone groups");
    return ARX_GLB_NO_GROUPS_FOR_TEA;
  }

  ExportCtx cx{.ftl = ftl, .teas = teas};
  cx.ng       = static_cast<int32_t>(ftl.groups.size());
  cx.na       = static_cast<int32_t>(ftl.actions.size());
  cx.has_skin = cx.ng > 0;

  if (ftl.header.origin < ftl.vertices.size()) cx.origin_shift = ftl.vertices[ftl.header.origin].position;

  cx.bone_world_pos.assign(ftl.extras.bone_world_pos.size(), {});
  for (size_t i = 0; i < cx.bone_world_pos.size(); ++i)
    cx.bone_world_pos[i] = ftl.extras.bone_world_pos[i] - cx.origin_shift;

  cx.bone_children.assign(cx.ng, {});
  const auto& parent = ftl.extras.parent_bone;
  for (int32_t gi = 1; gi < cx.ng; ++gi)
    if (parent[gi] >= 0) cx.bone_children[parent[gi]].push_back(gi);

  Json primitives, materials, textures, images, skin_json, nodes;
  size_t n_mat_keys = 0;
  std::vector<std::string> sel_names_ordered;
  ARX_RETURN_IF_ERR(
      buildPrimitivesAndMaterials(cx, primitives, materials, textures, images, n_mat_keys, sel_names_ordered));
  ARX_RETURN_IF_ERR(cx.acc.bin);
  buildSkinAndNodes(cx, skin_json, nodes);
  ARX_RETURN_IF_ERR(cx.acc.bin);
  const size_t n_primitives                = primitives.size();
  const size_t n_materials                 = materials.size();
  const size_t n_bones                     = countEmittedBones(skin_json);
  const size_t n_action_points             = countEmittedActionPoints(nodes);
  const size_t n_selection_vec4_attributes = countEmittedSelectionAttributes(primitives, sel_names_ordered);

  Json animations = Json::array();
  for (size_t ti = 0; ti < teas.size(); ++ti) {
    if (!teas[ti]) continue;  // SILENT: null slot = "no anim at this slot" per caller convention
    Json anim;
    buildAnimations(cx, *teas[ti], ti, anim);
    ARX_RETURN_IF_ERR(cx.acc.bin);
    animations.push_back(std::move(anim));
  }
  size_t n_animations = animations.size();

  Json gltf;
  gltf["asset"]["version"]   = "2.0";
  gltf["asset"]["generator"] = "arx-pistoris";
  gltf["scene"]              = 0;
  gltf["scenes"]             = Json::array({Json::object({{"nodes", Json::array({0, 1})}})});
  gltf["nodes"]              = std::move(nodes);

  Json mesh_json;
  mesh_json["primitives"] = std::move(primitives);
  // Caller extras follow library extras; key collisions are caller-owned
  if (!sel_names_ordered.empty()) {
    Json names_arr = Json::array();
    for (const auto& n : sel_names_ordered) names_arr.push_back(n);
    mesh_json["extras"]["arx_selection_names"] = std::move(names_arr);
  }
  for (const auto& [k, v] : extras) {
    if (k.empty()) continue;
    mesh_json["extras"][k] = v;
  }
  gltf["meshes"] = Json::array({std::move(mesh_json)});

  if (cx.has_skin) gltf["skins"] = Json::array({std::move(skin_json)});

  gltf["materials"] = std::move(materials);
  if (!textures.empty()) gltf["textures"] = std::move(textures);
  if (!images.empty()) gltf["images"] = std::move(images);
  if (!animations.empty()) gltf["animations"] = std::move(animations);

  gltf["accessors"]   = std::move(cx.acc.accs);
  gltf["bufferViews"] = std::move(cx.acc.bvs);

  // buffer 0 is the BIN chunk; no uri = embedded
  size_t bin_data_size = cx.acc.bin.size();
  gltf["buffers"]      = Json::array({Json::object({{"byteLength", bin_data_size}})});

  ARX_RETURN_IF_ERR(assembleGlbContainer(std::move(gltf), cx.acc, out));

  log(ARX_LOG_INFO, std::format("GLB export: {} vertices, {} faces, {} primitives, {} materials, {} bones, "
                                "{} action points, {} selection VEC4 attributes, {} animations, {} bytes",
                                ftl.vertices.size(), ftl.faces.size(), n_primitives, n_materials, n_bones,
                                n_action_points, n_selection_vec4_attributes, n_animations, out.size()));

  return ARX_OK;
}

}  // namespace pistoris
