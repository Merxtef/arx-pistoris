// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/dlf.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/json.h"
#include "external/json/native_common.h"
#include "native/write_metadata.h"
#include "paths/entity_class.h"
#include "utils/log.h"
#include "utils/return_code.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

constexpr std::string_view kDlfSchema = "https://arx-tools.github.io/schemas/dlf.schema.json";
constexpr std::string_view kEntityPrefix = "graph/obj3d/interactive/";

bool dlfLevel(const dlf::Data& data, std::uint32_t& out) {
  std::string fts_path;
  return paths::ftsFromDlfScene(data.scene_path, fts_path) && paths::levelFromFts(fts_path, out);
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

bool entityClassPath(std::string_view json_name, std::string& out, bool& normalized_legacy_teo) {
  normalized_legacy_teo = false;
  std::string name = json_detail::lowerSlashes(json_name);
  while (!name.empty() && name.back() == '/') name.pop_back();
  if (name.empty() || name.front() == '/') return false;

  std::string candidate(kEntityPrefix);
  if (name.ends_with(".asl")) {
    name.resize(name.size() - 4);
    candidate += name;
  } else {
    if (name.size() >= 4 && isLegacyTeoExtension(std::string_view(name).substr(name.size() - 4))) {
      name.resize(name.size() - 4);
      normalized_legacy_teo = true;
    }
    const std::size_t separator = name.find_last_of('/');
    const std::string_view leaf =
        separator == std::string::npos ? std::string_view(name) : std::string_view(name).substr(separator + 1);
    if (leaf.empty()) return false;
    candidate += name;
    candidate.push_back('/');
    candidate.append(leaf);
  }
  std::string_view removed_extension;
  if (!normalizeEntityClassPath(candidate, out, removed_extension) ||
      (!removed_extension.empty() && !isLegacyTeoExtension(removed_extension))) {
    return false;
  }
  if (isLegacyTeoExtension(removed_extension)) normalized_legacy_teo = true;
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

ArxReturnCode parseHeader(const json_detail::Json& root, dlf::Data& out) {
  const json_detail::Json* header = json_detail::member(root, "header");
  if (!header || !header->is_object()) return ARX_JSON_BAD_SCHEMA;
  const json_detail::Json* last_modified_by = json_detail::member(*header, "lastModifiedBy");
  const json_detail::Json* last_modified_at = json_detail::member(*header, "lastModifiedAt");
  const json_detail::Json* player = json_detail::member(*header, "player");
  const json_detail::Json* polygon_count = json_detail::member(*header, "numberOfPolygonsInFTS");
  const json_detail::Json* level_index = json_detail::member(*header, "levelIdx");
  if (!last_modified_by || !last_modified_at || !player || !polygon_count || !level_index) {
    return ARX_JSON_BAD_SCHEMA;
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
    return ARX_JSON_BAD_SCHEMA;
  }

  const std::string level_name = "level" + std::to_string(level);
  if (!paths::dlfSceneFromLevelName(level_name, out.scene_path)) return ARX_JSON_BAD_SCHEMA;
  out.scene_path.push_back('/');
  return ARX_OK;
}

ArxReturnCode parseEntities(const json_detail::Json& root, dlf::Data& out, std::uint64_t& normalized_legacy_teo) {
  const json_detail::Json* entities = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "interactiveObjects", entities, kDlfMaxEntities));
  out.entities.reserve(entities->size());
  for (const json_detail::Json& json : *entities) {
    const json_detail::Json* name = json_detail::member(json, "name");
    const json_detail::Json* position = json_detail::member(json, "position");
    const json_detail::Json* orientation = json_detail::member(json, "orientation");
    const json_detail::Json* identifier = json_detail::member(json, "identifier");
    std::string json_name;
    dlf::Entity entity;
    bool entity_legacy_teo = false;
    if (!name || !position || !orientation || !identifier || !json_detail::getString(*name, json_name) ||
        !entityClassPath(json_name, entity.class_path, entity_legacy_teo) ||
        !json_detail::getVector(*position, entity.position) || !json_detail::getAngle(*orientation, entity.angle) ||
        !json_detail::getSigned(*identifier, entity.ident)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    if (entity_legacy_teo) ++normalized_legacy_teo;
    out.entities.push_back(std::move(entity));
  }
  return ARX_OK;
}

ArxReturnCode parseFogs(const json_detail::Json& root, dlf::Data& out) {
  const json_detail::Json* fogs = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "fogs", fogs, kDlfMaxFogs));
  out.fogs.reserve(fogs->size());
  for (const json_detail::Json& json : *fogs) {
    dlf::Fog fog;
    if (!parseFog(json, fog)) return ARX_JSON_BAD_SCHEMA;
    out.fogs.push_back(fog);
  }
  return ARX_OK;
}

