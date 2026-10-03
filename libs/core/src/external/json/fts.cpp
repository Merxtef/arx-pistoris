// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/fts.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"

#include "api/result_failure.h"
#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/math/finite.h"
#include "utils/native_text.h"
#include "utils/return_code.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr std::string_view kFtsSchema = "https://arx-tools.github.io/schemas/fts.schema.json";

std::string jsonPointer(const FtsLocation& location) {
  if (location.element == FtsElement::kHeader) {
    if (location.field == "scene.Mscenepos") return "/header/mScenePosition";
    if (location.field == "textures") return "/textureContainers";
    if (location.field == "cells" || location.field == "scene.sizex" || location.field == "scene.sizez")
      return "/cells";
    if (location.field == "anchors") return "/anchors";
    if (location.field == "portals") return "/portals";
    if (location.field == "rooms" || location.field == "scene.num_rooms") return "/rooms";
    if (location.field == "scene.num_polys") return "/polygons";
    if (location.field == "scene.num_textures") return "/textureContainers";
    if (location.field == "scene.num_anchors") return "/anchors";
    if (location.field == "scene.num_portals") return "/portals";
    if (location.field == "room_distances") return "/roomDistances";
    return "/header";
  }
  if (location.index == kNoElementIndex) return {};
  std::string_view array;
  switch (location.element) {
    case FtsElement::kCell:
      array = "/cells";
      break;
    case FtsElement::kVertex:
    case FtsElement::kFace:
      array = "/polygons";
      break;
    case FtsElement::kTexture:
      array = "/textureContainers";
      break;
    case FtsElement::kRoom:
      array = "/rooms";
      break;
    case FtsElement::kPortal:
      array = "/portals";
      break;
    case FtsElement::kRoomDistance:
      array = "/roomDistances";
      break;
    case FtsElement::kAnchor:
    case FtsElement::kAnchorConnection:
      array = "/anchors";
      break;
    case FtsElement::kHeader:
      return {};
  }
  std::string result = json_detail::indexedPointer(array, location.index);

  switch (location.element) {
    case FtsElement::kCell:
      if (location.field == "anchor_ids") result += "/anchors";
      break;
    case FtsElement::kVertex:
    case FtsElement::kFace:
      if (location.field == "type") result += "/flags";
      if (location.field == "tex") result += "/textureContainerId";
      if (location.field == "transval") result += "/transval";
      if (location.field == "v") result += "/vertices";
      if (location.field == "nrml") result += "/normals";
      if (location.field == "norm") result += "/norm";
      if (location.field == "norm2") result += "/norm2";
      if (location.field == "area") result += "/area";
      break;
    case FtsElement::kTexture:
      if (location.field == "id") result += "/id";
      if (location.field == "fic") result += "/filename";
      break;
    case FtsElement::kRoom:
      if (location.field == "portal_ids") result += "/portals";
      if (location.field == "polygons") result += "/polygons";
      break;
    case FtsElement::kPortal:
      if (location.field == "room_1") result += "/room1";
      if (location.field == "room_2") result += "/room2";
      if (location.field == "poly" || location.field == "poly.type") result += "/polygon";
      if (location.field == "poly.v") result += "/polygon/vertices";
      if (location.field.starts_with("poly.") && location.field != "poly.type" && location.field != "poly.v")
        result += "/polygon/" + location.field.substr(5);
      break;
    case FtsElement::kRoomDistance:
      if (location.field == "distance") result += "/distance";
      if (location.field == "startpos") result += "/startPosition";
      if (location.field == "endpos") result += "/endPosition";
      break;
    case FtsElement::kAnchor:
      if (location.field == "data.pos") result += "/data/position";
      if (location.field == "data.radius") result += "/data/radius";
      if (location.field == "data.height") result += "/data/height";
      if (location.field == "linked") result += "/linkedAnchors";
      break;
    case FtsElement::kAnchorConnection:
      result += "/linkedAnchors";
      break;
    case FtsElement::kHeader:
      break;
  }

  if (location.subindex != kNoElementIndex) result += "/" + std::to_string(location.subindex);
  return result;
}
constexpr std::int32_t kJsonGridSize = 160;
constexpr std::int16_t kAnchorBlocked = 1 << 3;

json_detail::Json vertexJson(const fts::Vertex& vertex) {
  return {
      {"x", vertex.ssx},
      {"y", vertex.sy},
      {"z", vertex.ssz},
      {"u", vertex.stu},
      {"v", vertex.stv},
  };
}

bool parseVertex(const json_detail::Json& json, fts::Vertex& out) {
  const json_detail::Json* x = json_detail::member(json, "x");
  const json_detail::Json* y = json_detail::member(json, "y");
  const json_detail::Json* z = json_detail::member(json, "z");
  const json_detail::Json* u = json_detail::member(json, "u");
  const json_detail::Json* v = json_detail::member(json, "v");
  return x && y && z && u && v && json_detail::getFloat(*x, out.ssx) && json_detail::getFloat(*y, out.sy) &&
         json_detail::getFloat(*z, out.ssz) && json_detail::getFloat(*u, out.stu) && json_detail::getFloat(*v, out.stv);
}

