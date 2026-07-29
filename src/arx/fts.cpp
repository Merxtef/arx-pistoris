/*
 * Copyright 2011-2022 Arx Libertatis Team (see the AUTHORS file)
 *
 * This file is part of Arx Libertatis.
 *
 * Arx Libertatis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Arx Libertatis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Arx Libertatis.  If not, see <http://www.gnu.org/licenses/>.
 */
/* Based on:
===========================================================================
ARX FATALIS GPL Source Code
Copyright (C) 1999-2010 Arkane Studios SA, a ZeniMax Media company.

This file is part of the Arx Fatalis GPL Source Code ('Arx Fatalis Source Code').

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any
later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source Code.  If not, see
<http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should have received a copy of
these additional terms immediately following the terms and conditions of the GNU General Public License which
accompanied the Arx Fatalis Source Code. If not, please request a copy in writing from Arkane Studios at the address
below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing Arkane
Studios, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// Source:
// https://github.com/arx/ArxLibertatis/blob/5b95e4c5ca9d583f1b11c085326979772645e0f3/src/graphics/data/FastSceneFormat.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#include "arx/fts.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "modules/rooms.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/mem_utils.h"
#include "utils/parse_utils.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris {

namespace {

bool countFits(std::int32_t n, std::size_t max) { return n >= 0 && static_cast<std::size_t>(n) <= max; }

bool mulFits(std::size_t a, std::size_t b, std::size_t max, std::size_t& out) {
  if (a != 0 && b > max / a) return false;
  out = a * b;
  return out <= max;
}

bool sizeFitsInt32(std::size_t n) { return n <= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()); }

char lowerAscii(char value) noexcept {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

std::string runtimeTextureResourceKey(std::string_view source) {
  std::string normalized(source.size(), '\0');
  std::size_t input_start = 0;
  std::size_t output_start = 0;
  while (input_start < source.size()) {
    std::size_t separator = source.find_first_of("/\\", input_start);
    if (separator == std::string_view::npos) separator = source.size();

    const std::size_t component_start = input_start;
    input_start = separator + 1U;
    if (separator == component_start) continue;
    if (separator - component_start == 1U && source[component_start] == '.') continue;
    if (separator - component_start == 2U && source[component_start] == '.' && source[component_start + 1U] == '.') {
      if (output_start == 0) {
        normalized[output_start++] = '.';
        normalized[output_start++] = '.';
      } else {
        const std::size_t previous_separator = normalized.find_last_of('/', output_start - 1U);
        if (previous_separator == std::string::npos) {
          if (output_start == 2U && normalized[0] == '.' && normalized[1] == '.') {
            normalized[output_start++] = '/';
            normalized[output_start++] = '.';
            normalized[output_start++] = '.';
          } else {
            output_start = 0;
          }
        } else if (output_start - previous_separator - 1U == 2U && normalized[previous_separator + 1U] == '.' &&
                   normalized[previous_separator + 2U] == '.') {
          normalized[output_start++] = '/';
          normalized[output_start++] = '.';
          normalized[output_start++] = '.';
        } else {
          output_start = previous_separator;
        }
      }
      continue;
    }

    if (output_start != 0) normalized[output_start++] = '/';
    for (std::size_t i = component_start; i < separator; ++i) normalized[output_start++] = lowerAscii(source[i]);
  }
  normalized.resize(output_start);

  const bool has_info =
      !normalized.empty() && normalized != ".." && !(normalized.size() >= 3U && normalized.ends_with("/.."));
  if (has_info) {
    const std::size_t extension = normalized.find_last_of("/.");
    if (extension != std::string::npos && normalized[extension] == '.') normalized.resize(extension);
  }
  return normalized;
}

bool roomDistCount(std::int32_t num_rooms, std::size_t& out) {
  if (num_rooms < 0 || static_cast<std::size_t>(num_rooms) > kFtsMaxRooms) return false;
  const std::size_t room_count = static_cast<std::size_t>(num_rooms) + 1U;
  return mulFits(room_count, room_count, (kFtsMaxRooms + 1U) * (kFtsMaxRooms + 1U), out);
}

struct TextureRecord {
  std::int32_t tc = 0;
  std::int32_t temp = 0;
  char fic[256] = {};
};
static_assert(sizeof(TextureRecord) == 264);

bool validPortalGeometry(const fts::SavePoly& poly) {
  Portal portal;
  portal.shape = (poly.type & kFaceBitQuad) != 0 ? PortalShape::kQuad : PortalShape::kTriangle;
  if (portal.shape == PortalShape::kQuad) {
    portal.vertices = {poly.v[0].pos, poly.v[1].pos, poly.v[3].pos, poly.v[2].pos};
  } else {
    for (std::size_t i = 0; i < 3; ++i) portal.vertices[i] = poly.v[i].pos;
  }
  return rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kValid;
}

ArxReturnCode validateRoomTextureVertexCounts(const fts::Data& data) {
  try {
    std::unordered_map<std::string, std::size_t> batches_by_resource;
    std::unordered_map<std::int32_t, std::size_t> batch_by_texture;
    batches_by_resource.reserve(data.textures.size());
    batch_by_texture.reserve(data.textures.size());
    for (const auto& [id, texture] : data.textures) {
      std::string resource = runtimeTextureResourceKey(texture.fic);
      if (resource.empty()) continue;
      const std::size_t next_batch = batches_by_resource.size();
      const auto [batch, inserted] = batches_by_resource.try_emplace(std::move(resource), next_batch);
      (void)inserted;
      batch_by_texture.emplace(id, batch->second);
    }

    std::vector<std::size_t> vertex_counts(batches_by_resource.size());
    for (const fts::Room& room : data.rooms) {
      std::fill(vertex_counts.begin(), vertex_counts.end(), 0U);
      for (const fts::EpData& reference : room.polygons) {
        const std::size_t cell_index =
            static_cast<std::size_t>(reference.py) * static_cast<std::size_t>(data.scene.sizex) +
            static_cast<std::size_t>(reference.px);
        const fts::Poly& polygon = data.cells[cell_index].polygons[static_cast<std::size_t>(reference.idx)];
        if ((polygon.type & (kFaceBitIgnore | kFaceBitHide)) != 0 || polygon.tex == 0) continue;

        const auto batch = batch_by_texture.find(polygon.tex);
        if (batch == batch_by_texture.end()) continue;
        const std::size_t vertex_count = (polygon.type & kFaceBitQuad) != 0 ? 4U : 3U;
        std::size_t& count = vertex_counts[batch->second];
        if (count > kFtsMaxRoomTextureVertices - vertex_count) return ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT;
        count += vertex_count;
      }
    }
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  }
  return ARX_OK;
}

ArxReturnCode readPayload(fts::Data* d, ReadCursor& c) {
  c.read(d->scene);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (d->scene.version != kFtsVersion) return ARX_FTS_BAD_VERSION;
  if (!countFits(d->scene.sizex, kFtsMaxGridSize)) return ARX_FTS_BAD_GRID_SIZE;
  if (!countFits(d->scene.sizez, kFtsMaxGridSize)) return ARX_FTS_BAD_GRID_SIZE;
  if (!countFits(d->scene.num_textures, kFtsMaxTextures)) return ARX_FTS_BAD_TEXTURE_COUNT;
  if (!countFits(d->scene.num_polys, kFtsMaxPolygons)) return ARX_FTS_BAD_POLYGON_COUNT;
  if (!countFits(d->scene.num_anchors, kFtsMaxAnchors)) return ARX_FTS_BAD_ANCHOR_COUNT;
  if (!countFits(d->scene.num_portals, kFtsMaxPortals)) return ARX_FTS_BAD_PORTAL_COUNT;
  if (!countFits(d->scene.num_rooms, kFtsMaxRooms)) return ARX_FTS_BAD_ROOM_COUNT;

  std::size_t cell_count = 0;
  if (!mulFits(static_cast<std::size_t>(d->scene.sizex),
               static_cast<std::size_t>(d->scene.sizez),
               kFtsMaxGridSize,
               cell_count))
    return ARX_FTS_BAD_GRID_SIZE;

  if (!tryReserve(d->textures, static_cast<std::size_t>(d->scene.num_textures))) return ARX_BAD_ALLOC;
  for (std::int32_t i = 0; i < d->scene.num_textures; ++i) {
    TextureRecord record;
    c.read(record);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (record.tc <= 0) return ARX_FTS_BAD_TEXTURE_ID;
    clampStr(record.fic, "FTS: texture.fic", i);
    fts::Texture texture;
    texture.temp = record.temp;
    std::memcpy(texture.fic, record.fic, sizeof(texture.fic));
    try {
      if (!d->textures.emplace(record.tc, texture).second) return ARX_FTS_DUPLICATE_TEXTURE_ID;
    } catch (const std::bad_alloc&) {
      return ARX_BAD_ALLOC;
    }
  }

  if (!tryResize(d->cells, cell_count)) return ARX_BAD_ALLOC;
  std::size_t poly_total = 0;
  for (auto& cell : d->cells) {
    fts::SceneInfo info;
    c.read(info);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (!countFits(info.nbpoly, kFtsMaxCellPolygons)) return ARX_FTS_BAD_CELL_POLYGON_COUNT;
    if (!countFits(info.nbianchors, kFtsMaxAnchors)) return ARX_FTS_BAD_CELL_ANCHOR_COUNT;
    if (static_cast<std::size_t>(info.nbpoly) > kFtsMaxPolygons - poly_total) return ARX_FTS_BAD_POLYGON_COUNT;
    poly_total += static_cast<std::size_t>(info.nbpoly);

    if (!tryResize(cell.polygons, info.nbpoly)) return ARX_BAD_ALLOC;
    ARX_RETURN_IF_ERR(c.readArray(cell.polygons));
    if (!tryResize(cell.anchor_ids, info.nbianchors)) return ARX_BAD_ALLOC;
    ARX_RETURN_IF_ERR(c.readArray(cell.anchor_ids));
  }
  if (poly_total != static_cast<std::size_t>(d->scene.num_polys)) return ARX_FTS_BAD_POLYGON_COUNT;

  if (!tryResize(d->anchors, d->scene.num_anchors)) return ARX_BAD_ALLOC;
  for (auto& anchor : d->anchors) {
    c.read(anchor.data);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (!countFits(anchor.data.num_linked, kFtsMaxAnchors)) return ARX_FTS_BAD_ANCHOR_LINK_COUNT;
    if (!tryResize(anchor.linked, anchor.data.num_linked)) return ARX_BAD_ALLOC;
    ARX_RETURN_IF_ERR(c.readArray(anchor.linked));
  }

  if (!tryResize(d->portals, d->scene.num_portals)) return ARX_BAD_ALLOC;
  ARX_RETURN_IF_ERR(c.readArray(d->portals));

  if (!tryResize(d->rooms, static_cast<std::size_t>(d->scene.num_rooms) + 1)) return ARX_BAD_ALLOC;
  for (auto& room : d->rooms) {
    c.read(room.data);
    if (!c) return ARX_UNEXPECTED_EOF;
    if (!countFits(room.data.num_portals, kFtsMaxPortals)) return ARX_FTS_BAD_ROOM_PORTAL_COUNT;
    if (!countFits(room.data.num_polys, kFtsMaxPolygons)) return ARX_FTS_BAD_ROOM_POLYGON_COUNT;
    if (!tryResize(room.portal_ids, room.data.num_portals)) return ARX_BAD_ALLOC;
    ARX_RETURN_IF_ERR(c.readArray(room.portal_ids));
    if (!tryResize(room.polygons, room.data.num_polys)) return ARX_BAD_ALLOC;
    ARX_RETURN_IF_ERR(c.readArray(room.polygons));
  }

  std::size_t room_dist_count = 0;
  if (!roomDistCount(d->scene.num_rooms, room_dist_count)) return ARX_FTS_BAD_ROOM_COUNT;
  if (!tryResize(d->room_distances, room_dist_count)) return ARX_BAD_ALLOC;
  ARX_RETURN_IF_ERR(c.readArray(d->room_distances));

  return ARX_OK;
}

WriteCursor& writePayload(const fts::Data* d, WriteCursor& c) {
  fts::SceneHeader scene = d->scene;
  scene.num_textures = static_cast<std::int32_t>(d->textures.size());
  c.write(scene);
  for (const auto& [id, texture] : d->textures) {
    TextureRecord record;
    record.tc = id;
    record.temp = texture.temp;
    std::memcpy(record.fic, texture.fic, sizeof(record.fic));
    c.write(record);
  }
  for (const auto& cell : d->cells) {
    fts::SceneInfo info;
    info.nbpoly = static_cast<std::int32_t>(cell.polygons.size());
    info.nbianchors = static_cast<std::int32_t>(cell.anchor_ids.size());
    c.write(info);
    c.writeArray(cell.polygons);
    c.writeArray(cell.anchor_ids);
  }
  for (const auto& anchor : d->anchors) {
    fts::AnchorData data = anchor.data;
    data.num_linked = static_cast<std::int16_t>(anchor.linked.size());
    c.write(data);
    c.writeArray(anchor.linked);
  }
  c.writeArray(d->portals);
  for (const auto& room : d->rooms) {
    fts::RoomData data = room.data;
    data.num_portals = static_cast<std::int32_t>(room.portal_ids.size());
    data.num_polys = static_cast<std::int32_t>(room.polygons.size());
    c.write(data);
    c.writeArray(room.portal_ids);
    c.writeArray(room.polygons);
  }
  c.writeArray(d->room_distances);
  return c;
}

}  // namespace

ArxReturnCode loadFts(fts::Data* d, ReadCursor& c) {
  fts::Data tmp;
  c.read(tmp.header);
  if (!c) return ARX_UNEXPECTED_EOF;
  if (tmp.header.version != kFtsVersion) return ARX_FTS_BAD_VERSION;
  if (!countFits(tmp.header.count, kFtsMaxHeaderBlocks)) return ARX_FTS_BAD_METADATA_COUNT;

  if (!tryResize(tmp.unique_headers, tmp.header.count)) return ARX_BAD_ALLOC;
  ARX_RETURN_IF_ERR(c.readArray(tmp.unique_headers));

  std::vector<std::uint8_t> payload;
  if (!tryResize(payload, c.remaining())) return ARX_BAD_ALLOC;
  ARX_RETURN_IF_ERR(c.readArray(payload));

  ReadCursor payload_cursor(payload.data(), payload.size());
  ArxReturnCode rc = readPayload(&tmp, payload_cursor);
  if (rc != ARX_OK) return rc;
  rc = validateFts(&tmp);
  if (rc != ARX_OK) return rc;

  *d = std::move(tmp);
  log(ARX_LOG_INFO,
      std::format("FTS loaded: {}x{} cells, {} polygons, {} textures, {} anchors, {} portals, {} rooms",
                  d->scene.sizex,
                  d->scene.sizez,
                  d->scene.num_polys,
                  d->textures.size(),
                  d->anchors.size(),
                  d->portals.size(),
                  d->scene.num_rooms));
  return ARX_OK;
}

ArxReturnCode saveFts(const fts::Data* d, WriteCursor& c) {
  ARX_RETURN_IF_ERR(validateFts(d));

  fts::Header header = d->header;
  header.count = static_cast<std::int32_t>(d->unique_headers.size());
  header.version = kFtsVersion;
  header.uncompressedsize = 0;

  log(ARX_LOG_INFO,
      std::format("FTS saving: {}x{} cells, {} polygons, {} textures, {} anchors, {} portals, {} rooms",
                  d->scene.sizex,
                  d->scene.sizez,
                  d->scene.num_polys,
                  d->textures.size(),
                  d->anchors.size(),
                  d->portals.size(),
                  d->scene.num_rooms));

  c.write(header);
  c.writeArray(d->unique_headers);
  writePayload(d, c);
  return c ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode validateFts(const fts::Data* d) {
  if (d == nullptr) return ARX_INVALID_DATA_POINTER;
  if (d->header.version != kFtsVersion) return ARX_FTS_BAD_VERSION;
  if (d->scene.version != kFtsVersion) return ARX_FTS_BAD_VERSION;
  if (d->unique_headers.size() > kFtsMaxHeaderBlocks || !sizeFitsInt32(d->unique_headers.size()))
    return ARX_FTS_BAD_METADATA_COUNT;
  if (!countFits(d->scene.sizex, kFtsMaxGridSize) || !countFits(d->scene.sizez, kFtsMaxGridSize))
    return ARX_FTS_BAD_GRID_SIZE;

  std::size_t cell_count = 0;
  if (!mulFits(static_cast<std::size_t>(d->scene.sizex),
               static_cast<std::size_t>(d->scene.sizez),
               kFtsMaxGridSize,
               cell_count))
    return ARX_FTS_BAD_GRID_SIZE;
  if (d->cells.size() != cell_count) return ARX_FTS_BAD_GRID_SIZE;

  if (d->textures.size() > kFtsMaxTextures || !sizeFitsInt32(d->textures.size())) return ARX_FTS_BAD_TEXTURE_COUNT;
  for (const auto& [id, texture] : d->textures) {
    if (id <= 0) return ARX_FTS_BAD_TEXTURE_ID;
    const void* terminator = std::memchr(texture.fic, '\0', sizeof(texture.fic));
    if (terminator == nullptr) return ARX_FTS_BAD_TEXTURE_PATH;
  }
  if (d->anchors.size() > kFtsMaxAnchors || !sizeFitsInt32(d->anchors.size())) return ARX_FTS_BAD_ANCHOR_COUNT;
  if (d->portals.size() > kFtsMaxPortals || !sizeFitsInt32(d->portals.size())) return ARX_FTS_BAD_PORTAL_COUNT;
  if (static_cast<std::size_t>(d->scene.num_rooms) > kFtsMaxRooms || d->scene.num_rooms < 0)
    return ARX_FTS_BAD_ROOM_COUNT;
  if (d->rooms.size() != static_cast<std::size_t>(d->scene.num_rooms) + 1) return ARX_FTS_BAD_ROOM_COUNT;

  std::size_t poly_total = 0;
  for (const auto& cell : d->cells) {
    if (cell.polygons.size() > kFtsMaxCellPolygons || cell.polygons.size() > kFtsMaxPolygons - poly_total ||
        !sizeFitsInt32(cell.polygons.size()))
      return ARX_FTS_BAD_CELL_POLYGON_COUNT;
    if (cell.anchor_ids.size() > kFtsMaxAnchors || !sizeFitsInt32(cell.anchor_ids.size()))
      return ARX_FTS_BAD_CELL_ANCHOR_COUNT;
    poly_total += cell.polygons.size();
    for (std::int32_t idx : cell.anchor_ids)
      if (idx < 0 || static_cast<std::size_t>(idx) >= d->anchors.size()) return ARX_FTS_BAD_ANCHOR_INDEX;
    for (const auto& poly : cell.polygons) {
      if ((poly.type & kFaceBitsAll) != poly.type) return ARX_FTS_BAD_POLYGON_TYPE;
      if (poly.tex < 0) return ARX_FTS_BAD_POLYGON_TEXTURE_ID;
      if (poly.tex > 0 && !d->textures.contains(poly.tex)) return ARX_FTS_BAD_POLYGON_TEXTURE_ID;
      if (!std::isfinite(poly.transval)) return ARX_FTS_BAD_POLYGON_TRANSVAL;
      std::size_t vertex_count = (poly.type & kFaceBitQuad) != 0 ? 4U : 3U;
      for (std::size_t i = 0; i < vertex_count; ++i) {
        ArxVector3 position = {poly.v[i].ssx, poly.v[i].sy, poly.v[i].ssz};
        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) ||
            position.x < 0.0f || position.x > 16000.0f || position.z < 0.0f || position.z > 16000.0f)
          return ARX_FTS_BAD_POLYGON_POSITION;
        if (!std::isfinite(poly.v[i].stu) || !std::isfinite(poly.v[i].stv)) return ARX_FTS_BAD_POLYGON_UV;
      }
    }
  }
  if (poly_total != static_cast<std::size_t>(d->scene.num_polys)) return ARX_FTS_BAD_POLYGON_COUNT;

  if (d->scene.num_textures != static_cast<std::int32_t>(d->textures.size())) return ARX_FTS_BAD_TEXTURE_COUNT;
  if (d->scene.num_anchors != static_cast<std::int32_t>(d->anchors.size())) return ARX_FTS_BAD_ANCHOR_COUNT;
  if (d->scene.num_portals != static_cast<std::int32_t>(d->portals.size())) return ARX_FTS_BAD_PORTAL_COUNT;

  for (const auto& anchor : d->anchors) {
    const auto& data = anchor.data;
    if (!std::isfinite(data.pos.x) || !std::isfinite(data.pos.y) || !std::isfinite(data.pos.z)) {
      return ARX_FTS_BAD_ANCHOR_POSITION;
    }
    if (anchor.linked.size() > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()))
      return ARX_FTS_BAD_ANCHOR_LINK_COUNT;
    for (std::int32_t idx : anchor.linked)
      if (idx < 0 || static_cast<std::size_t>(idx) >= d->anchors.size()) return ARX_FTS_BAD_ANCHOR_INDEX;
  }

  for (const auto& portal : d->portals) {
    if (portal.room_1 < 0 || portal.room_2 < 0 || portal.room_1 > d->scene.num_rooms ||
        portal.room_2 > d->scene.num_rooms)
      return ARX_FTS_BAD_PORTAL_ROOM_INDEX;
    if ((portal.poly.type & ~kFaceBitQuad) != 0) return ARX_FTS_BAD_PORTAL_TYPE;
    std::size_t vertex_count = (portal.poly.type & kFaceBitQuad) != 0 ? 4U : 3U;
    for (std::size_t i = 0; i < vertex_count; ++i) {
      const ArxVector3& position = portal.poly.v[i].pos;
      if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || position.x < 0.0f ||
          position.x > 16000.0f || position.z < 0.0f || position.z > 16000.0f)
        return ARX_FTS_BAD_PORTAL_POSITION;
    }
    if (!validPortalGeometry(portal.poly)) return ARX_FTS_BAD_PORTAL_GEOMETRY;
  }

  for (const auto& room : d->rooms) {
    if (room.portal_ids.size() > kFtsMaxPortals || !sizeFitsInt32(room.portal_ids.size()))
      return ARX_FTS_BAD_ROOM_PORTAL_COUNT;
    if (room.polygons.size() > kFtsMaxPolygons || !sizeFitsInt32(room.polygons.size()))
      return ARX_FTS_BAD_ROOM_POLYGON_COUNT;
    for (std::int32_t idx : room.portal_ids)
      if (idx < 0 || static_cast<std::size_t>(idx) >= d->portals.size()) return ARX_FTS_BAD_ROOM_PORTAL_INDEX;
    for (const auto& ep : room.polygons) {
      if (ep.px < 0 || ep.py < 0 || ep.idx < 0) return ARX_FTS_BAD_ROOM_POLYGON_INDEX;
      if (ep.px >= d->scene.sizex || ep.py >= d->scene.sizez) return ARX_FTS_BAD_ROOM_POLYGON_INDEX;
      const auto cell_idx =
          static_cast<std::size_t>(ep.py) * static_cast<std::size_t>(d->scene.sizex) + static_cast<std::size_t>(ep.px);
      if (static_cast<std::size_t>(ep.idx) >= d->cells[cell_idx].polygons.size()) return ARX_FTS_BAD_ROOM_POLYGON_INDEX;
    }
  }
  ArxReturnCode rc = validateRoomTextureVertexCounts(*d);
  if (rc != ARX_OK) return rc;

  std::size_t room_dist_count = 0;
  if (!roomDistCount(d->scene.num_rooms, room_dist_count)) return ARX_FTS_BAD_ROOM_COUNT;
  if (d->room_distances.size() != room_dist_count) return ARX_FTS_BAD_ROOM_DISTANCE_COUNT;
  for (const fts::RoomDistData& distance : d->room_distances)
    if (!std::isfinite(distance.distance)) return ARX_FTS_BAD_ROOM_DISTANCE;

  return ARX_OK;
}

}  // namespace pistoris
