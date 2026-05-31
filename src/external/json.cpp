// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "external/json.h"

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "arx/ftl.h"
#include "arx/tea.h"
#include "nlohmann/json.hpp"
#include "utils/log.h"
#include "utils/parse_utils.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {

using Json = nlohmann::json;

static Json vec3Obj(const ArxVector3& v) { return {{"x", v.x}, {"y", v.y}, {"z", v.z}}; }
static Json vec3Arr(const ArxVector3& v) { return Json::array({v.x, v.y, v.z}); }
static Json quatObj(const ArxQuat& q) { return {{"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w}}; }

static bool isZeroVec3(const ArxVector3& v) { return v.x == 0.0f && v.y == 0.0f && v.z == 0.0f; }

static bool isIdentityQuat(const ArxQuat& q) { return q.w == 1.0f && q.x == 0.0f && q.y == 0.0f && q.z == 0.0f; }

ArxReturnCode exportFtlToJson(const ftl::Data& d, bool pretty, std::string& out) {
  ARX_RETURN_IF_ERR(validateFtl(&d));

  Json j;
  j["header"] = {{"origin", d.header.origin}, {"name", std::string(d.header.name)}};

  j["vertices"] = Json::array();
  for (const auto& v : d.vertices)
    j["vertices"].push_back({{"vector", vec3Obj(v.position)}, {"norm", vec3Obj(v.normal)}});

  j["faces"] = Json::array();
  for (const auto& f : d.faces)
    j["faces"].push_back({{"faceType", f.type},
                          {"vertexIdx", {f.vertex_idx.x, f.vertex_idx.y, f.vertex_idx.z}},
                          {"textureIdx", f.texture_id},
                          {"u", vec3Arr(f.u)},
                          {"v", vec3Arr(f.v)},
                          {"transval", f.transval},
                          {"norm", vec3Obj(f.norm)}});

  j["textureContainers"] = Json::array();
  for (const auto& tc : d.texture_containers)
    j["textureContainers"].push_back({{"filename", std::string(tc.filename)}});

  j["groups"] = Json::array();
  for (const auto& g : d.groups)
    j["groups"].push_back({{"name", std::string(g.name)},
                           {"origin", g.origin},
                           {"indices", g.indices},
                           {"blobShadowSize", g.blob_shadow_size}});

  j["actions"] = Json::array();
  for (const auto& a : d.actions)
    j["actions"].push_back(
        {{"name", std::string(a.name)}, {"vertexIdx", a.vertex_idx}, {"action", a.action}, {"sfx", a.sfx}});

  j["selections"] = Json::array();
  for (const auto& s : d.selections)
    j["selections"].push_back({{"name", std::string(s.name)}, {"selected", s.selected}});

  out = pretty ? j.dump(2) : j.dump();
  return ARX_OK;
}

static void copyStr(const std::string& src, char* dst, std::size_t n) {
  std::size_t len = std::min(src.size(), n - 1);
  // NOLINTNEXTLINE(bugprone-not-null-terminated-result)
  std::memcpy(dst, src.data(), len);
  std::memset(dst + len, 0, n - len);
}

static const Json* member(const Json& j, const char* key) {
  if (!j.is_object()) return nullptr;
  auto it = j.find(key);
  return it == j.end() ? nullptr : &*it;
}

static bool getFloat(const Json& j, float& out) {
  if (!j.is_number()) return false;
  out = j.get<float>();
  return true;
}

template <class Int>
static bool getSignedInt(const Json& j, Int& out) {
  static_assert(std::numeric_limits<Int>::is_signed);
  if (!j.is_number_integer()) return false;
  auto value = j.get<std::int64_t>();
  if (value < static_cast<std::int64_t>(std::numeric_limits<Int>::min()) ||
      value > static_cast<std::int64_t>(std::numeric_limits<Int>::max()))
    return false;
  out = static_cast<Int>(value);
  return true;
}

template <class Int>
static bool getUnsignedInt(const Json& j, Int& out) {
  static_assert(!std::numeric_limits<Int>::is_signed);
  if (!j.is_number_integer()) return false;
  std::uint64_t value = 0;
  if (j.is_number_unsigned()) {
    value = j.get<std::uint64_t>();
  } else {
    auto signed_value = j.get<std::int64_t>();
    if (signed_value < 0) return false;
    value = static_cast<std::uint64_t>(signed_value);
  }
  if (value > static_cast<std::uint64_t>(std::numeric_limits<Int>::max())) return false;
  out = static_cast<Int>(value);
  return true;
}

static bool getString(const Json& j, std::string& out) {
  if (!j.is_string()) return false;
  out = j.get<std::string>();
  return true;
}

static bool getBool(const Json& j, bool& out) {
  if (!j.is_boolean()) return false;
  out = j.get<bool>();
  return true;
}

static bool getVec3Object(const Json& j, ArxVector3& out) {
  const Json* x = member(j, "x");
  const Json* y = member(j, "y");
  const Json* z = member(j, "z");
  return x && y && z && getFloat(*x, out.x) && getFloat(*y, out.y) && getFloat(*z, out.z);
}

static bool getQuatObject(const Json& j, ArxQuat& out) {
  const Json* x = member(j, "x");
  const Json* y = member(j, "y");
  const Json* z = member(j, "z");
  const Json* w = member(j, "w");
  return x && y && z && w && getFloat(*x, out.x) && getFloat(*y, out.y) && getFloat(*z, out.z) && getFloat(*w, out.w);
}

static bool getVec3Array(const Json& j, ArxVector3& out) {
  return j.is_array() && j.size() == 3 && getFloat(j[0], out.x) && getFloat(j[1], out.y) && getFloat(j[2], out.z);
}

static ArxReturnCode getArrayMember(const Json& j, const char* key, const Json*& out, std::size_t max_size) {
  out = member(j, key);
  if (!out || !out->is_array()) return ARX_JSON_BAD_SCHEMA;
  if (out->size() > max_size) return ARX_JSON_LIMIT_EXCEEDED;
  return ARX_OK;
}

static ArxReturnCode getInt32Array(const Json& j, std::vector<int32_t>& out, std::size_t max_size) {
  if (!j.is_array()) return ARX_JSON_BAD_SCHEMA;
  if (j.size() > max_size) return ARX_JSON_LIMIT_EXCEEDED;
  out.clear();
  out.reserve(j.size());
  for (const auto& item : j) {
    int32_t value = 0;
    if (!getSignedInt(item, value)) return ARX_JSON_BAD_SCHEMA;
    out.push_back(value);
  }
  return ARX_OK;
}

static ArxReturnCode importJsonToFtlImpl(std::string_view text, ftl::Data& out) {
  try {
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded()) return ARX_JSON_BAD_FORMAT;

    const Json* jh = member(j, "header");
    if (!jh) return ARX_JSON_BAD_SCHEMA;
    const Json* origin = member(*jh, "origin");
    const Json* name   = member(*jh, "name");
    std::string text_value;
    if (!origin || !name || !getUnsignedInt(*origin, out.header.origin) || !getString(*name, text_value))
      return ARX_JSON_BAD_SCHEMA;
    copyStr(text_value, out.header.name, sizeof(out.header.name));

    const Json* vertices = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "vertices", vertices, kFtlMaxVertices));
    for (const auto& jv : *vertices) {
      ftl::Vertex v;
      const Json* vector = member(jv, "vector");
      const Json* norm   = member(jv, "norm");
      if (!vector || !norm || !getVec3Object(*vector, v.position) || !getVec3Object(*norm, v.normal))
        return ARX_JSON_BAD_SCHEMA;
      out.vertices.push_back(v);
    }

    const Json* faces = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "faces", faces, kFtlMaxFaces));
    for (const auto& jf : *faces) {
      ftl::Face f;
      const Json* face_type   = member(jf, "faceType");
      const Json* vertex_idx  = member(jf, "vertexIdx");
      const Json* texture_idx = member(jf, "textureIdx");
      const Json* u           = member(jf, "u");
      const Json* v           = member(jf, "v");
      const Json* norm        = member(jf, "norm");
      if (!face_type || !vertex_idx || !texture_idx || !u || !v || !norm || !getUnsignedInt(*face_type, f.type) ||
          !vertex_idx->is_array() || vertex_idx->size() != 3 || !getUnsignedInt((*vertex_idx)[0], f.vertex_idx.x) ||
          !getUnsignedInt((*vertex_idx)[1], f.vertex_idx.y) || !getUnsignedInt((*vertex_idx)[2], f.vertex_idx.z) ||
          !getSignedInt(*texture_idx, f.texture_id) || !getVec3Array(*u, f.u) || !getVec3Array(*v, f.v) ||
          !getVec3Object(*norm, f.norm))
        return ARX_JSON_BAD_SCHEMA;
      const Json* transval = member(jf, "transval");
      if (transval && !getFloat(*transval, f.transval)) return ARX_JSON_BAD_SCHEMA;
      out.faces.push_back(f);
    }

    const Json* texture_containers = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "textureContainers", texture_containers, kFtlMaxTextures));
    for (const auto& jt : *texture_containers) {
      ftl::TextureContainer tc{};
      const Json* filename = member(jt, "filename");
      if (!filename || !getString(*filename, text_value)) return ARX_JSON_BAD_SCHEMA;
      copyStr(text_value, tc.filename, sizeof(tc.filename));
      out.texture_containers.push_back(tc);
    }

    const Json* groups = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "groups", groups, kFtlMaxGroups));
    for (const auto& jg : *groups) {
      ftl::Group g;
      name                         = member(jg, "name");
      origin                       = member(jg, "origin");
      const Json* indices          = member(jg, "indices");
      const Json* blob_shadow_size = member(jg, "blobShadowSize");
      if (!name || !origin || !indices || !blob_shadow_size || !getString(*name, text_value) ||
          !getUnsignedInt(*origin, g.origin) || !getFloat(*blob_shadow_size, g.blob_shadow_size))
        return ARX_JSON_BAD_SCHEMA;
      ARX_RETURN_IF_ERR(getInt32Array(*indices, g.indices, out.vertices.size()));
      copyStr(text_value, g.name, sizeof(g.name));
      out.groups.push_back(std::move(g));
    }

    const Json* actions = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "actions", actions, kFtlMaxActions));
    for (const auto& ja : *actions) {
      ftl::Action a{};
      name                   = member(ja, "name");
      const Json* vertex_idx = member(ja, "vertexIdx");
      const Json* action     = member(ja, "action");
      const Json* sfx        = member(ja, "sfx");
      if (!name || !vertex_idx || !action || !sfx || !getString(*name, text_value) ||
          !getSignedInt(*vertex_idx, a.vertex_idx) || !getSignedInt(*action, a.action) || !getSignedInt(*sfx, a.sfx))
        return ARX_JSON_BAD_SCHEMA;
      copyStr(text_value, a.name, sizeof(a.name));
      out.actions.push_back(a);
    }

    const Json* selections = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "selections", selections, kFtlMaxSelections));
    for (const auto& js : *selections) {
      ftl::Selection s;
      name                 = member(js, "name");
      const Json* selected = member(js, "selected");
      if (!name || !selected || !getString(*name, text_value)) return ARX_JSON_BAD_SCHEMA;
      ARX_RETURN_IF_ERR(getInt32Array(*selected, s.selected, out.vertices.size()));
      copyStr(text_value, s.name, sizeof(s.name));
      out.selections.push_back(std::move(s));
    }
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (const nlohmann::json::exception& e) {
    log(ARX_LOG_WARN, std::format("FTL JSON import: {}", e.what()));
    return ARX_JSON_BAD_SCHEMA;
  } catch (const std::exception& e) {
    log(ARX_LOG_WARN, std::format("FTL JSON import: {}", e.what()));
    return ARX_JSON_BAD_SCHEMA;
  }

  auto rc = validateFtl(&out);
  if (rc != ARX_OK) return rc;

  log(ARX_LOG_INFO, std::format("FTL JSON loaded: {} vertices, {} faces, {} textures, {} groups, {} actions, {} "
                                "selections",
                                out.vertices.size(), out.faces.size(), out.texture_containers.size(), out.groups.size(),
                                out.actions.size(), out.selections.size()));

  return ARX_OK;
}

