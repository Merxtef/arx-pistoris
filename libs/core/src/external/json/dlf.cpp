// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/dlf.h"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"

#include "api/result_failure.h"
#include "external/json.h"
#include "external/json/native_common.h"
#include "native/write_metadata.h"
#include "paths/entity_class.h"
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

constexpr std::string_view kDlfSchema = "https://arx-tools.github.io/schemas/dlf.schema.json";
constexpr std::string_view kEntityPrefix = "graph/obj3d/interactive/";

std::string jsonPointer(const DlfLocation& location) {
  if (location.element == DlfElement::kHeader) {
    if (location.field == "scene_path") return "/header/level";
    if (location.field == "player_spawn.position") return "/header/player/position";
    if (location.field == "player_spawn.angle") return "/header/player/orientation";
    if (location.field == "entities") return "/interactiveObjects";
    if (location.field == "fogs") return "/fogs";
    if (location.field == "zones") return "/zones";
    if (location.field == "paths") return "/paths";
    return "/header";
  }
  if (location.index == kNoElementIndex) return {};
  std::string_view array;
  switch (location.element) {
    case DlfElement::kEntity:
      array = "/interactiveObjects";
      break;
    case DlfElement::kFog:
      array = "/fogs";
      break;
    case DlfElement::kZone:
    case DlfElement::kZonePoint:
      array = "/zones";
      break;
    case DlfElement::kPath:
    case DlfElement::kPathNode:
      array = "/paths";
      break;
    case DlfElement::kHeader:
      return "/header";
  }
  std::string result = json_detail::indexedPointer(array, location.index);
  if (location.element == DlfElement::kZonePoint || location.element == DlfElement::kPathNode) {
    result += "/points";
    if (location.subindex != kNoElementIndex) result += "/" + std::to_string(location.subindex);
    if (location.element == DlfElement::kZonePoint) {
      result += "/position";
      if (!location.field.empty()) result += "/" + location.field;
    } else if (location.field == "relative_position")
      result += "/position";
    else if (location.field == "time_ms")
      result += "/time";
    else if (!location.field.empty())
      result += "/" + location.field;
  } else if (location.field == "class_path") {
    result += "/name";
  } else if (location.field == "ambiance.name") {
    result += "/ambience";
  } else if (location.field == "ambiance.volume") {
    result += "/ambienceMaxVolume";
  } else if ((location.element == DlfElement::kEntity || location.element == DlfElement::kFog) &&
             location.field == "angle") {
    result += "/orientation";
  } else if (location.element == DlfElement::kFog && location.field == "rotate_speed") {
    result += "/rotateSpeed";
  } else if (location.element == DlfElement::kZone && location.field == "color") {
    result += "/backgroundColor";
  } else if (location.element == DlfElement::kZone && location.field == "farclip") {
    result += "/drawDistance";
  } else if ((location.element == DlfElement::kZone && location.field == "points") ||
             (location.element == DlfElement::kPath && location.field == "nodes")) {
    result += "/points";
  } else if (!location.field.empty()) {
    result += "/" + location.field;
  }
  return result;
}

bool dlfLevel(const dlf::Data& data, NativeTextMode text_mode, std::uint32_t& out) {
  std::string scene_path;
  if (!native_text::decode(json_detail::fixedString(data.scene_path), text_mode, scene_path)) return false;
  std::string fts_path;
  return paths::ftsFromDlfScene(scene_path, fts_path) && paths::levelFromFts(fts_path, out);
}

std::string entityJsonName(std::string_view class_path) {
  std::string normalized = json_detail::lowerSlashes(class_path);
  if (normalized.starts_with(kEntityPrefix)) normalized.erase(0, kEntityPrefix.size());
  const std::size_t last_separator = normalized.find_last_of('/');
  if (last_separator == std::string::npos) return normalized;
  const std::string_view filename(normalized.data() + last_separator + 1, normalized.size() - last_separator - 1);
  const std::size_t parent_separator = normalized.find_last_of('/', last_separator - 1);
  const std::size_t parent_begin = parent_separator == std::string::npos ? 0 : parent_separator + 1;
  const std::string_view parent(normalized.data() + parent_begin, last_separator - parent_begin);
  if (parent == filename) {
    normalized.resize(last_separator);
  } else {
    normalized += ".asl";
  }
  return normalized;
}

