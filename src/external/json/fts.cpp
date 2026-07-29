// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/fts.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "external/json.h"
#include "external/json/native_common.h"
#include "utils/parse_utils.h"

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

bool parsePolygon(const json_detail::Json& json, fts::Poly& out) {
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
    return false;
  }
  for (std::size_t index = 0; index < 4; ++index)
    if (!parseVertex((*vertices)[index], out.v[index])) return false;
  if (!json_detail::getSigned(*texture, out.tex) || !json_detail::getVector(*norm, out.norm) ||
      !json_detail::getVector(*norm2, out.norm2) || !json_detail::getFloat(*transval, out.transval) ||
      !json_detail::getFloat(*area, out.area) || !json_detail::getUnsigned(*flags, out.type) ||
      !json_detail::getSigned(*room, out.room)) {
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
  if (!normals->is_array() || normals->size() != 4) return false;
  for (std::size_t index = 0; index < 4; ++index)
    if (!json_detail::getVector((*normals)[index], out.nrml[index])) return false;
  return true;
}

std::string textureJsonName(const fts::Texture& texture) {
  std::string result = json_detail::lowerSlashes(json_detail::fixedString(texture.fic));
  constexpr std::string_view kPrefix = "graph/obj3d/textures/";
  if (result.starts_with(kPrefix)) result.erase(0, kPrefix.size());
  return result;
}

bool texturePath(std::string_view json_name, fts::Texture& out) {
  std::string path = "graph\\obj3d\\textures\\";
  std::string normalized = json_detail::lowerSlashes(json_name);
  for (char& value : normalized)
    if (value == '/') value = '\\';
  path += normalized;
  return json_detail::copyFixed(path, out.fic);
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

bool parsePortal(const json_detail::Json& json, fts::Portal& out) {
  const json_detail::Json* polygon = json_detail::member(json, "polygon");
  const json_detail::Json* room1 = json_detail::member(json, "room1");
  const json_detail::Json* room2 = json_detail::member(json, "room2");
  const json_detail::Json* use_portal = json_detail::member(json, "usePortal");
  if (!polygon || !room1 || !room2 || !use_portal || !json_detail::getSigned(*room1, out.room_1) ||
      !json_detail::getSigned(*room2, out.room_2) || !json_detail::getSigned(*use_portal, out.useportal)) {
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
    return false;
  }
  for (std::size_t index = 0; index < 4; ++index) {
    const json_detail::Json* position = json_detail::member((*vertices)[index], "position");
    const json_detail::Json* rhw = json_detail::member((*vertices)[index], "rhw");
    if (!position || !rhw || !json_detail::getVector(*position, out.poly.v[index].pos) ||
        !json_detail::getFloat(*rhw, out.poly.v[index].rhw)) {
      return false;
    }
  }
  out.poly.type = kFaceBitQuad;
  return true;
}

ArxReturnCode parseHeader(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* header = json_detail::member(root, "header");
  const json_detail::Json* level_index = header ? json_detail::member(*header, "levelIdx") : nullptr;
  const json_detail::Json* scene_position = header ? json_detail::member(*header, "mScenePosition") : nullptr;
  std::uint32_t level = 0;
  if (!level_index || !scene_position || !json_detail::getUnsigned(*level_index, level) ||
      !json_detail::getVector(*scene_position, out.scene.Mscenepos)) {
    return ARX_JSON_BAD_SCHEMA;
  }
  if (!json_detail::copyFixed(paths::levelFts(level), out.header.path)) return ARX_JSON_BAD_SCHEMA;
  out.header.version = kFtsVersion;
  out.scene.version = kFtsVersion;
  out.scene.sizex = kJsonGridSize;
  out.scene.sizez = kJsonGridSize;
  return ARX_OK;
}

ArxReturnCode parseUniqueHeaders(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* headers = json_detail::member(root, "uniqueHeaders");
  if (!headers) return ARX_OK;
  if (!headers->is_array()) return ARX_JSON_BAD_SCHEMA;
  if (headers->size() > kFtsMaxHeaderBlocks) return ARX_JSON_LIMIT_EXCEEDED;
  out.unique_headers.reserve(headers->size());
  for (const json_detail::Json& json : *headers) {
    const json_detail::Json* path = json_detail::member(json, "path");
    const json_detail::Json* check = json_detail::member(json, "check");
    std::string path_value;
    fts::UniqueHeader3 header;
    if (!path || !check || !json_detail::getString(*path, path_value) ||
        !json_detail::copyFixed(path_value, header.path) || !check->is_array() ||
        check->size() != sizeof(header.check)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    for (std::size_t index = 0; index < sizeof(header.check); ++index) {
      std::uint8_t value = 0;
      if (!json_detail::getUnsigned((*check)[index], value)) return ARX_JSON_BAD_SCHEMA;
      header.check[index] = static_cast<char>(value);
    }
    out.unique_headers.push_back(header);
  }
  out.header.count = static_cast<std::int32_t>(out.unique_headers.size());
  return ARX_OK;
}

ArxReturnCode parseTextures(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* textures = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "textureContainers", textures, kFtsMaxTextures));
  for (const json_detail::Json& json : *textures) {
    const json_detail::Json* id = json_detail::member(json, "id");
    const json_detail::Json* filename = json_detail::member(json, "filename");
    std::int32_t texture_id = 0;
    std::string name;
    fts::Texture texture;
    if (!id || !filename || !json_detail::getSigned(*id, texture_id) || !json_detail::getString(*filename, name) ||
        !texturePath(name, texture) || !out.textures.emplace(texture_id, texture).second) {
      return ARX_JSON_BAD_SCHEMA;
    }
  }
  out.scene.num_textures = static_cast<std::int32_t>(out.textures.size());
  return ARX_OK;
}

