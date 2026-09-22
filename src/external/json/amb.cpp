// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/amb.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/text.hpp"

#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

using Json = json_detail::Json;

constexpr std::string_view kAmbSchema = "https://arx-tools.github.io/schemas/amb.schema.json";
constexpr std::uint32_t kJsonTrackFlagMask = 0x3f;
constexpr std::uint64_t kMaxJsonLoop = static_cast<std::uint64_t>(UINT32_MAX) + 1;

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

ArxReturnCode importAmb(std::string_view text, NativeTextMode text_mode, amb::Data& out) {
  Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root));
  if (!root.is_object() || !json_detail::validSchema(root, kAmbSchema)) return ARX_JSON_BAD_SCHEMA;

  const Json* tracks = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "tracks", tracks, UINT32_MAX));
  out.tracks.reserve(tracks->size());
  for (const Json& json : *tracks) {
    const Json* filename = json_detail::member(json, "filename");
    const Json* flags = json_detail::member(json, "flags");
    const Json* keys = nullptr;
    amb::Track track;
    std::string sample_path;
    if (!filename || !flags || !json_detail::getString(*filename, sample_path) ||
        !native_text::encode(sample_path, text_mode, track.sample_path) ||
        !json_detail::getUnsigned(*flags, track.flags) || (track.flags & ~kJsonTrackFlagMask) != 0) {
      return ARX_JSON_BAD_SCHEMA;
    }
    ARX_RETURN_IF_ERR(json_detail::arrayMember(json, "keys", keys, UINT32_MAX));
    track.keys.reserve(keys->size());
    for (const Json& key_json : *keys) {
      amb::Key key;
      if (!parseKey(key_json, key)) return ARX_JSON_BAD_SCHEMA;
      track.keys.push_back(key);
    }
    out.tracks.push_back(std::move(track));
  }

  ARX_RETURN_IF_ERR(canonicalizeAmb(&out));
  return validateAmb(&out);
}

}  // namespace

ArxReturnCode exportAmbToJson(const amb::Data& data, bool pretty, NativeTextMode text_mode, std::string& out) {
  return json_detail::guarded("AMB export", [&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    ARX_RETURN_IF_ERR(validateAmb(&data));

    Json root;
    root["$schema"] = kAmbSchema;
    root["tracks"] = Json::array();
    for (const amb::Track& track : data.tracks) {
      std::string sample_path;
      if (!native_text::decode(track.sample_path, text_mode, sample_path)) return ARX_AMB_BAD_SAMPLE_PATH;
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

    return json_detail::dump(root, pretty, out);
  });
}

ArxReturnCode importJsonToAmb(std::string_view text, NativeTextMode text_mode, amb::Data* out) {
  if (!out) return ARX_INVALID_DATA_POINTER;
  return json_detail::guarded("AMB import", [&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    amb::Data temporary;
    const ArxReturnCode rc = importAmb(text, text_mode, temporary);
    if (rc == ARX_OK) *out = std::move(temporary);
    return rc;
  });
}

}  // namespace pistoris
