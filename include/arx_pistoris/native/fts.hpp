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

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace pistoris {

constexpr float kFtsVersion = 0.141f;
constexpr std::size_t kFtsMaxHeaderBlocks = 0x10000;
constexpr std::size_t kFtsMaxGridSize = 0x100000;
constexpr std::size_t kFtsMaxTextures = 0x8000;
constexpr std::size_t kFtsMaxPolygons = 0x100000;
constexpr std::size_t kFtsMaxCellPolygons = 0x8000;
constexpr std::size_t kFtsMaxAnchors = 0x100000;
constexpr std::size_t kFtsMaxPortals = 0x10000;
constexpr std::size_t kFtsMaxRooms = 254;
constexpr std::size_t kFtsMaxRoomTextureVertices = 0xffff;

namespace fts {

struct Header {
  char path[256] = {};
  std::int32_t count = 0;
  float version = kFtsVersion;
  std::int32_t uncompressedsize = 0;
  std::int32_t pad[3] = {};
};
static_assert(sizeof(Header) == 280);

struct UniqueHeader3 {
  char path[256] = {};
  char check[512] = {};
};
static_assert(sizeof(UniqueHeader3) == 768);

struct SceneHeader {
  float version = kFtsVersion;
  std::int32_t sizex = 0;
  std::int32_t sizez = 0;
  std::int32_t num_textures = 0;
  std::int32_t num_polys = 0;
  std::int32_t num_anchors = 0;
  ArxVector3 playerpos = {};
  ArxVector3 Mscenepos = {};  // NOLINT(readability-identifier-naming)
  std::int32_t num_portals = 0;
  std::int32_t num_rooms = 0;
};
static_assert(sizeof(SceneHeader) == 56);

struct Texture {
  std::int32_t temp = 0;
  char fic[256] = {};
};

struct SceneInfo {
  std::int32_t nbpoly = 0;
  std::int32_t nbianchors = 0;
};
static_assert(sizeof(SceneInfo) == 8);

struct Vertex {
  float sy = 0.0f;
  float ssx = 0.0f;
  float ssz = 0.0f;
  float stu = 0.0f;
  float stv = 0.0f;
};
static_assert(sizeof(Vertex) == 20);

struct Poly {
  Vertex v[4] = {};
  std::int32_t tex = 0;
  ArxVector3 norm = {};
  ArxVector3 norm2 = {};
  ArxVector3 nrml[4] = {};
  float transval = 0.0f;
  float area = 0.0f;
  FaceType type = 0;
  std::int16_t room = 0;
  std::int16_t paddy = 0;
};
static_assert(sizeof(Poly) == 172);

struct AnchorData {
  ArxVector3 pos = {};
  float radius = 0.0f;
  float height = 0.0f;
  std::int16_t num_linked = 0;
  std::int16_t flags = 0;
};
static_assert(sizeof(AnchorData) == 24);

struct SavedTextureVertex {
  ArxVector3 pos = {};
  float rhw = 0.0f;
  std::uint32_t color = 0;
  std::uint32_t specular = 0;
  float tu = 0.0f;
  float tv = 0.0f;
};
static_assert(sizeof(SavedTextureVertex) == 32);

struct SavePoly {
  std::int32_t type = 0;
  ArxVector3 min = {};
  ArxVector3 max = {};
  ArxVector3 norm = {};
  ArxVector3 norm2 = {};
  SavedTextureVertex v[4] = {};
  SavedTextureVertex tv[4] = {};
  ArxVector3 nrml[4] = {};
  std::int32_t tex = 0;
  ArxVector3 center = {};
  float transval = 0.0f;
  float area = 0.0f;
  std::int16_t room = 0;
  std::int16_t misc = 0;
};
static_assert(sizeof(SavePoly) == 384);

struct Portal {
  SavePoly poly = {};
  std::int32_t room_1 = 0;
  std::int32_t room_2 = 0;
  std::int16_t useportal = 0;
  std::int16_t paddy = 0;
};
static_assert(sizeof(Portal) == 396);

struct RoomData {
  std::int32_t num_portals = 0;
  std::int32_t num_polys = 0;
  std::int32_t padd[6] = {};
};
static_assert(sizeof(RoomData) == 32);

struct EpData {
  std::int16_t px = 0;
  std::int16_t py = 0;
  std::int16_t idx = 0;
  std::int16_t padd = 0;
};
static_assert(sizeof(EpData) == 8);

struct RoomDistData {
  float distance = 0.0f;
  ArxVector3 startpos = {};
  ArxVector3 endpos = {};
};
static_assert(sizeof(RoomDistData) == 28);

struct Cell {
  std::vector<Poly> polygons;
  std::vector<std::int32_t> anchor_ids;
};

struct Anchor {
  AnchorData data;
  std::vector<std::int32_t> linked;
};

struct Room {
  RoomData data;
  std::vector<std::int32_t> portal_ids;
  std::vector<EpData> polygons;
};

struct Data {  // NOLINT(bugprone-exception-escape)
  Header header;
  std::vector<UniqueHeader3> unique_headers;
  SceneHeader scene;
  std::unordered_map<std::int32_t, Texture> textures;
  std::vector<Cell> cells;  // row-major, sizez * sizex
  std::vector<Anchor> anchors;
  std::vector<Portal> portals;
  std::vector<Room> rooms;                   // num_rooms + 1
  std::vector<RoomDistData> room_distances;  // (num_rooms + 1) * (num_rooms + 1)
};

}  // namespace fts

using Fts = fts::Data;

}  // namespace pistoris