json_detail::Json polygonJson(const fts::Poly& polygon, std::size_t& lighting_index) {
  json_detail::Json vertices = json_detail::Json::array();
  const std::size_t active_vertices = (polygon.type & kFaceBitQuad) != 0 ? 4U : 3U;
  for (std::size_t index = 0; index < 4; ++index) {
    json_detail::Json vertex = vertexJson(polygon.v[index]);
    if (index < active_vertices) vertex["llfColorIdx"] = lighting_index++;
    vertices.push_back(std::move(vertex));
  }
  json_detail::Json normals = json_detail::Json::array();
  for (const ArxVector3& normal : polygon.nrml) normals.push_back(json_detail::vector(normal));
  return {
      {"vertices", std::move(vertices)},
      {"textureContainerId", polygon.tex},
      {"norm", json_detail::vector(polygon.norm)},
      {"norm2", json_detail::vector(polygon.norm2)},
      {"normals", std::move(normals)},
      {"transval", polygon.transval},
      {"area", polygon.area},
      {"flags", polygon.type},
      {"room", polygon.room},
  };
}

bool parsePolygon(const json_detail::Json& json, fts::Poly& out, std::string_view base,
                  std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* vertices = json_detail::member(json, "vertices");
  const json_detail::Json* texture = json_detail::member(json, "textureContainerId");
  const json_detail::Json* norm = json_detail::member(json, "norm");
  const json_detail::Json* norm2 = json_detail::member(json, "norm2");
  const json_detail::Json* transval = json_detail::member(json, "transval");
  const json_detail::Json* area = json_detail::member(json, "area");
  const json_detail::Json* flags = json_detail::member(json, "flags");
  const json_detail::Json* room = json_detail::member(json, "room");
  if (!vertices || !vertices->is_array() || vertices->size() != 4 || !texture || !norm || !norm2 || !transval ||
      !area || !flags || !room) {
    json_detail::schemaFailure(failure_location, base);
    return false;
  }
  for (std::size_t index = 0; index < 4; ++index) {
    if (!parseVertex((*vertices)[index], out.v[index])) {
      json_detail::schemaFailure(failure_location, std::string(base) + "/vertices/" + std::to_string(index));
      return false;
    }
  }
  if (!json_detail::getSigned(*texture, out.tex) || !json_detail::getVector(*norm, out.norm) ||
      !json_detail::getVector(*norm2, out.norm2) || !json_detail::getFloat(*transval, out.transval) ||
      !json_detail::getFloat(*area, out.area) || !json_detail::getUnsigned(*flags, out.type) ||
      !json_detail::getSigned(*room, out.room)) {
    json_detail::schemaFailure(failure_location, base);
    return false;
  }

  const json_detail::Json* normals = json_detail::member(json, "normals");
  if (!normals) {
    out.nrml[0] = out.norm;
    out.nrml[1] = out.norm;
    out.nrml[2] = out.norm;
    out.nrml[3] = out.norm2;
    return true;
  }
  if (!normals->is_array() || normals->size() != 4) {
    json_detail::schemaFailure(failure_location, std::string(base) + "/normals");
    return false;
  }
  for (std::size_t index = 0; index < 4; ++index) {
    if (!json_detail::getVector((*normals)[index], out.nrml[index])) {
      json_detail::schemaFailure(failure_location, std::string(base) + "/normals/" + std::to_string(index));
      return false;
    }
  }
  return true;
}

bool textureJsonName(const fts::Texture& texture, NativeTextMode text_mode, std::string& result) {
  std::string decoded;
  if (!json_detail::decodeFixed(texture.fic, text_mode, decoded)) return false;
  result = json_detail::lowerSlashes(decoded);
  constexpr std::string_view kPrefix = "graph/obj3d/textures/";
  if (result.starts_with(kPrefix)) result.erase(0, kPrefix.size());
  return true;
}

bool texturePath(std::string_view json_name, NativeTextMode text_mode, fts::Texture& out) {
  std::string path = "graph\\obj3d\\textures\\";
  std::string normalized = json_detail::lowerSlashes(json_name);
  for (char& value : normalized)
    if (value == '/') value = '\\';
  path += normalized;
  return json_detail::encodeFixed(path, text_mode, out.fic);
}

json_detail::Json portalJson(const fts::Portal& portal) {
  json_detail::Json vertices = json_detail::Json::array();
  for (const fts::SavedTextureVertex& vertex : portal.poly.v) {
    vertices.push_back({
        {"position", json_detail::vector(vertex.pos)},
        {"rhw", vertex.rhw},
    });
  }
  return {
      {"polygon",
       {
           {"min", json_detail::vector(portal.poly.min)},
           {"max", json_detail::vector(portal.poly.max)},
           {"norm", json_detail::vector(portal.poly.norm)},
           {"norm2", json_detail::vector(portal.poly.norm2)},
           {"vertices", std::move(vertices)},
           {"center", json_detail::vector(portal.poly.center)},
       }},
      {"room1", portal.room_1},
      {"room2", portal.room_2},
      {"usePortal", portal.useportal},
  };
}