ArxReturnCode importJsonToFtl(std::string_view text, ftl::Data* out) {
  ftl::Data tmp;
  ArxReturnCode rc = importJsonToFtlImpl(text, tmp);
  if (rc == ARX_OK) *out = std::move(tmp);
  return rc;
}

ArxReturnCode exportTeaToJson(const tea::Data& d, bool pretty, std::string& out) {
  ARX_RETURN_IF_ERR(validateTea(&d));

  Json j;
  j["$schema"]                       = "https://arx-tools.github.io/schemas/tea.schema.json";
  j["header"]                        = Json::object();
  j["header"]["name"]                = std::string(d.name);
  j["header"]["totalNumberOfFrames"] = d.num_frames;

  j["keyframes"] = Json::array();
  for (const auto& kf : d.keyframes) {
    Json jkf;
    jkf["frame"]            = kf.num_frame;
    jkf["flags"]            = kf.flag_frame;
    jkf["isMasterKeyFrame"] = false;
    jkf["isKeyFrame"]       = false;
    jkf["timeFrame"]        = 0;

    jkf["groups"] = Json::array();
    for (const auto& group : kf.groups) {
      Json jg;
      jg["isKey"] = group.key_group != 0;
      if (!isIdentityQuat(group.quat)) jg["quaternion"] = quatObj(group.quat);
      if (!isZeroVec3(group.translate)) jg["translate"] = vec3Obj(group.translate);
      if (!isZeroVec3(group.zoom)) jg["zoom"] = vec3Obj(group.zoom);
      jkf["groups"].push_back(std::move(jg));
    }

    if (kf.translate && !isZeroVec3(*kf.translate)) jkf["translate"] = vec3Obj(*kf.translate);
    if (kf.quat && !isIdentityQuat(*kf.quat)) jkf["quaternion"] = quatObj(*kf.quat);
    if (const auto* sample = kf.sample ? &*kf.sample : nullptr)
      jkf["sample"] = {{"name", std::string(sample->name)}, {"sizeInBytes", 0}};

    j["keyframes"].push_back(std::move(jkf));
  }

  out = pretty ? j.dump(2) : j.dump();
  return ARX_OK;
}