ArxReturnCode parsePaths(const json_detail::Json& root, dlf::Data& out) {
  const json_detail::Json* paths = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "paths", paths, kDlfMaxPaths));
  out.paths.reserve(paths->size());
  for (const json_detail::Json& json : *paths) {
    const json_detail::Json* name = json_detail::member(json, "name");
    const json_detail::Json* points = nullptr;
    dlf::Path path;
    if (!name || !json_detail::getString(*name, path.name)) return ARX_JSON_BAD_SCHEMA;
    ARX_RETURN_IF_ERR(json_detail::arrayMember(json, "points", points, kDlfMaxPathNodes));
    if (points->empty()) return ARX_JSON_BAD_SCHEMA;
    path.nodes.reserve(points->size());
    for (const json_detail::Json& point : *points) {
      ArxVector3 absolute;
      dlf::PathNode node;
      if (!parsePoint(point, absolute, node.type, node.time_ms)) return ARX_JSON_BAD_SCHEMA;
      if (path.nodes.empty()) path.position = absolute;
      node.relative_position = json_detail::subtract(absolute, path.position);
      path.nodes.push_back(node);
    }
    out.paths.push_back(std::move(path));
  }
  return ARX_OK;
}

ArxReturnCode parseZones(const json_detail::Json& root, dlf::Data& out) {
  const json_detail::Json* zones = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "zones", zones, kDlfMaxPaths));
  if (zones->size() > kDlfMaxPaths - out.paths.size()) return ARX_JSON_LIMIT_EXCEEDED;
  out.zones.reserve(zones->size());
  for (const json_detail::Json& json : *zones) {
    const json_detail::Json* name = json_detail::member(json, "name");
    const json_detail::Json* height = json_detail::member(json, "height");
    const json_detail::Json* points = nullptr;
    dlf::Zone zone;
    if (!name || !height || !json_detail::getString(*name, zone.name) ||
        !json_detail::getSigned(*height, zone.height)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    ARX_RETURN_IF_ERR(json_detail::arrayMember(json, "points", points, kDlfMaxPathNodes));
    if (points->empty()) return ARX_JSON_BAD_SCHEMA;
    zone.points.reserve(points->size());
    for (const json_detail::Json& point : *points) {
      ArxVector3 absolute;
      dlf::PathNodeType ignored_type;
      std::uint32_t ignored_time = 0;
      if (!parsePoint(point, absolute, ignored_type, ignored_time)) return ARX_JSON_BAD_SCHEMA;
      if (zone.points.empty()) zone.position = absolute;
      zone.points.push_back(json_detail::subtract(absolute, zone.position));
    }

    if (const json_detail::Json* color = json_detail::member(json, "backgroundColor")) {
      ArxColor3 parsed;
      if (!json_detail::getColor(*color, parsed)) return ARX_JSON_BAD_SCHEMA;
      zone.color = parsed;
    }
    if (const json_detail::Json* distance = json_detail::member(json, "drawDistance")) {
      float parsed = 0.0f;
      if (!json_detail::getFloat(*distance, parsed)) return ARX_JSON_BAD_SCHEMA;
      zone.farclip = parsed;
    }

    const json_detail::Json* ambiance = json_detail::member(json, "ambience");
    const json_detail::Json* volume = json_detail::member(json, "ambienceMaxVolume");
    if ((ambiance == nullptr) != (volume == nullptr)) return ARX_JSON_BAD_SCHEMA;
    if (ambiance) {
      std::string name_value;
      float volume_value = 0.0f;
      if (!json_detail::getString(*ambiance, name_value) || !json_detail::getFloat(*volume, volume_value)) {
        return ARX_JSON_BAD_SCHEMA;
      }
      zone.ambiance = dlf::ZoneAmbiance{std::move(name_value), volume_value};
    }
    out.zones.push_back(std::move(zone));
  }
  return ARX_OK;
}