bool parsePortal(const json_detail::Json& json, fts::Portal& out, std::string_view base,
                 std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* polygon = json_detail::member(json, "polygon");
  const json_detail::Json* room1 = json_detail::member(json, "room1");
  const json_detail::Json* room2 = json_detail::member(json, "room2");
  const json_detail::Json* use_portal = json_detail::member(json, "usePortal");
  if (!polygon || !room1 || !room2 || !use_portal || !json_detail::getSigned(*room1, out.room_1) ||
      !json_detail::getSigned(*room2, out.room_2) || !json_detail::getSigned(*use_portal, out.useportal)) {
    json_detail::schemaFailure(failure_location, base);
    return false;
  }

  const json_detail::Json* min = json_detail::member(*polygon, "min");
  const json_detail::Json* max = json_detail::member(*polygon, "max");
  const json_detail::Json* norm = json_detail::member(*polygon, "norm");
  const json_detail::Json* norm2 = json_detail::member(*polygon, "norm2");
  const json_detail::Json* vertices = json_detail::member(*polygon, "vertices");
  const json_detail::Json* center = json_detail::member(*polygon, "center");
  if (!min || !max || !norm || !norm2 || !vertices || !center || !vertices->is_array() || vertices->size() != 4 ||
      !json_detail::getVector(*min, out.poly.min) || !json_detail::getVector(*max, out.poly.max) ||
      !json_detail::getVector(*norm, out.poly.norm) || !json_detail::getVector(*norm2, out.poly.norm2) ||
      !json_detail::getVector(*center, out.poly.center)) {
    json_detail::schemaFailure(failure_location, std::string(base) + "/polygon");
    return false;
  }
  for (std::size_t index = 0; index < 4; ++index) {
    const json_detail::Json* position = json_detail::member((*vertices)[index], "position");
    const json_detail::Json* rhw = json_detail::member((*vertices)[index], "rhw");
    if (!position || !rhw || !json_detail::getVector(*position, out.poly.v[index].pos) ||
        !json_detail::getFloat(*rhw, out.poly.v[index].rhw)) {
      json_detail::schemaFailure(failure_location, std::string(base) + "/polygon/vertices/" + std::to_string(index));
      return false;
    }
  }
  out.poly.type = kFaceBitQuad;
  return true;
}

ArxReturnCode parseHeader(const json_detail::Json& root, fts::Data& out, std::uint32_t& level,
                          std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* header = json_detail::member(root, "header");
  const json_detail::Json* level_index = header ? json_detail::member(*header, "levelIdx") : nullptr;
  const json_detail::Json* scene_position = header ? json_detail::member(*header, "mScenePosition") : nullptr;
  if (!level_index || !scene_position || !json_detail::getUnsigned(*level_index, level) ||
      !json_detail::getVector(*scene_position, out.scene.Mscenepos)) {
    return json_detail::schemaFailure(failure_location, "/header");
  }
  out.scene.version = kFtsVersion;
  out.scene.sizex = kJsonGridSize;
  out.scene.sizez = kJsonGridSize;
  return ARX_OK;
}

ArxReturnCode parseUniqueHeaders(const json_detail::Json& root, NativeTextMode text_mode,
                                 std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* headers = json_detail::member(root, "uniqueHeaders");
  if (!headers) return ARX_OK;
  if (!headers->is_array()) return json_detail::schemaFailure(failure_location, "/uniqueHeaders");
  if (headers->size() > fts_detail::kMaxSourceChecks)
    return json_detail::schemaFailure(failure_location, "/uniqueHeaders", ARX_JSON_LIMIT_EXCEEDED);
  for (std::size_t header_index = 0; header_index < headers->size(); ++header_index) {
    const json_detail::Json& json = (*headers)[header_index];
    const std::string base = json_detail::indexedPointer("/uniqueHeaders", header_index);
    const json_detail::Json* path = json_detail::member(json, "path");
    const json_detail::Json* check = json_detail::member(json, "check");
    std::string path_value;
    char encoded_path[fts_detail::kSourceCheckPathSize] = {};
    if (!path || !check || !json_detail::getString(*path, path_value) ||
        !json_detail::encodeFixed(path_value, text_mode, encoded_path) || !check->is_array() ||
        check->size() != fts_detail::kSourceCheckDataSize) {
      return json_detail::schemaFailure(failure_location, base);
    }
    for (std::size_t index = 0; index < fts_detail::kSourceCheckDataSize; ++index) {
      std::uint8_t value = 0;
      if (!json_detail::getUnsigned((*check)[index], value))
        return json_detail::schemaFailure(failure_location, base + "/check/" + std::to_string(index));
    }
  }
  return ARX_OK;
}

ArxReturnCode parseTextures(const json_detail::Json& root, NativeTextMode text_mode, fts::Data& out,
                            std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* textures = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "textureContainers", textures, kFtsMaxTextures);
      rc != ARX_OK) {
    return json_detail::schemaFailure(failure_location, "/textureContainers", rc);
  }
  for (std::size_t index = 0; index < textures->size(); ++index) {
    const json_detail::Json& json = (*textures)[index];
    const json_detail::Json* id = json_detail::member(json, "id");
    const json_detail::Json* filename = json_detail::member(json, "filename");
    std::int32_t texture_id = 0;
    std::string name;
    fts::Texture texture;
    if (!id || !filename || !json_detail::getSigned(*id, texture_id) || !json_detail::getString(*filename, name) ||
        !texturePath(name, text_mode, texture) || !out.textures.emplace(texture_id, texture).second) {
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/textureContainers", index));
    }
  }
  out.scene.num_textures = static_cast<std::int32_t>(out.textures.size());
  return ARX_OK;
}

