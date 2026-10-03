
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native/fts.hpp"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/accessor.h"
#include "external/glb/level/api.h"
#include "external/glb/level/coordinates.h"
#include "external/glb/level/palette.h"
#include "external/glb/writer.h"
#include "native/fts.h"
#include "utils/log.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <vector>

namespace pistoris {
namespace {

using glb::Builder;
using GlbVec3 = glb::Vec3;

std::array<GlbVec3, 4> polyPositions(const fts::Poly& poly) {
  return {{{poly.v[0].ssx, poly.v[0].sy, poly.v[0].ssz},
           {poly.v[1].ssx, poly.v[1].sy, poly.v[1].ssz},
           {poly.v[2].ssx, poly.v[2].sy, poly.v[2].ssz},
           {poly.v[3].ssx, poly.v[3].sy, poly.v[3].ssz}}};
}

void appendPoly(std::vector<GlbVec3>& positions, std::vector<std::uint32_t>& indices, const fts::Poly& poly) {
  std::uint32_t base = static_cast<std::uint32_t>(positions.size());
  auto values = polyPositions(poly);
  positions.push_back(values[0]);
  positions.push_back(values[1]);
  positions.push_back(values[2]);
  if ((poly.type & kFaceBitQuad) != 0) {
    positions.push_back(values[3]);
    indices.insert(indices.end(), {base, base + 1, base + 2, base + 3, base + 2, base + 1});
  } else {
    indices.insert(indices.end(), {base, base + 1, base + 2});
  }
}

void appendSavePoly(std::vector<GlbVec3>& positions, std::vector<std::uint32_t>& indices, const fts::SavePoly& poly) {
  std::uint32_t base = static_cast<std::uint32_t>(positions.size());
  std::size_t count = (poly.type & kFaceBitQuad) != 0 ? 4U : 3U;
  for (std::size_t i = 0; i < count; ++i) {
    const ArxVector3& position = poly.v[i].pos;
    positions.push_back({position.x, position.y, position.z});
  }
  if (count == 4) {
    indices.insert(indices.end(), {base, base + 1, base + 2, base + 3, base + 2, base + 1});
  } else {
    indices.insert(indices.end(), {base, base + 1, base + 2});
  }
}

}  // namespace

ArxReturnCode buildFtsCellsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out,
                                    const Level::GlbExportOptions& options) {
  ArxReturnCode rc = validateFts(&fts);
  if (rc != ARX_OK) return rc;

  Builder builder;
  glb_level::Palette palette(builder);
  rc = glb_level::configureGlbExportCoordinates(builder, options);
  if (rc != ARX_OK) return rc;
  int material = palette.material(glb_level::PaletteItem::kDebugGeometry);

  std::size_t emitted_cells = 0;
  std::size_t emitted_polys = 0;
  for (std::int32_t z = 0; z < fts.scene.sizez; ++z) {
    for (std::int32_t x = 0; x < fts.scene.sizex; ++x) {
      std::size_t cell_index =
          static_cast<std::size_t>(z) * static_cast<std::size_t>(fts.scene.sizex) + static_cast<std::size_t>(x);
      const auto& cell = fts.cells[cell_index];
      if (cell.polygons.empty()) continue;

      std::vector<GlbVec3> positions;
      std::vector<std::uint32_t> indices;
      positions.reserve(cell.polygons.size() * 4);
      indices.reserve(cell.polygons.size() * 6);
      for (const auto& poly : cell.polygons) appendPoly(positions, indices, poly);
      builder.addDebugMeshNode(std::format("fts_cell__x{}_z{}", x, z), positions, indices, material);
      ++emitted_cells;
      emitted_polys += cell.polygons.size();
    }
  }

  log(ARX_LOG_INFO, "FTS debug cells GLB export: {} cells, {} polygons", emitted_cells, emitted_polys);
  return builder.write(out);
}

ArxReturnCode buildFtsRoomsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out) {
  ArxReturnCode rc = validateFts(&fts);
  if (rc != ARX_OK) return rc;

  Builder builder;
  glb_level::Palette palette(builder);
  int portal_material = palette.material(glb_level::PaletteItem::kPortal);

  std::vector<int> room_materials;
  room_materials.reserve(fts.rooms.size());
  for (std::size_t room = 0; room < fts.rooms.size(); ++room) {
    room_materials.push_back(palette.roomMaterial(room));
  }

  std::size_t emitted_rooms = 0;
  std::size_t emitted_polys = 0;
  for (std::size_t room_index = 0; room_index < fts.rooms.size(); ++room_index) {
    const auto& room = fts.rooms[room_index];
    std::vector<GlbVec3> positions;
    std::vector<std::uint32_t> indices;
    positions.reserve(room.polygons.size() * 4);
    indices.reserve(room.polygons.size() * 6);
    for (const auto& polygon : room.polygons) {
      if (polygon.px < 0 || polygon.py < 0) continue;
      std::size_t cell_index = static_cast<std::size_t>(polygon.py) * static_cast<std::size_t>(fts.scene.sizex) +
                               static_cast<std::size_t>(polygon.px);
      if (cell_index >= fts.cells.size()) continue;
      const auto& cell = fts.cells[cell_index];
      if (polygon.idx < 0 || static_cast<std::size_t>(polygon.idx) >= cell.polygons.size()) continue;
      appendPoly(positions, indices, cell.polygons[static_cast<std::size_t>(polygon.idx)]);
    }
    if (positions.empty()) continue;
    builder.addDebugMeshNode(std::format("fts_room__r{}", room_index), positions, indices, room_materials[room_index]);
    ++emitted_rooms;
    emitted_polys += room.polygons.size();
  }

  std::size_t emitted_portals = 0;
  for (const auto& portal : fts.portals) {
    std::vector<GlbVec3> positions;
    std::vector<std::uint32_t> indices;
    appendSavePoly(positions, indices, portal.poly);
    builder.addDebugMeshNode(std::format("fts_portal__r{}_r{}_u{}", portal.room_1, portal.room_2, portal.useportal),
                             positions,
                             indices,
                             portal_material);
    ++emitted_portals;
  }

  log(ARX_LOG_INFO,
      "FTS debug rooms GLB export: {} rooms, {} polygons, {} portals",
      emitted_rooms,
      emitted_polys,
      emitted_portals);
  return builder.write(out);
}

}  // namespace pistoris