ArxReturnCode parseCellAnchors(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* cells = nullptr;
  constexpr std::size_t kCellCount = static_cast<std::size_t>(kJsonGridSize) * kJsonGridSize;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "cells", cells, kCellCount));
  if (cells->size() != kCellCount) return ARX_JSON_BAD_SCHEMA;
  out.cells.resize(kCellCount);
  for (std::size_t index = 0; index < cells->size(); ++index) {
    const json_detail::Json* anchors = json_detail::member((*cells)[index], "anchors");
    if (!anchors) continue;
    if (!anchors->is_array() || anchors->size() > kFtsMaxAnchors) return ARX_JSON_BAD_SCHEMA;
    out.cells[index].anchor_ids.reserve(anchors->size());
    for (const json_detail::Json& item : *anchors) {
      std::int32_t anchor = 0;
      if (!json_detail::getSigned(item, anchor)) return ARX_JSON_BAD_SCHEMA;
      out.cells[index].anchor_ids.push_back(anchor);
    }
  }
  return ARX_OK;
}

ArxReturnCode parsePolygons(const json_detail::Json& root, std::vector<fts::Poly>& out) {
  const json_detail::Json* polygons = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "polygons", polygons, kFtsMaxPolygons));
  out.reserve(polygons->size());
  for (const json_detail::Json& json : *polygons) {
    fts::Poly polygon;
    if (!parsePolygon(json, polygon)) return ARX_JSON_BAD_SCHEMA;
    out.push_back(polygon);
  }
  return ARX_OK;
}

ArxReturnCode parseAnchors(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* anchors = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "anchors", anchors, kFtsMaxAnchors));
  out.anchors.reserve(anchors->size());
  for (const json_detail::Json& json : *anchors) {
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
      return ARX_JSON_BAD_SCHEMA;
    }
    anchor.data.flags = is_blocked ? kAnchorBlocked : 0;
    anchor.linked.reserve(links->size());
    for (const json_detail::Json& link : *links) {
      std::int32_t index = 0;
      if (!json_detail::getSigned(link, index)) return ARX_JSON_BAD_SCHEMA;
      anchor.linked.push_back(index);
    }
    anchor.data.num_linked = static_cast<std::int16_t>(anchor.linked.size());
    out.anchors.push_back(std::move(anchor));
  }
  out.scene.num_anchors = static_cast<std::int32_t>(out.anchors.size());
  return ARX_OK;
}