static ArxReturnCode importJsonToTeaImpl(std::string_view text, tea::Data& out) {
  try {
    const Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded()) return ARX_JSON_BAD_FORMAT;

    const Json* jh = member(j, "header");
    if (!jh) return ARX_JSON_BAD_SCHEMA;

    const Json* name                   = member(*jh, "name");
    const Json* total_number_of_frames = member(*jh, "totalNumberOfFrames");
    std::string text_value;
    if (!name || !total_number_of_frames || !getString(*name, text_value) ||
        !getSignedInt(*total_number_of_frames, out.num_frames))
      return ARX_JSON_BAD_SCHEMA;
    copyStr(text_value, out.name, sizeof(out.name));

    const Json* keyframes = nullptr;
    ARX_RETURN_IF_ERR(getArrayMember(j, "keyframes", keyframes, kTeaMaxKeyframes));

    std::size_t num_groups = 0;
    if (!keyframes->empty()) {
      const Json* first_groups = member((*keyframes)[0], "groups");
      if (!first_groups || !first_groups->is_array()) return ARX_JSON_BAD_SCHEMA;
      num_groups = first_groups->size();
    }
    if (num_groups > kTeaMaxGroups) {
      return ARX_JSON_LIMIT_EXCEEDED;
    }
    out.num_groups = static_cast<int32_t>(num_groups);

    for (const auto& jkf : *keyframes) {
      tea::Keyframe kf;
      const Json* frame = member(jkf, "frame");
      if (!frame || !getSignedInt(*frame, kf.num_frame)) return ARX_JSON_BAD_SCHEMA;

      const Json* flags = member(jkf, "flags");
      if (flags && !getSignedInt(*flags, kf.flag_frame)) return ARX_JSON_BAD_SCHEMA;

      bool ignored_bool               = false;
      const Json* is_master_key_frame = member(jkf, "isMasterKeyFrame");
      if (is_master_key_frame && !getBool(*is_master_key_frame, ignored_bool)) return ARX_JSON_BAD_SCHEMA;
      const Json* is_key_frame = member(jkf, "isKeyFrame");
      if (is_key_frame && !getBool(*is_key_frame, ignored_bool)) return ARX_JSON_BAD_SCHEMA;
      int32_t ignored_int    = 0;
      const Json* time_frame = member(jkf, "timeFrame");
      if (time_frame && !getSignedInt(*time_frame, ignored_int)) return ARX_JSON_BAD_SCHEMA;

      const Json* translate = member(jkf, "translate");
      if (translate) {
        ArxVector3 value{};
        if (!getVec3Object(*translate, value)) return ARX_JSON_BAD_SCHEMA;
        kf.translate = value;
      }
      const Json* quaternion = member(jkf, "quaternion");
      if (quaternion) {
        ArxQuat value{};
        if (!getQuatObject(*quaternion, value)) return ARX_JSON_BAD_SCHEMA;
        kf.quat = value;
      }

      const Json* groups = nullptr;
      ARX_RETURN_IF_ERR(getArrayMember(jkf, "groups", groups, kTeaMaxGroups));
      for (const auto& jg : *groups) {
        tea::GroupAnim group;
        const Json* is_key = member(jg, "isKey");
        bool key_group     = false;
        if (!is_key || !getBool(*is_key, key_group)) return ARX_JSON_BAD_SCHEMA;
        group.key_group = key_group ? 1 : 0;

        quaternion = member(jg, "quaternion");
        if (quaternion && !getQuatObject(*quaternion, group.quat)) return ARX_JSON_BAD_SCHEMA;
        translate = member(jg, "translate");
        if (translate && !getVec3Object(*translate, group.translate)) return ARX_JSON_BAD_SCHEMA;
        const Json* zoom = member(jg, "zoom");
        if (zoom && !getVec3Object(*zoom, group.zoom)) return ARX_JSON_BAD_SCHEMA;
        kf.groups.push_back(group);
      }

      const Json* js = member(jkf, "sample");
      if (js) {
        if (!js->is_object()) return ARX_JSON_BAD_SCHEMA;
        const Json* sample_name = member(*js, "name");
        if (!sample_name || !getString(*sample_name, text_value)) return ARX_JSON_BAD_SCHEMA;
        auto& sample = kf.sample.emplace();
        copyStr(text_value, sample.name, sizeof(sample.name));
      }

      out.keyframes.push_back(std::move(kf));
    }
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (const nlohmann::json::exception& e) {
    log(ARX_LOG_WARN, std::format("TEA JSON import: {}", e.what()));
    return ARX_JSON_BAD_SCHEMA;
  } catch (const std::exception& e) {
    log(ARX_LOG_WARN, std::format("TEA JSON import: {}", e.what()));
    return ARX_JSON_BAD_SCHEMA;
  }

  auto rc = validateTea(&out);
  if (rc != ARX_OK) return rc;

  log(ARX_LOG_INFO, std::format("TEA JSON loaded: {} keyframes, {} groups, num_frames={}", out.keyframes.size(),
                                out.num_groups, out.num_frames));

  return ARX_OK;
}

ArxReturnCode importJsonToTea(std::string_view text, tea::Data* out) {
  tea::Data tmp;
  ArxReturnCode rc = importJsonToTeaImpl(text, tmp);
  if (rc == ARX_OK) *out = std::move(tmp);
  return rc;
}

}  // namespace pistoris
