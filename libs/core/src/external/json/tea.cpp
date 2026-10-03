// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/tea.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/log.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

using Json = json_detail::Json;

constexpr std::string_view kTeaSchema = "https://arx-tools.github.io/schemas/tea.schema.json";

std::string jsonPointer(const TeaLocation& location) {
  if (location.element == TeaElement::kHeader) {
    if (location.field == "name") return "/header/name";
    if (location.field == "num_frames") return "/header/totalNumberOfFrames";
    if (location.field == "keyframes") return "/keyframes";
    return "/header";
  }
  if (location.index == kNoElementIndex) return {};
  std::string result = json_detail::indexedPointer("/keyframes", location.index);
  if (location.element == TeaElement::kGroupTransform && location.subindex != kNoElementIndex) {
    result += "/groups/" + std::to_string(location.subindex);
    if (location.field == "quat")
      result += "/quaternion";
    else if (!location.field.empty())
      result += "/" + location.field;
    return result;
  }
  if (location.element == TeaElement::kSound) return result + "/sample/name";
  if (location.field == "num_frame") return result + "/frame";
  if (location.field == "flag_frame") return result + "/flags";
  if (location.field == "quat") return result + "/quaternion";
  if (!location.field.empty()) return result + "/" + location.field;
  return result;
}

bool isZero(const ArxVector3& value) { return value.x == 0.0f && value.y == 0.0f && value.z == 0.0f; }

bool isIdentity(const ArxQuat& value) {
  return value.w == 1.0f && value.x == 0.0f && value.y == 0.0f && value.z == 0.0f;
}