ArxReturnCode parseCellAnchors(const json_detail::Json& root, fts::Data& out,
                               std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* cells = nullptr;
  constexpr std::size_t kCellCount = static_cast<std::size_t>(kJsonGridSize) * kJsonGridSize;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "cells", cells, kCellCount); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/cells", rc);
  if (cells->size() != kCellCount) return json_detail::schemaFailure(failure_location, "/cells");
  out.cells.resize(kCellCount);
  for (std::size_t index = 0; index < cells->size(); ++index) {
    const json_detail::Json* anchors = json_detail::member((*cells)[index], "anchors");
    if (!anchors) continue;
    const std::string base = json_detail::indexedPointer("/cells", index) + "/anchors";
    if (!anchors->is_array()) return json_detail::schemaFailure(failure_location, base);
    if (anchors->size() > kFtsMaxAnchors)
      return json_detail::schemaFailure(failure_location, base, ARX_JSON_LIMIT_EXCEEDED);
    out.cells[index].anchor_ids.reserve(anchors->size());
    for (std::size_t anchor_index = 0; anchor_index < anchors->size(); ++anchor_index) {
      std::int32_t anchor = 0;
      if (!json_detail::getSigned((*anchors)[anchor_index], anchor))
        return json_detail::schemaFailure(failure_location, base + "/" + std::to_string(anchor_index));
      out.cells[index].anchor_ids.push_back(anchor);
    }
  }
  return ARX_OK;
}

ArxReturnCode parsePolygons(const json_detail::Json& root, std::vector<fts::Poly>& out,
                            std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* polygons = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "polygons", polygons, kFtsMaxPolygons); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/polygons", rc);
  out.reserve(polygons->size());
  for (std::size_t index = 0; index < polygons->size(); ++index) {
    fts::Poly polygon;
    const std::string base = json_detail::indexedPointer("/polygons", index);
    if (!parsePolygon((*polygons)[index], polygon, base, failure_location)) return ARX_JSON_BAD_SCHEMA;
    out.push_back(polygon);
  }
  return ARX_OK;
}

ArxReturnCode parseAnchors(const json_detail::Json& root, fts::Data& out,
                           std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* anchors = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "anchors", anchors, kFtsMaxAnchors); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/anchors", rc);
  out.anchors.reserve(anchors->size());
  for (std::size_t anchor_index = 0; anchor_index < anchors->size(); ++anchor_index) {
    const json_detail::Json& json = (*anchors)[anchor_index];
    const std::string base = json_detail::indexedPointer("/anchors", anchor_index);
    const json_detail::Json* data = json_detail::member(json, "data");
    const json_detail::Json* links = json_detail::member(json, "linkedAnchors");
    const json_detail::Json* position = data ? json_detail::member(*data, "position") : nullptr;
    const json_detail::Json* radius = data ? json_detail::member(*data, "radius") : nullptr;
    const json_detail::Json* height = data ? json_detail::member(*data, "height") : nullptr;
    const json_detail::Json* blocked = data ? json_detail::member(*data, "isBlocked") : nullptr;
    bool is_blocked = false;
    fts::Anchor anchor;
    if (!position || !radius || !height || !blocked || !links || !links->is_array() ||
        !json_detail::getVector(*position, anchor.data.pos) || !json_detail::getFloat(*radius, anchor.data.radius) ||
        !json_detail::getFloat(*height, anchor.data.height) || !json_detail::getBool(*blocked, is_blocked) ||
        links->size() > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max())) {
      return json_detail::schemaFailure(failure_location, base);
    }
    anchor.data.flags = is_blocked ? kAnchorBlocked : 0;
    anchor.linked.reserve(links->size());
    for (std::size_t link_index = 0; link_index < links->size(); ++link_index) {
      std::int32_t index = 0;
      if (!json_detail::getSigned((*links)[link_index], index))
        return json_detail::schemaFailure(failure_location, base + "/linkedAnchors/" + std::to_string(link_index));
      anchor.linked.push_back(index);
    }
    anchor.data.num_linked = static_cast<std::int16_t>(anchor.linked.size());
    out.anchors.push_back(std::move(anchor));
  }
  out.scene.num_anchors = static_cast<std::int32_t>(out.anchors.size());
  return ARX_OK;
}

ArxReturnCode parsePortals(const json_detail::Json& root, fts::Data& out,
                           std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* portals = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "portals", portals, kFtsMaxPortals); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/portals", rc);
  out.portals.reserve(portals->size());
  for (std::size_t index = 0; index < portals->size(); ++index) {
    fts::Portal portal;
    const std::string base = json_detail::indexedPointer("/portals", index);
    if (!parsePortal((*portals)[index], portal, base, failure_location)) return ARX_JSON_BAD_SCHEMA;
    out.portals.push_back(portal);
  }
  out.scene.num_portals = static_cast<std::int32_t>(out.portals.size());
  return ARX_OK;
}