ArxReturnCode parsePortals(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* portals = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "portals", portals, kFtsMaxPortals));
  out.portals.reserve(portals->size());
  for (const json_detail::Json& json : *portals) {
    fts::Portal portal;
    if (!parsePortal(json, portal)) return ARX_JSON_BAD_SCHEMA;
    out.portals.push_back(portal);
  }
  out.scene.num_portals = static_cast<std::int32_t>(out.portals.size());
  return ARX_OK;
}

ArxReturnCode parseRooms(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* rooms = nullptr;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "rooms", rooms, kFtsMaxRooms + 1));
  if (rooms->empty()) return ARX_JSON_BAD_SCHEMA;
  out.rooms.reserve(rooms->size());
  for (const json_detail::Json& json : *rooms) {
    const json_detail::Json* portals = json_detail::member(json, "portals");
    const json_detail::Json* polygons = json_detail::member(json, "polygons");
    fts::Room room;
    if (!portals || !portals->is_array() || !polygons || !polygons->is_array() || portals->size() > kFtsMaxPortals ||
        polygons->size() > kFtsMaxPolygons) {
      return ARX_JSON_BAD_SCHEMA;
    }
    room.portal_ids.reserve(portals->size());
    for (const json_detail::Json& item : *portals) {
      std::int32_t index = 0;
      if (!json_detail::getSigned(item, index)) return ARX_JSON_BAD_SCHEMA;
      room.portal_ids.push_back(index);
    }
    room.polygons.reserve(polygons->size());
    for (const json_detail::Json& item : *polygons) {
      const json_detail::Json* x = json_detail::member(item, "cellX");
      const json_detail::Json* y = json_detail::member(item, "cellY");
      const json_detail::Json* index = json_detail::member(item, "polygonIdx");
      fts::EpData polygon;
      if (!x || !y || !index || !json_detail::getSigned(*x, polygon.px) || !json_detail::getSigned(*y, polygon.py) ||
          !json_detail::getSigned(*index, polygon.idx)) {
        return ARX_JSON_BAD_SCHEMA;
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

ArxReturnCode parseRoomDistances(const json_detail::Json& root, fts::Data& out) {
  const json_detail::Json* distances = nullptr;
  const std::size_t room_count = out.rooms.size();
  const std::size_t expected = room_count * room_count;
  ARX_RETURN_IF_ERR(json_detail::arrayMember(root, "roomDistances", distances, expected));
  if (distances->size() != expected) return ARX_JSON_BAD_SCHEMA;
  out.room_distances.reserve(expected);
  for (const json_detail::Json& json : *distances) {
    const json_detail::Json* distance = json_detail::member(json, "distance");
    const json_detail::Json* start = json_detail::member(json, "startPosition");
    const json_detail::Json* end = json_detail::member(json, "endPosition");
    fts::RoomDistData value;
    if (!distance || !start || !end || !json_detail::getFloat(*distance, value.distance) ||
        !json_detail::getVector(*start, value.startpos) || !json_detail::getVector(*end, value.endpos)) {
      return ARX_JSON_BAD_SCHEMA;
    }
    out.room_distances.push_back(value);
  }
  return ARX_OK;
}

struct PolygonLocation {
  std::size_t cell = 0;
  std::size_t index = 0;
};

std::optional<std::size_t> centroidCell(const fts::Poly& polygon) {
  const float x = (polygon.v[0].ssx + polygon.v[1].ssx + polygon.v[2].ssx) / 3.0f;
  const float z = (polygon.v[0].ssz + polygon.v[1].ssz + polygon.v[2].ssz) / 3.0f;
  const auto cell_x = static_cast<std::int32_t>(std::floor(x / 100.0f));
  const auto cell_z = static_cast<std::int32_t>(std::floor(z / 100.0f));
  if (cell_x < 0 || cell_x >= kJsonGridSize || cell_z < 0 || cell_z >= kJsonGridSize) return std::nullopt;
  return static_cast<std::size_t>(cell_z) * kJsonGridSize + static_cast<std::size_t>(cell_x);
}

ArxReturnCode placePolygons(std::vector<fts::Poly>& polygons, fts::Data& out) {
  std::vector<std::optional<PolygonLocation>> locations(polygons.size());
  for (std::size_t room_index = 0; room_index < out.rooms.size(); ++room_index) {
    std::vector<std::size_t> polygon_indices;
    for (std::size_t index = 0; index < polygons.size(); ++index)
      if (polygons[index].room == static_cast<std::int16_t>(room_index)) polygon_indices.push_back(index);

    std::vector<fts::EpData> references = out.rooms[room_index].polygons;
    if (polygon_indices.size() != references.size()) continue;
    std::sort(references.begin(), references.end(), [](const fts::EpData& left, const fts::EpData& right) {
      if (left.py != right.py) return left.py < right.py;
      if (left.px != right.px) return left.px < right.px;
      return left.idx < right.idx;
    });
    for (std::size_t index = 0; index < polygon_indices.size(); ++index) {
      const fts::EpData& reference = references[index];
      if (reference.px < 0 || reference.px >= kJsonGridSize || reference.py < 0 || reference.py >= kJsonGridSize ||
          reference.idx < 0) {
        return ARX_JSON_BAD_SCHEMA;
      }
      locations[polygon_indices[index]] = PolygonLocation{
          static_cast<std::size_t>(reference.py) * kJsonGridSize + static_cast<std::size_t>(reference.px),
          static_cast<std::size_t>(reference.idx)};
    }
  }

  struct PlacedPolygon {
    std::optional<std::size_t> index;
    fts::Poly polygon;
  };
  std::vector<std::vector<PlacedPolygon>> placed(out.cells.size());
  for (std::size_t index = 0; index < polygons.size(); ++index) {
    const PolygonLocation location = locations[index].value_or(PolygonLocation{});
    if (locations[index].has_value()) {
      placed[location.cell].push_back({location.index, polygons[index]});
      continue;
    }
    const std::optional<std::size_t> cell = centroidCell(polygons[index]);
    if (!cell) return ARX_JSON_BAD_SCHEMA;
    placed[*cell].push_back({std::nullopt, polygons[index]});
  }

  for (std::size_t cell_index = 0; cell_index < placed.size(); ++cell_index) {
    std::vector<PlacedPolygon>& source = placed[cell_index];
    std::vector<std::optional<fts::Poly>> slots(source.size());
    for (PlacedPolygon& item : source) {
      const auto& index = item.index;
      if (!index) continue;
      if (*index >= slots.size() || slots[*index]) return ARX_JSON_BAD_SCHEMA;
      slots[*index] = item.polygon;
    }
    std::size_t next = 0;
    for (PlacedPolygon& item : source) {
      if (item.index) continue;
      while (next < slots.size() && slots[next]) ++next;
      if (next == slots.size()) return ARX_JSON_BAD_SCHEMA;
      slots[next] = item.polygon;
    }
    out.cells[cell_index].polygons.reserve(slots.size());
    for (std::optional<fts::Poly>& polygon : slots) {
      if (!polygon) return ARX_JSON_BAD_SCHEMA;
      out.cells[cell_index].polygons.push_back(*polygon);
    }
  }
  out.scene.num_polys = static_cast<std::int32_t>(polygons.size());
  return ARX_OK;
}

ArxReturnCode importFts(std::string_view text, fts::Data& out) {
  json_detail::Json root;
  ARX_RETURN_IF_ERR(json_detail::parse(text, root));
  if (!root.is_object() || !json_detail::validSchema(root, kFtsSchema)) return ARX_JSON_BAD_SCHEMA;
  ARX_RETURN_IF_ERR(parseHeader(root, out));
  ARX_RETURN_IF_ERR(parseUniqueHeaders(root, out));
  ARX_RETURN_IF_ERR(parseTextures(root, out));
  ARX_RETURN_IF_ERR(parseCellAnchors(root, out));
  std::vector<fts::Poly> polygons;
  ARX_RETURN_IF_ERR(parsePolygons(root, polygons));
  ARX_RETURN_IF_ERR(parseAnchors(root, out));
  ARX_RETURN_IF_ERR(parsePortals(root, out));
  ARX_RETURN_IF_ERR(parseRooms(root, out));
  ARX_RETURN_IF_ERR(parseRoomDistances(root, out));
  ARX_RETURN_IF_ERR(placePolygons(polygons, out));
  return validateFts(&out);
}

}  // namespace

ArxReturnCode exportFtsToJson(const fts::Data& data, bool pretty, std::string& out) {
  return json_detail::guarded("FTS export", [&]() -> ArxReturnCode {
    ARX_RETURN_IF_ERR(validateFts(&data));
    if (data.scene.sizex != kJsonGridSize || data.scene.sizez != kJsonGridSize) return ARX_JSON_BAD_SCHEMA;
    std::uint32_t level = 0;
    if (!paths::levelFromFts(json_detail::fixedString(data.header.path), level)) return ARX_JSON_BAD_SCHEMA;

    json_detail::Json root;
    root["$schema"] = kFtsSchema;
    root["header"] = {
        {"levelIdx", level},
        {"mScenePosition", json_detail::vector(data.scene.Mscenepos)},
    };

    root["uniqueHeaders"] = json_detail::Json::array();
    for (const fts::UniqueHeader3& header : data.unique_headers) {
      json_detail::Json check = json_detail::Json::array();
      for (char value : header.check) check.push_back(static_cast<std::uint8_t>(value));
      root["uniqueHeaders"].push_back({
          {"path", json_detail::fixedString(header.path)},
          {"check", std::move(check)},
      });
    }

    std::vector<std::pair<std::int32_t, const fts::Texture*>> textures;
    textures.reserve(data.textures.size());
    for (const auto& [id, texture] : data.textures) textures.emplace_back(id, &texture);
    std::sort(
        textures.begin(), textures.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
    root["textureContainers"] = json_detail::Json::array();
    for (const auto& [id, texture] : textures) {
      root["textureContainers"].push_back({{"id", id}, {"filename", textureJsonName(*texture)}});
    }

    root["cells"] = json_detail::Json::array();
    for (const fts::Cell& cell : data.cells) {
      json_detail::Json json = json_detail::Json::object();
      if (!cell.anchor_ids.empty()) json["anchors"] = cell.anchor_ids;
      root["cells"].push_back(std::move(json));
    }

    root["polygons"] = json_detail::Json::array();
    std::size_t lighting_index = 0;
    for (const fts::Cell& cell : data.cells)
      for (const fts::Poly& polygon : cell.polygons) root["polygons"].push_back(polygonJson(polygon, lighting_index));

    root["anchors"] = json_detail::Json::array();
    for (const fts::Anchor& anchor : data.anchors) {
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
    for (const fts::Portal& portal : data.portals) root["portals"].push_back(portalJson(portal));

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
    for (const fts::RoomDistData& distance : data.room_distances) {
      root["roomDistances"].push_back({
          {"distance", distance.distance},
          {"startPosition", json_detail::vector(distance.startpos)},
          {"endPosition", json_detail::vector(distance.endpos)},
      });
    }
    json_detail::dump(root, pretty, out);
    return ARX_OK;
  });
}

ArxReturnCode importJsonToFts(std::string_view text, fts::Data* out) {
  if (!out) return ARX_INVALID_DATA_POINTER;
  return json_detail::guarded("FTS import", [&] {
    fts::Data temporary;
    ArxReturnCode rc = importFts(text, temporary);
    if (rc == ARX_OK) *out = std::move(temporary);
    return rc;
  });
}

}  // namespace pistoris