bool entityClassPath(std::string_view json_name, std::string& out) {
  std::string name = json_detail::lowerSlashes(json_name);
  while (!name.empty() && name.back() == '/') name.pop_back();
  if (name.empty() || name.front() == '/') return false;

  std::string candidate(kEntityPrefix);
  if (name.ends_with(".asl")) {
    name.resize(name.size() - 4);
    candidate += name;
  } else {
    std::string legacy_extension;
    if (name.size() >= 4 && isLegacyTeoExtension(std::string_view(name).substr(name.size() - 4))) {
      legacy_extension = name.substr(name.size() - 4);
      name.resize(name.size() - 4);
    }
    const std::size_t separator = name.find_last_of('/');
    const std::string_view leaf =
        separator == std::string::npos ? std::string_view(name) : std::string_view(name).substr(separator + 1);
    if (leaf.empty()) return false;
    candidate += name;
    candidate.push_back('/');
    candidate.append(leaf);
    candidate += legacy_extension;
  }
  std::string normalized;
  std::string_view removed_extension;
  if (!normalizeEntityClassPath(candidate, normalized, removed_extension) ||
      (!removed_extension.empty() && !isLegacyTeoExtension(removed_extension))) {
    return false;
  }
  out = std::move(candidate);
  return true;
}

json_detail::Json pointJson(const ArxVector3& position, dlf::PathNodeType type, std::uint32_t time) {
  return {
      {"position", json_detail::vector(position)},
      {"type", static_cast<std::uint8_t>(type)},
      {"time", time},
  };
}

bool parsePoint(const json_detail::Json& json, ArxVector3& position, dlf::PathNodeType& type, std::uint32_t& time) {
  const json_detail::Json* position_json = json_detail::member(json, "position");
  const json_detail::Json* type_json = json_detail::member(json, "type");
  const json_detail::Json* time_json = json_detail::member(json, "time");
  std::uint8_t parsed_type = 0;
  if (!position_json || !type_json || !time_json || !json_detail::getVector(*position_json, position) ||
      !json_detail::getUnsigned(*type_json, parsed_type) || parsed_type > 2 ||
      !json_detail::getUnsigned(*time_json, time)) {
    return false;
  }
  type = static_cast<dlf::PathNodeType>(parsed_type);
  return true;
}

json_detail::Json fogJson(const dlf::Fog& fog) {
  return {
      {"position", json_detail::vector(fog.position)},
      {"color", json_detail::color(fog.color)},
      {"size", fog.size},
      {"special", fog.directional ? 1 : 0},
      {"scale", fog.scale},
      {"move", json_detail::vector({})},
      {"orientation", json_detail::angle(fog.angle)},
      {"speed", fog.speed},
      {"rotateSpeed", fog.rotate_speed},
      {"toLive", fog.lifetime_ms},
      {"frequency", fog.frequency},
  };
}

