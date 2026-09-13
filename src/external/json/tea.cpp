// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/tea.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/log.h"
#include "utils/return_code.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

using Json = json_detail::Json;

constexpr std::string_view kTeaSchema = "https://arx-tools.github.io/schemas/tea.schema.json";

bool isZero(const ArxVector3& value) { return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f; }

bool isIdentity(const ArxQuat& value) {
  return value.w == 1.0f && value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}

ArxReturnCode importTea(std::string_view text, tea::Data& out) {
  Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root));
  if (!root.is_object() || !json_detail::validSchema(root, kTeaSchema)) return ARX_JSON_BAD_SCHEMA;

  const Json* header = json_detail::member(root, "header");
  if (!header) return ARX_JSON_BAD_SCHEMA;

  const Json* name = json_detail::member(*header, "name");
  const Json* total_frames = json_detail::member(*header, "totalNumberOfFrames");
  std::string text_value;
  if (!name || !total_frames || !json_detail::getString(*name, text_value) ||
      !json_detail::getSigned(*total_frames, out.num_frames)) {
    return ARX_JSON_BAD_SCHEMA;
  }
  json_detail::copyTruncated(text_value, out.name);

  const Json* keyframes = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "keyframes", keyframes, kTeaMaxKeyframes));
  out.keyframes.reserve(keyframes->size());

  std::size_t group_count = 0;
  if (!keyframes->empty()) {
    const Json* first_groups = json_detail::member((*keyframes)[0], "groups");
    if (!first_groups || !first_groups->is_array()) return ARX_JSON_BAD_SCHEMA;
    group_count = first_groups->size();
  }
  if (group_count > kTeaMaxGroups) return ARX_JSON_LIMIT_EXCEEDED;
  out.num_groups = static_cast<std::int32_t>(group_count);

  for (const Json& json : *keyframes) {
    tea::Keyframe keyframe;
    const Json* frame = json_detail::member(json, "frame");
    if (!frame || !json_detail::getSigned(*frame, keyframe.num_frame)) return ARX_JSON_BAD_SCHEMA;

    const Json* flags = json_detail::member(json, "flags");
    if (flags && !json_detail::getSigned(*flags, keyframe.flag_frame)) return ARX_JSON_BAD_SCHEMA;

    bool ignored_bool = false;
    const Json* master = json_detail::member(json, "isMasterKeyFrame");
    if (master && !json_detail::getBool(*master, ignored_bool)) return ARX_JSON_BAD_SCHEMA;
    const Json* is_keyframe = json_detail::member(json, "isKeyFrame");
    if (is_keyframe && !json_detail::getBool(*is_keyframe, ignored_bool)) return ARX_JSON_BAD_SCHEMA;
    std::int32_t ignored_int = 0;
    const Json* time_frame = json_detail::member(json, "timeFrame");
    if (time_frame && !json_detail::getSigned(*time_frame, ignored_int)) return ARX_JSON_BAD_SCHEMA;

    const Json* translation = json_detail::member(json, "translate");
    if (translation) {
      ArxVector3 value{};
      if (!json_detail::getVector(*translation, value)) return ARX_JSON_BAD_SCHEMA;
      keyframe.translate = value;
    }
    const Json* quaternion = json_detail::member(json, "quaternion");
    if (quaternion) {
      ArxQuat value{};
      if (!json_detail::getQuaternion(*quaternion, value)) return ARX_JSON_BAD_SCHEMA;
      keyframe.quat = value;
    }

    const Json* groups = nullptr;
    ARX_RETURN_IF_ERR(json_detail::arrayMember(json, "groups", groups, kTeaMaxGroups));
    keyframe.groups.reserve(groups->size());
    for (const Json& group_json : *groups) {
      tea::GroupAnim group;
      const Json* is_key = json_detail::member(group_json, "isKey");
      bool key_group = false;
      if (!is_key || !json_detail::getBool(*is_key, key_group)) return ARX_JSON_BAD_SCHEMA;
      group.key_group = key_group ? 1 : 0;

      quaternion = json_detail::member(group_json, "quaternion");
      if (quaternion && !json_detail::getQuaternion(*quaternion, group.quat)) return ARX_JSON_BAD_SCHEMA;
      translation = json_detail::member(group_json, "translate");
      if (translation && !json_detail::getVector(*translation, group.translate)) return ARX_JSON_BAD_SCHEMA;
      const Json* zoom = json_detail::member(group_json, "zoom");
      if (zoom && !json_detail::getVector(*zoom, group.zoom)) return ARX_JSON_BAD_SCHEMA;
      keyframe.groups.push_back(group);
    }

    const Json* sample_json = json_detail::member(json, "sample");
    if (sample_json) {
      if (!sample_json->is_object()) return ARX_JSON_BAD_SCHEMA;
      const Json* sample_name = json_detail::member(*sample_json, "name");
      if (!sample_name || !json_detail::getString(*sample_name, text_value)) return ARX_JSON_BAD_SCHEMA;
      tea::Sample& sample = keyframe.sample.emplace();
      json_detail::copyTruncated(text_value, sample.name);
    }

    out.keyframes.push_back(std::move(keyframe));
  }

  ARX_RETURN_IF_ERR(validateTea(&out));
  log(ARX_LOG_INFO,
      "TEA JSON loaded: {} keyframes, {} groups, num_frames={}",
      out.keyframes.size(),
      out.num_groups,
      out.num_frames);
  return ARX_OK;
}

}  // namespace