ArxReturnCode parseRooms(const json_detail::Json& root, fts::Data& out, std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* rooms = nullptr;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "rooms", rooms, kFtsMaxRooms + 1); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/rooms", rc);
  if (rooms->empty()) return json_detail::schemaFailure(failure_location, "/rooms");
  out.rooms.reserve(rooms->size());
  for (std::size_t room_index = 0; room_index < rooms->size(); ++room_index) {
    const json_detail::Json& json = (*rooms)[room_index];
    const std::string base = json_detail::indexedPointer("/rooms", room_index);
    const json_detail::Json* portals = json_detail::member(json, "portals");
    const json_detail::Json* polygons = json_detail::member(json, "polygons");
    fts::Room room;
    if (!portals || !portals->is_array() || !polygons || !polygons->is_array() || portals->size() > kFtsMaxPortals ||
        polygons->size() > kFtsMaxPolygons) {
      return json_detail::schemaFailure(failure_location, base);
    }
    room.portal_ids.reserve(portals->size());
    for (std::size_t portal_index = 0; portal_index < portals->size(); ++portal_index) {
      std::int32_t index = 0;
      if (!json_detail::getSigned((*portals)[portal_index], index))
        return json_detail::schemaFailure(failure_location, base + "/portals/" + std::to_string(portal_index));
      room.portal_ids.push_back(index);
    }
    room.polygons.reserve(polygons->size());
    for (std::size_t polygon_index = 0; polygon_index < polygons->size(); ++polygon_index) {
      const json_detail::Json& item = (*polygons)[polygon_index];
      const json_detail::Json* x = json_detail::member(item, "cellX");
      const json_detail::Json* y = json_detail::member(item, "cellY");
      const json_detail::Json* index = json_detail::member(item, "polygonIdx");
      fts::EpData polygon;
      if (!x || !y || !index || !json_detail::getSigned(*x, polygon.px) || !json_detail::getSigned(*y, polygon.py) ||
          !json_detail::getSigned(*index, polygon.idx)) {
        return json_detail::schemaFailure(failure_location, base + "/polygons/" + std::to_string(polygon_index));
      }
      room.polygons.push_back(polygon);
    }
    room.data.num_portals = static_cast<std::int32_t>(room.portal_ids.size());
    room.data.num_polys = static_cast<std::int32_t>(room.polygons.size());
    out.rooms.push_back(std::move(room));
  }
  out.scene.num_rooms = static_cast<std::int32_t>(out.rooms.size() - 1);
  return ARX_OK;
}

ArxReturnCode parseRoomDistances(const json_detail::Json& root, fts::Data& out,
                                 std::optional<JsonLocation>& failure_location) {
  const json_detail::Json* distances = nullptr;
  const std::size_t room_count = out.rooms.size();
  const std::size_t expected = room_count * room_count;
  if (const ArxReturnCode rc = json_detail::arrayMember(root, "roomDistances", distances, expected); rc != ARX_OK)
    return json_detail::schemaFailure(failure_location, "/roomDistances", rc);
  if (distances->size() != expected) return json_detail::schemaFailure(failure_location, "/roomDistances");
  out.room_distances.reserve(expected);
  for (std::size_t index = 0; index < distances->size(); ++index) {
    const json_detail::Json& json = (*distances)[index];
    const json_detail::Json* distance = json_detail::member(json, "distance");
    const json_detail::Json* start = json_detail::member(json, "startPosition");
    const json_detail::Json* end = json_detail::member(json, "endPosition");
    fts::RoomDistData value;
    if (!distance || !start || !end || !json_detail::getFloat(*distance, value.distance) ||
        !json_detail::getVector(*start, value.startpos) || !json_detail::getVector(*end, value.endpos)) {
      return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/roomDistances", index));
    }
    out.room_distances.push_back(value);
  }
  return ARX_OK;
}

struct PolygonLocation {
  std::size_t cell = 0;
  std::size_t index = 0;
  std::size_t room = 0;
  std::size_t room_entry = 0;
};

std::optional<std::size_t> centroidCell(const fts::Poly& polygon) {
  const float x = (polygon.v[0].ssx + polygon.v[1].ssx + polygon.v[2].ssx) / 3.0f;
  const float z = (polygon.v[0].ssz + polygon.v[1].ssz + polygon.v[2].ssz) / 3.0f;
  const auto cell_x = static_cast<std::int32_t>(std::floor(x / 100.0f));
  const auto cell_z = static_cast<std::int32_t>(std::floor(z / 100.0f));
  if (cell_x < 0 || cell_x >= kJsonGridSize || cell_z < 0 || cell_z >= kJsonGridSize) return std::nullopt;
  return static_cast<std::size_t>(cell_z) * kJsonGridSize + static_cast<std::size_t>(cell_x);
}