bool parseFog(const json_detail::Json& json, dlf::Fog& out) {
  const json_detail::Json* position = json_detail::member(json, "position");
  const json_detail::Json* color = json_detail::member(json, "color");
  const json_detail::Json* size = json_detail::member(json, "size");
  const json_detail::Json* special = json_detail::member(json, "special");
  const json_detail::Json* scale = json_detail::member(json, "scale");
  const json_detail::Json* move = json_detail::member(json, "move");
  const json_detail::Json* orientation = json_detail::member(json, "orientation");
  const json_detail::Json* speed = json_detail::member(json, "speed");
  const json_detail::Json* rotate_speed = json_detail::member(json, "rotateSpeed");
  const json_detail::Json* lifetime = json_detail::member(json, "toLive");
  const json_detail::Json* frequency = json_detail::member(json, "frequency");
  std::int32_t flags = 0;
  ArxVector3 ignored_move;
  return position && color && size && special && scale && move && orientation && speed && rotate_speed && lifetime &&
         frequency && json_detail::getVector(*position, out.position) && json_detail::getColor(*color, out.color) &&
         json_detail::getFloat(*size, out.size) && json_detail::getSigned(*special, flags) &&
         json_detail::getFloat(*scale, out.scale) && json_detail::getVector(*move, ignored_move) &&
         json_detail::getAngle(*orientation, out.angle) && json_detail::getFloat(*speed, out.speed) &&
         json_detail::getFloat(*rotate_speed, out.rotate_speed) && json_detail::getSigned(*lifetime, out.lifetime_ms) &&
         json_detail::getFloat(*frequency, out.frequency) && (out.directional = (flags & 1) != 0, true);
}

ArxReturnCode parseHeader(const json_detail::Json& root, NativeTextMode text_mode, dlf::Data& out,
                          std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* header = json_detail::member(root, "header");
  if (!header || !header->is_object()) return json_detail::schemaFailure(failure_location, "/header");
  const json_detail::Json* last_modified_by = json_detail::member(*header, "lastModifiedBy");
  const json_detail::Json* last_modified_at = json_detail::member(*header, "lastModifiedAt");
  const json_detail::Json* player = json_detail::member(*header, "player");
  const json_detail::Json* polygon_count = json_detail::member(*header, "numberOfPolygonsInFTS");
  const json_detail::Json* level_index = json_detail::member(*header, "levelIdx");
  if (!last_modified_by || !last_modified_at || !player || !polygon_count || !level_index) {
    return json_detail::schemaFailure(failure_location, "/header");
  }

  std::string ignored_text;
  std::uint32_t ignored_value = 0;
  std::uint32_t level = 0;
  const json_detail::Json* position = json_detail::member(*player, "position");
  const json_detail::Json* orientation = json_detail::member(*player, "orientation");
  if (!json_detail::getString(*last_modified_by, ignored_text) ||
      !json_detail::getUnsigned(*last_modified_at, ignored_value) ||
      !json_detail::getUnsigned(*polygon_count, ignored_value) || !json_detail::getUnsigned(*level_index, level) ||
      !position || !orientation || !json_detail::getVector(*position, out.player_spawn.position) ||
      !json_detail::getAngle(*orientation, out.player_spawn.angle)) {
    return json_detail::schemaFailure(failure_location, "/header");
  }

  const std::string level_name = "level" + std::to_string(level);
  std::string scene_path;
  if (!paths::dlfSceneFromLevelName(level_name, scene_path) ||
      !native_text::encodeFixed(scene_path, text_mode, out.scene_path))
    return json_detail::schemaFailure(failure_location, "/header/levelIdx");
  return ARX_OK;
}

ArxReturnCode parseEntities(const json_detail::Json& root, NativeTextMode text_mode, dlf::Data& out,
                            std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* entities = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "interactiveObjects", entities, kDlfMaxEntities);
      rc != ARX_OK) {
    return json_detail::schemaFailure(failure_location, "/interactiveObjects", rc);
  }
  out.entities.reserve(entities->size());
  for (std::size_t index = 0; index < entities->size(); ++index) {
    const json_detail::Json& json = (*entities)[index];
    const json_detail::Json* name = json_detail::member(json, "name");
    const json_detail::Json* position = json_detail::member(json, "position");
    const json_detail::Json* orientation = json_detail::member(json, "orientation");
    const json_detail::Json* identifier = json_detail::member(json, "identifier");
    std::string json_name;
    std::string class_path;
    dlf::Entity entity;
    if (!name || !position || !orientation || !identifier || !json_detail::getString(*name, json_name) ||
        !entityClassPath(json_name, class_path) ||
        !native_text::encodeFixed(class_path, text_mode, entity.class_path) ||
        !json_detail::getVector(*position, entity.position) || !json_detail::getAngle(*orientation, entity.angle) ||
        !json_detail::getSigned(*identifier, entity.ident)) {
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/interactiveObjects", index));
    }
    out.entities.push_back(entity);
  }
  return ARX_OK;
}

