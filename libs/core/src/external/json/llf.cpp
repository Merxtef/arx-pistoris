// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/llf.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"

#include "api/result_failure.h"
#include "external/json.h"
#include "external/json/native_common.h"
#include "native/write_metadata.h"
#include "utils/return_code.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

constexpr std::string_view kLlfSchema = "https://arx-tools.github.io/schemas/llf.schema.json";

std::string jsonPointer(const LlfLocation& location) {
  if (location.element == LlfElement::kHeader) {
    if (location.field == "lights") return "/lights";
    if (location.field == "colors") return "/colors";
    return "/header";
  }
  if (location.index == kNoElementIndex) return {};
  if (location.element == LlfElement::kLight) {
    std::string result = json_detail::indexedPointer("/lights", location.index);
    if (location.field == "fallstart") return result + "/fallStart";
    if (location.field == "fallend") return result + "/fallEnd";
    if (location.field == "flicker") return result + "/exFlicker";
    if (location.field == "effect_radius") return result + "/exRadius";
    if (location.field == "effect_frequency") return result + "/exFrequency";
    if (location.field == "effect_size") return result + "/exSize";
    if (location.field == "effect_speed") return result + "/exSpeed";
    if (location.field == "flare_size") return result + "/exFlareSize";
    if (!location.field.empty()) return result + "/" + location.field;
    return result;
  }
  std::string result = json_detail::indexedPointer("/colors", location.index);
  if (!location.field.empty()) result += "/" + location.field;
  return result;
}

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

ArxReturnCode importLlf(std::string_view text, llf::Data& out, std::optional<JsonLocation>& failure_location) {
  json_detail::Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root, failure_location));
  if (!root.is_object()) return json_detail::schemaFailure(failure_location, {});
  if (!json_detail::validSchema(root, kLlfSchema)) return json_detail::schemaFailure(failure_location, "/$schema");

  const json_detail::Json* header = json_detail::member(root, "header");
  if (!header || !header->is_object()) return json_detail::schemaFailure(failure_location, "/header");
  const json_detail::Json* last_modified_by = json_detail::member(*header, "lastModifiedBy");
  const json_detail::Json* last_modified_at = json_detail::member(*header, "lastModifiedAt");
  const json_detail::Json* polygon_count = json_detail::member(*header, "numberOfPolygonsInFTS");
  std::string ignored_text;
  std::uint32_t ignored_value = 0;
  if (!last_modified_by || !last_modified_at || !polygon_count ||
      !json_detail::getString(*last_modified_by, ignored_text) ||
      !json_detail::getUnsigned(*last_modified_at, ignored_value) ||
      !json_detail::getUnsigned(*polygon_count, ignored_value)) {
    return json_detail::schemaFailure(failure_location, "/header");
  }

  const json_detail::Json* lights = nullptr;
  ArxReturnCode code = json_detail::arrayMember(root, "lights", lights, kLlfMaxLights);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/lights", code);
  out.lights.reserve(lights->size());
  for (std::size_t index = 0; index < lights->size(); ++index) {
    llf::Light light;
    if (!parseLight((*lights)[index], light))
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/lights", index));
    out.lights.push_back(light);
  }

  const json_detail::Json* colors = nullptr;
  code = json_detail::arrayMember(root, "colors", colors, kLlfMaxColors);
  if (code != ARX_OK) return json_detail::schemaFailure(failure_location, "/colors", code);
  out.colors.reserve(colors->size());
  for (std::size_t index = 0; index < colors->size(); ++index) {
    ArxColor3 color;
    if (!json_detail::getColor((*colors)[index], color))
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/colors", index));
    out.colors.push_back(color);
  }
  LlfLocation native_location;
  const ArxReturnCode rc = validateLlf(&out, &native_location);
  return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
}

}  // namespace

LlfResult<std::string> exportLlfToJson(const llf::Data& data, bool pretty, std::string_view signer) {
  std::string output;
  std::optional<LlfLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    LlfLocation validation_location;
    if (const ArxReturnCode rc = validateLlf(&data, &validation_location); rc != ARX_OK) {
      location = std::move(validation_location);
      return rc;
    }
    NativeWriteMetadata metadata;
    ARX_RETURN_IF_ERR(nativeWriteMetadata(signer, metadata));
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
    return json_detail::dump(root, pretty, output);
  });
  if (code != ARX_OK) return api_detail::llfFailure<std::string>(code, std::move(location));
  return LlfResult<std::string>::success(std::move(output));
}

JsonResult<llf::Data> importJsonToLlf(std::string_view text) {
  llf::Data result;
  std::optional<JsonLocation> location;
  const ArxReturnCode code = json_detail::guarded([&] { return importLlf(text, result, location); });
  if (code != ARX_OK) return api_detail::jsonFailure<llf::Data>(code, std::move(location));
  return JsonResult<llf::Data>::success(std::move(result));
}

}  // namespace pistoris