ArxReturnCode placePolygons(std::vector<fts::Poly>& polygons, fts::Data& out,
                            std::optional<JsonLocation>& failure_location) {
  constexpr std::size_t kUnassigned = std::numeric_limits<std::size_t>::max();
  std::vector<PolygonLocation> locations(polygons.size(), {kUnassigned, kUnassigned, kUnassigned, kUnassigned});

  auto fail_placement = [&](std::size_t polygon) {
    const PolygonLocation& location = locations[polygon];
    if (location.room != kUnassigned) {
      return json_detail::schemaFailure(
          failure_location,
          json_detail::indexedPointer("/rooms", location.room) + "/polygons/" + std::to_string(location.room_entry));
    }
    return json_detail::schemaFailure(failure_location, json_detail::indexedPointer("/polygons", polygon));
  };

  std::vector<std::size_t> room_offsets(out.rooms.size() + 1U, 0);
  for (const fts::Poly& polygon : polygons) {
    if (polygon.room < 0 || static_cast<std::size_t>(polygon.room) >= out.rooms.size()) continue;
    ++room_offsets[static_cast<std::size_t>(polygon.room) + 1U];
  }
  for (std::size_t room = 0; room < out.rooms.size(); ++room) room_offsets[room + 1U] += room_offsets[room];

  std::vector<std::size_t> room_polygons(room_offsets.back());
  std::vector<std::size_t> room_cursors = room_offsets;
  for (std::size_t polygon = 0; polygon < polygons.size(); ++polygon) {
    const std::int16_t room = polygons[polygon].room;
    if (room < 0 || static_cast<std::size_t>(room) >= out.rooms.size()) continue;
    room_polygons[room_cursors[static_cast<std::size_t>(room)]++] = polygon;
  }

  std::size_t max_room_references = 0;
  for (const fts::Room& room : out.rooms) max_room_references = std::max(max_room_references, room.polygons.size());
  struct RoomReference {
    fts::EpData value;
    std::size_t source_index = 0;
  };
  std::vector<RoomReference> references;
  references.reserve(max_room_references);
  for (std::size_t room = 0; room < out.rooms.size(); ++room) {
    const std::size_t begin = room_offsets[room];
    const std::size_t count = room_offsets[room + 1U] - begin;
    if (count != out.rooms[room].polygons.size()) continue;

    references.clear();
    for (std::size_t index = 0; index < out.rooms[room].polygons.size(); ++index)
      references.push_back({out.rooms[room].polygons[index], index});
    std::sort(references.begin(), references.end(), [](const RoomReference& left, const RoomReference& right) {
      if (left.value.py != right.value.py) return left.value.py < right.value.py;
      if (left.value.px != right.value.px) return left.value.px < right.value.px;
      return left.value.idx < right.value.idx;
    });
    for (std::size_t ordinal = 0; ordinal < count; ++ordinal) {
      const RoomReference& reference = references[ordinal];
      const fts::EpData& value = reference.value;
      if (value.px < 0 || value.px >= kJsonGridSize || value.py < 0 || value.py >= kJsonGridSize || value.idx < 0) {
        return json_detail::schemaFailure(
            failure_location,
            json_detail::indexedPointer("/rooms", room) + "/polygons/" + std::to_string(reference.source_index));
      }
      locations[room_polygons[begin + ordinal]] = {
          static_cast<std::size_t>(value.py) * kJsonGridSize + static_cast<std::size_t>(value.px),
          static_cast<std::size_t>(value.idx),
          room,
          reference.source_index};
    }
  }

  std::vector<std::size_t> cell_offsets(out.cells.size() + 1U, 0);
  for (std::size_t polygon = 0; polygon < polygons.size(); ++polygon) {
    PolygonLocation& location = locations[polygon];
    if (location.cell == kUnassigned) {
      const std::optional<std::size_t> cell = centroidCell(polygons[polygon]);
      if (!cell) return fail_placement(polygon);
      location.cell = *cell;
    }
    if (location.cell >= out.cells.size()) return fail_placement(polygon);
    ++cell_offsets[location.cell + 1U];
  }
  for (std::size_t cell = 0; cell < out.cells.size(); ++cell) cell_offsets[cell + 1U] += cell_offsets[cell];

  struct PlacedPolygon {
    std::size_t source = 0;
    std::size_t index = kUnassigned;
  };
  std::vector<PlacedPolygon> placed(polygons.size());
  std::vector<std::size_t> cell_cursors = cell_offsets;
  for (std::size_t polygon = 0; polygon < polygons.size(); ++polygon) {
    const PolygonLocation& location = locations[polygon];
    placed[cell_cursors[location.cell]++] = {polygon, location.index};
  }

  std::vector<std::size_t> slots;
  for (std::size_t cell = 0; cell < out.cells.size(); ++cell) {
    const std::size_t begin = cell_offsets[cell];
    const std::size_t count = cell_offsets[cell + 1U] - begin;
    slots.assign(count, kUnassigned);
    for (std::size_t offset = 0; offset < count; ++offset) {
      const PlacedPolygon& polygon = placed[begin + offset];
      if (polygon.index == kUnassigned) continue;
      if (polygon.index >= slots.size() || slots[polygon.index] != kUnassigned) return fail_placement(polygon.source);
      slots[polygon.index] = polygon.source;
    }
    std::size_t next = 0;
    for (std::size_t offset = 0; offset < count; ++offset) {
      const PlacedPolygon& polygon = placed[begin + offset];
      if (polygon.index != kUnassigned) continue;
      while (next < slots.size() && slots[next] != kUnassigned) ++next;
      if (next == slots.size()) return fail_placement(polygon.source);
      slots[next] = polygon.source;
    }
    out.cells[cell].polygons.reserve(count);
    for (std::size_t source : slots) {
      if (source == kUnassigned) return json_detail::schemaFailure(failure_location, "/polygons");
      out.cells[cell].polygons.push_back(polygons[source]);
    }
  }
  out.scene.num_polys = static_cast<std::int32_t>(polygons.size());
  return ARX_OK;
}