ArxReturnCode parseFogs(const json_detail::Json& root, dlf::Data& out, std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* fogs = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "fogs", fogs, kDlfMaxFogs); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/fogs", rc);
  out.fogs.reserve(fogs->size());
  for (std::size_t index = 0; index < fogs->size(); ++index) {
    dlf::Fog fog;
    if (!parseFog((*fogs)[index], fog))
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/fogs", index));
    out.fogs.push_back(fog);
  }
  return ARX_OK;
}

ArxReturnCode parsePaths(const json_detail::Json& root, NativeTextMode text_mode, dlf::Data& out,
                         std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* paths = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "paths", paths, kDlfMaxPaths); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/paths", rc);
  out.paths.reserve(paths->size());
  for (std::size_t index = 0; index < paths->size(); ++index) {
    const json_detail::Json& json = (*paths)[index];
    const std::string base = json_detail::indexedPointer("/paths", index);
    const json_detail::Json* name = json_detail::member(json, "name");
    const json_detail::Json* points = nullptr;
    dlf::Path path;
    std::string path_name;
    if (!name || !json_detail::getString(*name, path_name) ||
        !native_text::encodeFixed(path_name, text_mode, path.name))
      return json_detail::schemaFailure(failure_location, base + "/name");
    if (const ArxReturnCode rc = json_detail::arrayMember(json, "points", points, kDlfMaxPathNodes); rc != ARX_OK)
      return json_detail::schemaFailure(failure_location, base + "/points", rc);
    if (points->empty()) return json_detail::schemaFailure(failure_location, base + "/points");
    path.nodes.reserve(points->size());
    for (std::size_t point_index = 0; point_index < points->size(); ++point_index) {
      ArxVector3 absolute;
      dlf::PathNode node;
      if (!parsePoint((*points)[point_index], absolute, node.type, node.time_ms))
        return json_detail::schemaFailure(failure_location, base + "/points/" + std::to_string(point_index));
      if (path.nodes.empty()) path.position = absolute;
      node.relative_position = json_detail::subtract(absolute, path.position);
      path.nodes.push_back(node);
    }
    out.paths.push_back(std::move(path));
  }
  return ARX_OK;
}

