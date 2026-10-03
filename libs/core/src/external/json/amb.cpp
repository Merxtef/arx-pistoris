// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/amb.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"

#include "api/result_failure.h"
#include "external/json.h"
#include "external/json/native_common.h"
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

constexpr std::string_view kAmbSchema = "https://arx-tools.github.io/schemas/amb.schema.json";
constexpr std::uint32_t kJsonTrackFlagMask = 0x3f;
constexpr std::uint64_t kMaxJsonLoop = static_cast<std::uint64_t>(UINT32_MAX) + 1;

std::string jsonPointer(const AmbLocation& location) {
  if (location.element == AmbElement::kHeader && location.field == "tracks") return "/tracks";
  if (location.element == AmbElement::kTrack && location.index != kNoElementIndex) {
    std::string result = json_detail::indexedPointer("/tracks", location.index);
    if (location.field == "sample_path") result += "/filename";
    if (location.field == "keys") result += "/keys";
    return result;
  }
  if (location.element == AmbElement::kKey && location.index != kNoElementIndex) {
    std::string result = json_detail::indexedPointer("/tracks", location.index) + "/keys";
    if (location.subindex != kNoElementIndex) result += "/" + std::to_string(location.subindex);
    if (location.field == "delay_min_ms")
      result += "/delayMin";
    else if (!location.field.empty())
      result += "/" + location.field;
    return result;
  }
  return {};
}

Json settingJson(const amb::Setting& setting) {
  return {
      {"min", setting.min},
      {"max", setting.max},
      {"interval", setting.interval_ms},
      {"flags", setting.flags & (amb::kSettingRandom | amb::kSettingInterpolate)},
  };
}

bool parseSetting(const Json& json, amb::Setting& out) {
  const Json* min = json_detail::member(json, "min");
  const Json* max = json_detail::member(json, "max");
  const Json* interval = json_detail::member(json, "interval");
  const Json* flags = json_detail::member(json, "flags");
  return min && max && interval && flags && json_detail::getFloat(*min, out.min) &&
         json_detail::getFloat(*max, out.max) && json_detail::getUnsigned(*interval, out.interval_ms) &&
         json_detail::getUnsigned(*flags, out.flags) &&
         (out.flags & ~(amb::kSettingRandom | amb::kSettingInterpolate)) == 0;
}

bool parseKey(const Json& json, amb::Key& out) {
  const Json* start = json_detail::member(json, "start");
  const Json* loop = json_detail::member(json, "loop");
  const Json* delay_min = json_detail::member(json, "delayMin");
  const Json* delay_max = json_detail::member(json, "delayMax");
  const Json* volume = json_detail::member(json, "volume");
  const Json* pitch = json_detail::member(json, "pitch");
  const Json* pan = json_detail::member(json, "pan");
  const Json* x = json_detail::member(json, "x");
  const Json* y = json_detail::member(json, "y");
  const Json* z = json_detail::member(json, "z");
  std::uint64_t play_count = 0;
  if (!start || !loop || !delay_min || !delay_max || !volume || !pitch || !pan || !x || !y || !z ||
      !json_detail::getUnsigned(*start, out.start_ms) || !json_detail::getUnsigned(*loop, play_count) ||
      play_count == 0 || play_count > kMaxJsonLoop || !json_detail::getUnsigned(*delay_min, out.delay_min_ms) ||
      !json_detail::getUnsigned(*delay_max, out.delay_max_ms) || !parseSetting(*volume, out.volume) ||
      !parseSetting(*pitch, out.pitch) || !parseSetting(*pan, out.pan) || !parseSetting(*x, out.x) ||
      !parseSetting(*y, out.y) || !parseSetting(*z, out.z)) {
    return false;
  }
  out.loop_minus_one = static_cast<std::uint32_t>(play_count - 1);
  return true;
}