ArxReturnCode importTea(std::string_view text, NativeTextMode text_mode, tea::Data& out,
                        std::optional<JsonLocation>& failure_location) {
  Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root, failure_location));
  if (!root.is_object()) return json_detail::schemaFailure(failure_location, {});
  if (!json_detail::validSchema(root, kTeaSchema)) return json_detail::schemaFailure(failure_location, "/$schema");

  const Json* header = json_detail::member(root, "header");
  if (!header) return json_detail::schemaFailure(failure_location, "/header");

  const Json* name = json_detail::member(*header, "name");
  const Json* total_frames = json_detail::member(*header, "totalNumberOfFrames");
  std::string text_value;
  if (!name || !total_frames || !json_detail::getString(*name, text_value) ||
      !json_detail::getSigned(*total_frames, out.num_frames)) {
    return json_detail::schemaFailure(failure_location, "/header");
  }
  if (!json_detail::encodeTruncated(text_value, text_mode, out.name))
    return json_detail::schemaFailure(failure_location, "/header/name");

  const Json* keyframes = nullptr;
  ArxReturnCode code = json_detail::arrayMember(root, "keyframes", keyframes, kTeaMaxKeyframes);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/keyframes", code);
  out.keyframes.reserve(keyframes->size());

  std::size_t group_count = 0;
  if (!keyframes->empty()) {
    const Json* first_groups = json_detail::member((*keyframes)[0], "groups");
    if (!first_groups || !first_groups->is_array())
      return json_detail::schemaFailure(failure_location, "/keyframes/0/groups");
    group_count = first_groups->size();
  }
  if (group_count > kTeaMaxGroups)
    return json_detail::schemaFailure(failure_location, "/keyframes/0/groups", ARX_JSON_LIMIT_EXCEEDED);
  out.num_groups = static_cast<std::int32_t>(group_count);

  for (std::size_t keyframe_index = 0; keyframe_index < keyframes->size(); ++keyframe_index) {
    const Json& json = (*keyframes)[keyframe_index];
    const std::string pointer = json_detail::indexedPointer("/keyframes", keyframe_index);
    tea::Keyframe keyframe;
    const Json* frame = json_detail::member(json, "frame");
    if (!frame || !json_detail::getSigned(*frame, keyframe.num_frame))
      return json_detail::schemaFailure(failure_location, pointer + "/frame");

    const Json* flags = json_detail::member(json, "flags");
    if (flags && !json_detail::getSigned(*flags, keyframe.flag_frame))
      return json_detail::schemaFailure(failure_location, pointer + "/flags");

    bool ignored_bool = false;
    const Json* master = json_detail::member(json, "isMasterKeyFrame");
    if (master && !json_detail::getBool(*master, ignored_bool))
      return json_detail::schemaFailure(failure_location, pointer + "/isMasterKeyFrame");
    const Json* is_keyframe = json_detail::member(json, "isKeyFrame");
    if (is_keyframe && !json_detail::getBool(*is_keyframe, ignored_bool))
      return json_detail::schemaFailure(failure_location, pointer + "/isKeyFrame");
    std::int32_t ignored_int = 0;
    const Json* time_frame = json_detail::member(json, "timeFrame");
    if (time_frame && !json_detail::getSigned(*time_frame, ignored_int))
      return json_detail::schemaFailure(failure_location, pointer + "/timeFrame");

    const Json* translation = json_detail::member(json, "translate");
    if (translation) {
      ArxVector3 value{};
      if (!json_detail::getVector(*translation, value))
        return json_detail::schemaFailure(failure_location, pointer + "/translate");
      keyframe.translate = value;
    }
    const Json* quaternion = json_detail::member(json, "quaternion");
    if (quaternion) {
      ArxQuat value{};
      if (!json_detail::getQuaternion(*quaternion, value))
        return json_detail::schemaFailure(failure_location, pointer + "/quaternion");
      keyframe.quat = value;
    }

    const Json* groups = nullptr;
    code = json_detail::arrayMember(json, "groups", groups, kTeaMaxGroups);
    if (code != ARX_OK) return json_detail::schemaFailure(failure_location, pointer + "/groups", code);
    keyframe.groups.reserve(groups->size());
    for (std::size_t group_index = 0; group_index < groups->size(); ++group_index) {
      const Json& group_json = (*groups)[group_index];
      const std::string group_pointer = pointer + "/groups/" + std::to_string(group_index);
      tea::GroupAnim group;
      const Json* is_key = json_detail::member(group_json, "isKey");
      bool key_group = false;
      if (!is_key || !json_detail::getBool(*is_key, key_group))
        return json_detail::schemaFailure(failure_location, group_pointer + "/isKey");
      group.key_group = key_group ? 1 : 0;

      quaternion = json_detail::member(group_json, "quaternion");
      if (quaternion && !json_detail::getQuaternion(*quaternion, group.quat))
        return json_detail::schemaFailure(failure_location, group_pointer + "/quaternion");
      translation = json_detail::member(group_json, "translate");
      if (translation && !json_detail::getVector(*translation, group.translate))
        return json_detail::schemaFailure(failure_location, group_pointer + "/translate");
      const Json* zoom = json_detail::member(group_json, "zoom");
      if (zoom && !json_detail::getVector(*zoom, group.zoom))
        return json_detail::schemaFailure(failure_location, group_pointer + "/zoom");
      keyframe.groups.push_back(group);
    }

    const Json* sample_json = json_detail::member(json, "sample");
    if (sample_json) {
      if (!sample_json->is_object()) return json_detail::schemaFailure(failure_location, pointer + "/sample");
      const Json* sample_name = json_detail::member(*sample_json, "name");
      if (!sample_name || !json_detail::getString(*sample_name, text_value))
        return json_detail::schemaFailure(failure_location, pointer + "/sample/name");
      tea::Sample& sample = keyframe.sample.emplace();
      if (!json_detail::encodeTruncated(text_value, text_mode, sample.name))
        return json_detail::schemaFailure(failure_location, pointer + "/sample/name");
    }

    out.keyframes.push_back(std::move(keyframe));
  }

  TeaLocation native_location;
  if (const ArxReturnCode rc = canonicalizeTea(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  if (const ArxReturnCode rc = validateTea(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  log(ARX_LOG_INFO,
      "TEA JSON loaded: {} keyframes, {} groups, num_frames={}",
      out.keyframes.size(),
      out.num_groups,
      out.num_frames);
  return ARX_OK;
}

}  // namespace

TeaResult<std::string> exportTeaToJson(const tea::Data& data, bool pretty, NativeTextMode text_mode) {
  std::string output;
  std::optional<TeaLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    TeaLocation validation_location;
    if (const ArxReturnCode rc = validateTea(&data, &validation_location); rc != ARX_OK) {
      location = std::move(validation_location);
      return rc;
    }

    std::string decoded;
    if (!json_detail::decodeFixed(data.name, text_mode, decoded)) {
      location = TeaLocation{.element = TeaElement::kHeader, .field = "name"};
      return ARX_TEA_BAD_NAME;
    }
    Json root;
    root["$schema"] = kTeaSchema;
    root["header"] = Json::object();
    root["header"]["name"] = decoded;
    root["header"]["totalNumberOfFrames"] = data.num_frames;

    root["keyframes"] = Json::array();
    for (std::size_t keyframe_index = 0; keyframe_index < data.keyframes.size(); ++keyframe_index) {
      const tea::Keyframe& keyframe = data.keyframes[keyframe_index];
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
        if (!json_detail::decodeFixed(sample->name, text_mode, decoded)) {
          location = TeaLocation{.element = TeaElement::kSound,
                                 .index = keyframe_index,
                                 .subindex = kNoElementIndex,
                                 .field = "sample.name"};
          return ARX_TEA_BAD_SAMPLE_PATH;
        }
        json["sample"] = {{"name", decoded}, {"sizeInBytes", 0}};
      }

      root["keyframes"].push_back(std::move(json));
    }

    return json_detail::dump(root, pretty, output);
  });
  if (code != ARX_OK) return api_detail::teaFailure<std::string>(code, std::move(location));
  return TeaResult<std::string>::success(std::move(output));
}

JsonResult<tea::Data> importJsonToTea(std::string_view text, NativeTextMode text_mode) {
  tea::Data result;
  std::optional<JsonLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    return importTea(text, text_mode, result, location);
  });
  if (code != ARX_OK) return api_detail::jsonFailure<tea::Data>(code, std::move(location));
  return JsonResult<tea::Data>::success(std::move(result));
}

}  // namespace pistoris