ArxReturnCode parseZones(const json_detail::Json& root, NativeTextMode text_mode, dlf::Data& out,
                         std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* zones = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "zones", zones, kDlfMaxPaths); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/zones", rc);
  if (zones->size() > kDlfMaxPaths - out.paths.size())
    return json_detail::schemaFailure(failure_location, "/zones", ARX_JSON_LIMIT_EXCEEDED);
  out.zones.reserve(zones->size());
  for (std::size_t index = 0; index < zones->size(); ++index) {
    const json_detail::Json& json = (*zones)[index];
    const std::string base = json_detail::indexedPointer("/zones", index);
    const json_detail::Json* name = json_detail::member(json, "name");
    const json_detail::Json* height = json_detail::member(json, "height");
    const json_detail::Json* points = nullptr;
    dlf::Zone zone;
    std::string zone_name;
    if (!name || !height || !json_detail::getString(*name, zone_name) ||
        !native_text::encodeFixed(zone_name, text_mode, zone.name) || !json_detail::getSigned(*height, zone.height)) {
      return json_detail::schemaFailure(failure_location, base);
    }
    if (const ArxReturnCode rc = json_detail::arrayMember(json, "points", points, kDlfMaxPathNodes); rc != ARX_OK)
      return json_detail::schemaFailure(failure_location, base + "/points", rc);
    if (points->empty()) return json_detail::schemaFailure(failure_location, base + "/points");
    zone.points.reserve(points->size());
    for (std::size_t point_index = 0; point_index < points->size(); ++point_index) {
      ArxVector3 absolute;
      dlf::PathNodeType ignored_type;
      std::uint32_t ignored_time = 0;
      if (!parsePoint((*points)[point_index], absolute, ignored_type, ignored_time))
        return json_detail::schemaFailure(failure_location, base + "/points/" + std::to_string(point_index));
      if (zone.points.empty()) zone.position = absolute;
      zone.points.push_back(json_detail::subtract(absolute, zone.position));
    }

    if (const json_detail::Json* color = json_detail::member(json, "backgroundColor")) {
      ArxColor3 parsed;
      if (!json_detail::getColor(*color, parsed))
        return json_detail::schemaFailure(failure_location, base + "/backgroundColor");
      zone.color = parsed;
    }
    if (const json_detail::Json* distance = json_detail::member(json, "drawDistance")) {
      float parsed = 0.0f;
      if (!json_detail::getFloat(*distance, parsed))
        return json_detail::schemaFailure(failure_location, base + "/drawDistance");
      zone.farclip = parsed;
    }

    const json_detail::Json* ambiance = json_detail::member(json, "ambience");
    const json_detail::Json* volume = json_detail::member(json, "ambienceMaxVolume");
    if ((ambiance == nullptr) != (volume == nullptr))
      return json_detail::schemaFailure(failure_location, base + "/ambience");
    if (ambiance) {
      std::string name_value;
      float volume_value = 0.0f;
      if (!json_detail::getString(*ambiance, name_value) || !json_detail::getFloat(*volume, volume_value)) {
        return json_detail::schemaFailure(failure_location, base + "/ambience");
      }
      zone.ambiance.emplace();
      if (!native_text::encodeFixed(name_value, text_mode, zone.ambiance->name))
        return json_detail::schemaFailure(failure_location, base + "/ambience");
      zone.ambiance->volume = volume_value;
    }
    out.zones.push_back(std::move(zone));
  }
  return ARX_OK;
}