ArxReturnCode exportTeaToJson(const tea::Data& data, bool pretty, std::string& out) {
  return json_detail::guarded("TEA export", [&]() -> ArxReturnCode {
    ARX_RETURN_IF_ERR(validateTea(&data));

    Json root;
    root["$schema"] = kTeaSchema;
    root["header"] = Json::object();
    root["header"]["name"] = std::string(data.name);
    root["header"]["totalNumberOfFrames"] = data.num_frames;

    root["keyframes"] = Json::array();
    for (const tea::Keyframe& keyframe : data.keyframes) {
      Json json;
      json["frame"] = keyframe.num_frame;
      json["flags"] = keyframe.flag_frame;
      json["isMasterKeyFrame"] = false;
      json["isKeyFrame"] = false;
      json["timeFrame"] = 0;

      json["groups"] = Json::array();
      for (const tea::GroupAnim& group : keyframe.groups) {
        Json group_json;
        group_json["isKey"] = group.key_group != 0;
        if (!isIdentity(group.quat)) group_json["quaternion"] = json_detail::quaternion(group.quat);
        if (!isZero(group.translate)) group_json["translate"] = json_detail::vector(group.translate);
        if (!isZero(group.zoom)) group_json["zoom"] = json_detail::vector(group.zoom);
        json["groups"].push_back(std::move(group_json));
      }

      const auto& translation = keyframe.translate;
      if (translation && !isZero(*translation)) json["translate"] = json_detail::vector(*translation);
      const auto& quaternion = keyframe.quat;
      if (quaternion && !isIdentity(*quaternion)) json["quaternion"] = json_detail::quaternion(*quaternion);
      const auto& sample_value = keyframe.sample;
      if (const auto* sample = sample_value ? &*sample_value : nullptr) {
        json["sample"] = {{"name", std::string(sample->name)}, {"sizeInBytes", 0}};
      }

      root["keyframes"].push_back(std::move(json));
    }

    return json_detail::dump(root, pretty, out);
  });
}

ArxReturnCode importJsonToTea(std::string_view text, tea::Data* out) {
  if (!out) return ARX_INVALID_DATA_POINTER;
  return json_detail::guarded("TEA import", [&] {
    tea::Data temporary;
    const ArxReturnCode rc = importTea(text, temporary);
    if (rc == ARX_OK) *out = std::move(temporary);
    return rc;
  });
}

}  // namespace pistoris