ArxReturnCode importAmb(std::string_view text, NativeTextMode text_mode, amb::Data& out,
                        std::optional<JsonLocation>& failure_location) {
  Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root, failure_location));
  if (!root.is_object()) return json_detail::schemaFailure(failure_location, {});
  if (!json_detail::validSchema(root, kAmbSchema)) return json_detail::schemaFailure(failure_location, "/$schema");

  const Json* tracks = nullptr;
  ArxReturnCode code = json_detail::arrayMember(root, "tracks", tracks, UINT32_MAX);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/tracks", code);
  out.tracks.reserve(tracks->size());
  for (std::size_t track_index = 0; track_index < tracks->size(); ++track_index) {
    const Json& json = (*tracks)[track_index];
    const std::string track_pointer = json_detail::indexedPointer("/tracks", track_index);
    const Json* filename = json_detail::member(json, "filename");
    const Json* flags = json_detail::member(json, "flags");
    const Json* keys = nullptr;
    amb::Track track;
    std::string sample_path;
    if (!filename || !flags || !json_detail::getString(*filename, sample_path) ||
        !native_text::encode(sample_path, text_mode, track.sample_path) ||
        !json_detail::getUnsigned(*flags, track.flags) || (track.flags & ~kJsonTrackFlagMask) != 0) {
      return json_detail::schemaFailure(failure_location, track_pointer);
    }
    code = json_detail::arrayMember(json, "keys", keys, UINT32_MAX);
    if (code != ARX_OK) return json_detail::schemaFailure(failure_location, track_pointer + "/keys", code);
    track.keys.reserve(keys->size());
    for (std::size_t key_index = 0; key_index < keys->size(); ++key_index) {
      amb::Key key;
      if (!parseKey((*keys)[key_index], key))
        return json_detail::schemaFailure(failure_location, track_pointer + "/keys/" + std::to_string(key_index));
      track.keys.push_back(key);
    }
    out.tracks.push_back(std::move(track));
  }

  AmbLocation native_location;
  if (const ArxReturnCode rc = canonicalizeAmb(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  const ArxReturnCode rc = validateAmb(&out, &native_location);
  return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
}

}  // namespace

AmbResult<std::string> exportAmbToJson(const amb::Data& data, bool pretty, NativeTextMode text_mode) {
  std::string output;
  std::optional<AmbLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    AmbLocation validation_location;
    if (const ArxReturnCode rc = validateAmb(&data, &validation_location); rc != ARX_OK) {
      location = std::move(validation_location);
      return rc;
    }

    Json root;
    root["$schema"] = kAmbSchema;
    root["tracks"] = Json::array();
    for (std::size_t track_index = 0; track_index < data.tracks.size(); ++track_index) {
      const amb::Track& track = data.tracks[track_index];
      std::string sample_path;
      if (!native_text::decode(track.sample_path, text_mode, sample_path)) {
        location = AmbLocation{
            .element = AmbElement::kTrack, .index = track_index, .subindex = kNoElementIndex, .field = "sample_path"};
        return ARX_AMB_BAD_SAMPLE_PATH;
      }
      Json json;
      json["filename"] = json_detail::lowerSlashes(sample_path);
      json["flags"] = track.flags & (amb::kTrackPosition | amb::kTrackMaster);
      json["keys"] = Json::array();
      for (const amb::Key& key : track.keys) {
        json["keys"].push_back({
            {"start", key.start_ms},
            {"loop", static_cast<std::uint64_t>(key.loop_minus_one) + 1},
            {"delayMin", key.delay_min_ms},
            {"delayMax", key.delay_max_ms},
            {"volume", settingJson(key.volume)},
            {"pitch", settingJson(key.pitch)},
            {"pan", settingJson(key.pan)},
            {"x", settingJson(key.x)},
            {"y", settingJson(key.y)},
            {"z", settingJson(key.z)},
        });
      }
      root["tracks"].push_back(std::move(json));
    }

    return json_detail::dump(root, pretty, output);
  });
  if (code != ARX_OK) return api_detail::ambFailure<std::string>(code, std::move(location));
  return AmbResult<std::string>::success(std::move(output));
}

JsonResult<amb::Data> importJsonToAmb(std::string_view text, NativeTextMode text_mode) {
  amb::Data result;
  std::optional<JsonLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    return importAmb(text, text_mode, result, location);
  });
  if (code != ARX_OK) return api_detail::jsonFailure<amb::Data>(code, std::move(location));
  return JsonResult<amb::Data>::success(std::move(result));
}

}  // namespace pistoris