ArxReturnCode importFts(std::string_view text, NativeTextMode text_mode, fts::Data& out, std::uint32_t& level,
                        std::optional<JsonLocation>& failure_location) {
  json_detail::Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root, failure_location));
  if (!root.is_object()) return json_detail::schemaFailure(failure_location, {});
  if (!json_detail::validSchema(root, kFtsSchema)) return json_detail::schemaFailure(failure_location, "/$schema");
  ArxReturnCode code = parseHeader(root, out, level, failure_location);
  if (code != ARX_OK) return code;
  code = parseUniqueHeaders(root, text_mode, failure_location);
  if (code != ARX_OK) return code;
  code = parseTextures(root, text_mode, out, failure_location);
  if (code != ARX_OK) return code;
  code = parseCellAnchors(root, out, failure_location);
  if (code != ARX_OK) return code;
  std::vector<fts::Poly> polygons;
  code = parsePolygons(root, polygons, failure_location);
  if (code != ARX_OK) return code;
  code = parseAnchors(root, out, failure_location);
  if (code != ARX_OK) return code;
  code = parsePortals(root, out, failure_location);
  if (code != ARX_OK) return code;
  code = parseRooms(root, out, failure_location);
  if (code != ARX_OK) return code;
  code = parseRoomDistances(root, out, failure_location);
  if (code != ARX_OK) return code;
  code = placePolygons(polygons, out, failure_location);
  if (code != ARX_OK) return json_detail::schemaFailureIfUnknown(failure_location, "/polygons", code);
  FtsLocation native_location;
  if (const ArxReturnCode rc = canonicalizeFts(&out, &native_location); rc != ARX_OK)
    return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
  const ArxReturnCode rc = validateFts(&out, &native_location);
  return json_detail::schemaFailureIfUnknown(failure_location, jsonPointer(native_location), rc);
}

}  // namespace