ArxReturnCode importDlf(std::string_view text, NativeTextMode text_mode, dlf::Data& out,
                        std::optional<JsonLocation>& failure_location) {
  json_detail::Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root, failure_location));
  if (!root.is_object()) return json_detail::schemaFailure(failure_location, {});
  if (!json_detail::validSchema(root, kDlfSchema)) return json_detail::schemaFailure(failure_location, "/$schema");
  ArxReturnCode code = parseHeader(root, text_mode, out, failure_location);
  if (code != ARX_OK) return code;
  code = parseEntities(root, text_mode, out, failure_location);
  if (code != ARX_OK) return code;
  code = parseFogs(root, out, failure_location);
  if (code != ARX_OK) return code;
  code = parsePaths(root, text_mode, out, failure_location);
  if (code != ARX_OK) return code;
  code = parseZones(root, text_mode, out, failure_location);
  if (code != ARX_OK) return code;
  DlfLocation native_location;
  if (const ArxReturnCode rc = canonicalizeDlf(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  if (const ArxReturnCode rc = validateDlf(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  return ARX_OK;
}

}  // namespace

DlfResult<std::string> exportDlfToJson(const dlf::Data& data, bool pretty, std::string_view signer,
                                       NativeTextMode text_mode) {
  std::string output;
  std::optional<DlfLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    DlfLocation validation_location;
    if (const ArxReturnCode rc = validateDlf(&data, &validation_location); rc != ARX_OK) {
      location = std::move(validation_location);
      return rc;
    }
    std::uint32_t level = 0;
    if (!dlfLevel(data, text_mode, level)) {
      location = DlfLocation{.element = DlfElement::kHeader, .field = "scene_path"};
      return ARX_JSON_BAD_SCHEMA;
    }
    NativeWriteMetadata metadata;
    ARX_RETURN_IF_ERR(nativeWriteMetadata(signer, metadata));
    json_detail::Json root;
    root["$schema"] = kDlfSchema;
    root["header"] = {
        {"lastModifiedBy", metadata.lastUser()},
        {"lastModifiedAt", metadata.modified_at},
        {"player",
         {
             {"position", json_detail::vector(data.player_spawn.position)},
             {"orientation", json_detail::angle(data.player_spawn.angle)},
         }},
        {"numberOfPolygonsInFTS", 0},
        {"levelIdx", level},
    };

    root["interactiveObjects"] = json_detail::Json::array();
    for (std::size_t entity_index = 0; entity_index < data.entities.size(); ++entity_index) {
      const dlf::Entity& entity = data.entities[entity_index];
      std::string class_path;
      if (!json_detail::decodeFixed(entity.class_path, text_mode, class_path)) {
        location = DlfLocation{.element = DlfElement::kEntity, .index = entity_index, .field = "class_path"};
        return ARX_DLF_BAD_ENTITY_CLASS_PATH;
      }
      root["interactiveObjects"].push_back({
          {"name", entityJsonName(class_path)},
          {"position", json_detail::vector(entity.position)},
          {"orientation", json_detail::angle(entity.angle)},
          {"identifier", entity.ident},
      });
    }
    root["fogs"] = json_detail::Json::array();
    for (const dlf::Fog& fog : data.fogs) root["fogs"].push_back(fogJson(fog));

    root["paths"] = json_detail::Json::array();
    for (std::size_t path_index = 0; path_index < data.paths.size(); ++path_index) {
      const dlf::Path& path = data.paths[path_index];
      std::string name;
      if (!json_detail::decodeFixed(path.name, text_mode, name)) {
        location = DlfLocation{.element = DlfElement::kPath, .index = path_index, .field = "name"};
        return ARX_DLF_BAD_PATH_NAME;
      }
      json_detail::Json json = {{"name", name}, {"points", json_detail::Json::array()}};
      for (const dlf::PathNode& node : path.nodes) {
        json["points"].push_back(
            pointJson(json_detail::add(path.position, node.relative_position), node.type, node.time_ms));
      }
      root["paths"].push_back(std::move(json));
    }

    root["zones"] = json_detail::Json::array();
    for (std::size_t zone_index = 0; zone_index < data.zones.size(); ++zone_index) {
      const dlf::Zone& zone = data.zones[zone_index];
      std::string name;
      if (!json_detail::decodeFixed(zone.name, text_mode, name)) {
        location = DlfLocation{.element = DlfElement::kZone, .index = zone_index, .field = "name"};
        return ARX_DLF_BAD_ZONE_NAME;
      }
      json_detail::Json json = {
          {"name", name},
          {"height", zone.height},
          {"points", json_detail::Json::array()},
      };
      for (const ArxVector3& point : zone.points) {
        json["points"].push_back(pointJson(json_detail::add(zone.position, point), dlf::PathNodeType::kStandard, 0));
      }
      const auto& color = zone.color;
      if (color) json["backgroundColor"] = json_detail::color(*color);
      const auto& farclip = zone.farclip;
      if (farclip) json["drawDistance"] = *farclip;
      const auto& ambiance = zone.ambiance;
      if (ambiance) {
        std::string ambiance_name;
        if (!json_detail::decodeFixed(ambiance->name, text_mode, ambiance_name)) {
          location = DlfLocation{.element = DlfElement::kZone, .index = zone_index, .field = "ambiance.name"};
          return ARX_DLF_BAD_ZONE_AMBIANCE;
        }
        json["ambienceMaxVolume"] = ambiance->volume;
        json["ambience"] = ambiance_name;
      }
      root["zones"].push_back(std::move(json));
    }
    return json_detail::dump(root, pretty, output);
  });
  if (code != ARX_OK) return api_detail::dlfFailure<std::string>(code, std::move(location));
  return DlfResult<std::string>::success(std::move(output));
}

JsonResult<dlf::Data> importJsonToDlf(std::string_view text, NativeTextMode text_mode) {
  dlf::Data result;
  std::optional<JsonLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    return importDlf(text, text_mode, result, location);
  });
  if (code != ARX_OK) return api_detail::jsonFailure<dlf::Data>(code, std::move(location));
  return JsonResult<dlf::Data>::success(std::move(result));
}

}  // namespace pistoris