ArxReturnCode importDlf(std::string_view text, dlf::Data& out) {
  json_detail::Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root));
  if (!root.is_object() || !json_detail::validSchema(root, kDlfSchema)) return ARX_JSON_BAD_SCHEMA;
  ARX_RETURN_IF_ERR(parseHeader(root, out));
  std::uint64_t normalized_legacy_teo = 0;
  ARX_RETURN_IF_ERR(parseEntities(root, out, normalized_legacy_teo));
  ARX_RETURN_IF_ERR(parseFogs(root, out));
  ARX_RETURN_IF_ERR(parsePaths(root, out));
  ARX_RETURN_IF_ERR(parseZones(root, out));
  ARX_RETURN_IF_ERR(validateDlf(&out));
  if (normalized_legacy_teo != 0) {
    log(ARX_LOG_WARN, "DLF JSON import: normalized {} legacy .teo entity class path(s)", normalized_legacy_teo);
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode exportDlfToJson(const dlf::Data& data, bool pretty, std::string_view signer, std::string& out) {
  return json_detail::guarded("DLF export", [&]() -> ArxReturnCode {
    ARX_RETURN_IF_ERR(validateDlf(&data));
    std::uint32_t level = 0;
    if (!dlfLevel(data, level)) return ARX_JSON_BAD_SCHEMA;
    const NativeWriteMetadata metadata = nativeWriteMetadata(signer);
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
    for (const dlf::Entity& entity : data.entities) {
      root["interactiveObjects"].push_back({
          {"name", entityJsonName(entity.class_path)},
          {"position", json_detail::vector(entity.position)},
          {"orientation", json_detail::angle(entity.angle)},
          {"identifier", entity.ident},
      });
    }
    root["fogs"] = json_detail::Json::array();
    for (const dlf::Fog& fog : data.fogs) root["fogs"].push_back(fogJson(fog));

    root["paths"] = json_detail::Json::array();
    for (const dlf::Path& path : data.paths) {
      json_detail::Json json = {{"name", path.name}, {"points", json_detail::Json::array()}};
      for (const dlf::PathNode& node : path.nodes) {
        json["points"].push_back(
            pointJson(json_detail::add(path.position, node.relative_position), node.type, node.time_ms));
      }
      root["paths"].push_back(std::move(json));
    }

    root["zones"] = json_detail::Json::array();
    for (const dlf::Zone& zone : data.zones) {
      json_detail::Json json = {
          {"name", zone.name},
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
        json["ambienceMaxVolume"] = ambiance->volume;
        json["ambience"] = ambiance->name;
      }
      root["zones"].push_back(std::move(json));
    }
    return json_detail::dump(root, pretty, out);
  });
}

ArxReturnCode importJsonToDlf(std::string_view text, dlf::Data* out) {
  if (!out) return ARX_INVALID_DATA_POINTER;
  return json_detail::guarded("DLF import", [&] {
    dlf::Data temporary;
    ArxReturnCode rc = importDlf(text, temporary);
    if (rc == ARX_OK) *out = std::move(temporary);
    return rc;
  });
}

}  // namespace pistoris