FtsResult<std::string> exportFtsToJson(const fts::Data& data, std::uint32_t level, bool pretty,
                                       NativeTextMode text_mode) {
  std::string output;
  std::optional<FtsLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    FtsLocation validation_location;
    if (const ArxReturnCode rc = validateFts(&data, &validation_location); rc != ARX_OK) {
      location = std::move(validation_location);
      return rc;
    }
    if (data.scene.sizex != kJsonGridSize) {
      location = FtsLocation{.element = FtsElement::kHeader, .field = "scene.sizex"};
      return ARX_JSON_BAD_SCHEMA;
    }
    if (data.scene.sizez != kJsonGridSize) {
      location = FtsLocation{.element = FtsElement::kHeader, .field = "scene.sizez"};
      return ARX_JSON_BAD_SCHEMA;
    }
    json_detail::Json root;
    root["$schema"] = kFtsSchema;
    root["header"] = {
        {"levelIdx", level},
        {"mScenePosition", json_detail::vector(data.scene.Mscenepos)},
    };

    root["uniqueHeaders"] = json_detail::Json::array();
    std::string decoded;

    std::vector<std::pair<std::int32_t, const fts::Texture*>> textures;
    textures.reserve(data.textures.size());
    for (const auto& [id, texture] : data.textures) textures.emplace_back(id, &texture);
    std::sort(
        textures.begin(), textures.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
    root["textureContainers"] = json_detail::Json::array();
    for (std::size_t texture_index = 0; texture_index < textures.size(); ++texture_index) {
      const auto& [id, texture] = textures[texture_index];
      if (!textureJsonName(*texture, text_mode, decoded)) {
        location = FtsLocation{.element = FtsElement::kTexture, .index = texture_index, .field = "fic"};
        return ARX_FTS_BAD_TEXTURE_PATH;
      }
      root["textureContainers"].push_back({{"id", id}, {"filename", decoded}});
    }

    root["cells"] = json_detail::Json::array();
    for (const fts::Cell& cell : data.cells) {
      json_detail::Json json = json_detail::Json::object();
      if (!cell.anchor_ids.empty()) json["anchors"] = cell.anchor_ids;
      root["cells"].push_back(std::move(json));
    }

    root["polygons"] = json_detail::Json::array();
    std::size_t lighting_index = 0;
    std::size_t polygon_index = 0;
    for (const fts::Cell& cell : data.cells) {
      for (const fts::Poly& polygon : cell.polygons) {
        for (std::size_t vertex_index = 0; vertex_index < 4; ++vertex_index) {
          const fts::Vertex& vertex = polygon.v[vertex_index];
          if (!std::isfinite(vertex.ssx) || !std::isfinite(vertex.sy) || !std::isfinite(vertex.ssz) ||
              !std::isfinite(vertex.stu) || !std::isfinite(vertex.stv)) {
            location = FtsLocation{
                .element = FtsElement::kFace, .index = polygon_index, .subindex = vertex_index, .field = "v"};
            return ARX_JSON_UNREPRESENTABLE_VALUE;
          }
          if (!math::finite(polygon.nrml[vertex_index])) {
            location = FtsLocation{
                .element = FtsElement::kFace, .index = polygon_index, .subindex = vertex_index, .field = "nrml"};
            return ARX_JSON_UNREPRESENTABLE_VALUE;
          }
        }
        if (!math::finite(polygon.norm) || !math::finite(polygon.norm2) || !std::isfinite(polygon.transval) ||
            !std::isfinite(polygon.area)) {
          std::string field = !math::finite(polygon.norm)    ? "norm"
                              : !math::finite(polygon.norm2) ? "norm2"
                              : !std::isfinite(polygon.area) ? "area"
                                                             : "transval";
          location = FtsLocation{.element = FtsElement::kFace, .index = polygon_index, .field = std::move(field)};
          return ARX_JSON_UNREPRESENTABLE_VALUE;
        }
        root["polygons"].push_back(polygonJson(polygon, lighting_index));
        ++polygon_index;
      }
    }

    root["anchors"] = json_detail::Json::array();
    for (std::size_t anchor_index = 0; anchor_index < data.anchors.size(); ++anchor_index) {
      const fts::Anchor& anchor = data.anchors[anchor_index];
      if (!std::isfinite(anchor.data.radius) || !std::isfinite(anchor.data.height)) {
        location = FtsLocation{.element = FtsElement::kAnchor,
                               .index = anchor_index,
                               .field = std::isfinite(anchor.data.radius) ? "data.height" : "data.radius"};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      root["anchors"].push_back({
          {"data",
           {
               {"position", json_detail::vector(anchor.data.pos)},
               {"radius", anchor.data.radius},
               {"height", anchor.data.height},
               {"isBlocked", (anchor.data.flags & kAnchorBlocked) != 0},
           }},
          {"linkedAnchors", anchor.linked},
      });
    }

    root["portals"] = json_detail::Json::array();
    for (std::size_t portal_index = 0; portal_index < data.portals.size(); ++portal_index) {
      const fts::Portal& portal = data.portals[portal_index];
      const fts::SavePoly& polygon = portal.poly;
      if (!math::finite(polygon.min) || !math::finite(polygon.max) || !math::finite(polygon.norm) ||
          !math::finite(polygon.norm2) || !math::finite(polygon.center)) {
        std::string field = !math::finite(polygon.min)     ? "poly.min"
                            : !math::finite(polygon.max)   ? "poly.max"
                            : !math::finite(polygon.norm)  ? "poly.norm"
                            : !math::finite(polygon.norm2) ? "poly.norm2"
                                                           : "poly.center";
        location = FtsLocation{.element = FtsElement::kPortal, .index = portal_index, .field = std::move(field)};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      for (std::size_t vertex_index = 0; vertex_index < 4; ++vertex_index) {
        if (!math::finite(polygon.v[vertex_index].pos) || !std::isfinite(polygon.v[vertex_index].rhw)) {
          location = FtsLocation{
              .element = FtsElement::kPortal, .index = portal_index, .subindex = vertex_index, .field = "poly.v"};
          return ARX_JSON_UNREPRESENTABLE_VALUE;
        }
      }
      root["portals"].push_back(portalJson(portal));
    }

    root["rooms"] = json_detail::Json::array();
    for (const fts::Room& room : data.rooms) {
      json_detail::Json polygons = json_detail::Json::array();
      for (const fts::EpData& polygon : room.polygons) {
        polygons.push_back({
            {"cellX", polygon.px},
            {"cellY", polygon.py},
            {"polygonIdx", polygon.idx},
        });
      }
      root["rooms"].push_back({
          {"portals", room.portal_ids},
          {"polygons", std::move(polygons)},
      });
    }

    root["roomDistances"] = json_detail::Json::array();
    for (std::size_t distance_index = 0; distance_index < data.room_distances.size(); ++distance_index) {
      const fts::RoomDistData& distance = data.room_distances[distance_index];
      if (!math::finite(distance.startpos) || !math::finite(distance.endpos)) {
        location = FtsLocation{.element = FtsElement::kRoomDistance,
                               .index = distance_index,
                               .field = math::finite(distance.startpos) ? "endpos" : "startpos"};
        return ARX_JSON_UNREPRESENTABLE_VALUE;
      }
      root["roomDistances"].push_back({
          {"distance", distance.distance},
          {"startPosition", json_detail::vector(distance.startpos)},
          {"endPosition", json_detail::vector(distance.endpos)},
      });
    }
    return json_detail::dump(root, pretty, output);
  });
  if (code != ARX_OK) return api_detail::ftsFailure<std::string>(code, std::move(location));
  return FtsResult<std::string>::success(std::move(output));
}

JsonResult<FtsJsonImport> importJsonToFts(std::string_view text, NativeTextMode text_mode) {
  FtsJsonImport result;
  std::optional<JsonLocation> location;
  const ArxReturnCode code = json_detail::guarded([&]() -> ArxReturnCode {
    if (!native_text::validMode(text_mode)) return ARX_INVALID_OPTIONS;
    return importFts(text, text_mode, result.fts, result.level, location);
  });
  if (code != ARX_OK) return api_detail::jsonFailure<FtsJsonImport>(code, std::move(location));
  return JsonResult<FtsJsonImport>::success(std::move(result));
}

}  // namespace pistoris
