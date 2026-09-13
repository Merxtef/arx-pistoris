// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/llf.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/llf.hpp"

#include "external/json.h"
#include "external/json/native_common.h"
#include "native/write_metadata.h"
#include "utils/return_code.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

constexpr std::string_view kLlfSchema = "https://arx-tools.github.io/schemas/llf.schema.json";

json_detail::Json lightJson(const llf::Light& light) {
  return {
      {"position", json_detail::vector(light.position)},
      {"color", json_detail::color(light.color)},
      {"fallStart", light.fallstart},
      {"fallEnd", light.fallend},
      {"intensity", light.intensity},
      {"exFlicker", json_detail::color(light.flicker)},
      {"exRadius", light.effect_radius},
      {"exFrequency", light.effect_frequency},
      {"exSize", light.effect_size},
      {"exSpeed", light.effect_speed},
      {"exFlareSize", light.flare_size},
      {"flags", light.flags},
  };
}

bool parseLight(const json_detail::Json& json, llf::Light& out) {
  const json_detail::Json* position = json_detail::member(json, "position");
  const json_detail::Json* color = json_detail::member(json, "color");
  const json_detail::Json* fall_start = json_detail::member(json, "fallStart");
  const json_detail::Json* fall_end = json_detail::member(json, "fallEnd");
  const json_detail::Json* intensity = json_detail::member(json, "intensity");
  const json_detail::Json* flicker = json_detail::member(json, "exFlicker");
  const json_detail::Json* radius = json_detail::member(json, "exRadius");
  const json_detail::Json* frequency = json_detail::member(json, "exFrequency");
  const json_detail::Json* size = json_detail::member(json, "exSize");
  const json_detail::Json* speed = json_detail::member(json, "exSpeed");
  const json_detail::Json* flare = json_detail::member(json, "exFlareSize");
  const json_detail::Json* flags = json_detail::member(json, "flags");
  return position && color && fall_start && fall_end && intensity && flicker && radius && frequency && size && speed &&
         flare && flags && json_detail::getVector(*position, out.position) &&
         json_detail::getColor(*color, out.color) && json_detail::getFloat(*fall_start, out.fallstart) &&
         json_detail::getFloat(*fall_end, out.fallend) && json_detail::getFloat(*intensity, out.intensity) &&
         json_detail::getColor(*flicker, out.flicker) && json_detail::getFloat(*radius, out.effect_radius) &&
         json_detail::getFloat(*frequency, out.effect_frequency) && json_detail::getFloat(*size, out.effect_size) &&
         json_detail::getFloat(*speed, out.effect_speed) && json_detail::getFloat(*flare, out.flare_size) &&
         json_detail::getUnsigned(*flags, out.flags);
}

ArxReturnCode importLlf(std::string_view text, llf::Data& out) {
  json_detail::Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root));
  if (!root.is_object() || !json_detail::validSchema(root, kLlfSchema)) return ARX_JSON_BAD_SCHEMA;

  const json_detail::Json* header = json_detail::member(root, "header");
  if (!header || !header->is_object()) return ARX_JSON_BAD_SCHEMA;
  const json_detail::Json* last_modified_by = json_detail::member(*header, "lastModifiedBy");
  const json_detail::Json* last_modified_at = json_detail::member(*header, "lastModifiedAt");
  const json_detail::Json* polygon_count = json_detail::member(*header, "numberOfPolygonsInFTS");
  std::string ignored_text;
  std::uint32_t ignored_value = 0;
  if (!last_modified_by || !last_modified_at || !polygon_count ||
      !json_detail::getString(*last_modified_by, ignored_text) ||
      !json_detail::getUnsigned(*last_modified_at, ignored_value) ||
      !json_detail::getUnsigned(*polygon_count, ignored_value)) {
    return ARX_JSON_BAD_SCHEMA;
  }

  const json_detail::Json* lights = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "lights", lights, kLlfMaxLights));
  out.lights.reserve(lights->size());
  for (const json_detail::Json& item : *lights) {
    llf::Light light;
    if (!parseLight(item, light)) return ARX_JSON_BAD_SCHEMA;
    out.lights.push_back(light);
  }

  const json_detail::Json* colors = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "colors", colors, kLlfMaxColors));
  out.colors.reserve(colors->size());
  for (const json_detail::Json& item : *colors) {
    ArxColor3 color;
    if (!json_detail::getColor(item, color)) return ARX_JSON_BAD_SCHEMA;
    out.colors.push_back(color);
  }
  return validateLlf(&out);
}

}  // namespace

ArxReturnCode exportLlfToJson(const llf::Data& data, bool pretty, std::string_view signer, std::string& out) {
  return json_detail::guarded("LLF export", [&]() -> ArxReturnCode {
    ARX_RETURN_IF_ERR(validateLlf(&data));
    const NativeWriteMetadata metadata = nativeWriteMetadata(signer);
    json_detail::Json root;
    root["$schema"] = kLlfSchema;
    root["header"] = {
        {"lastModifiedBy", metadata.lastUser()},
        {"lastModifiedAt", metadata.modified_at},
        {"numberOfPolygonsInFTS", 0},
    };
    root["lights"] = json_detail::Json::array();
    for (const llf::Light& light : data.lights) root["lights"].push_back(lightJson(light));
    root["colors"] = json_detail::Json::array();
    for (const ArxColor3& color : data.colors) root["colors"].push_back(json_detail::color(color));
    return json_detail::dump(root, pretty, out);
  });
}

ArxReturnCode importJsonToLlf(std::string_view text, llf::Data* out) {
  if (!out) return ARX_INVALID_DATA_POINTER;
  return json_detail::guarded("LLF import", [&] {
    llf::Data temporary;
    ArxReturnCode rc = importLlf(text, temporary);
    if (rc == ARX_OK) *out = std::move(temporary);
    return rc;
  });
}

}  // namespace pistoris
