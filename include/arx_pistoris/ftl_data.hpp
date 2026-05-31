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

Arx Fatalis Source Code is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

Arx Fatalis Source Code is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with Arx Fatalis Source
Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Arx Fatalis Source Code is also subject to certain additional terms. You should
have received a copy of these additional terms immediately following the terms and conditions of the
GNU General Public License which accompanied the Arx Fatalis Source Code. If not, please request a
copy in writing from Arkane Studios at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in
writing Arkane Studios, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/
// Copyright (c) 1999-2001 ARKANE Studios SA. All rights reserved
// Source: https://github.com/arx/ArxLibertatis/blob/1dfb6c4/src/graphics/data/FTLFormat.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#pragma once
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/common_data.hpp"

#include <cstdint>
#include <limits>
#include <vector>

namespace pistoris {

constexpr char kFtlMagic[4]             = {'F', 'T', 'L', '\0'};
constexpr uint32_t kFtlVersion          = 0x3F55234F;  // IEEE 754 float: 0.83257f
constexpr std::int16_t kFtlTextureNone  = -1;
constexpr std::size_t kFtlMaxVertices   = 0xFFFF;   // uint16_t::max reserved as sentinel
constexpr std::size_t kFtlMaxTextures   = 0x8000;   // int16_t non-negative
constexpr std::size_t kFtlMaxFaces      = 0x20000;  // sanity cap; not Arx-enforced
constexpr std::size_t kFtlMaxGroups     = 0xFFFF;   // matches vertex limit
constexpr std::size_t kFtlMaxActions    = 0x400;    // sanity cap; not Arx-enforced
constexpr std::size_t kFtlMaxSelections = 0xFFFF;   // matches vertex limit

namespace ftl {

struct Vertex {
  ArxVector3 position;
  ArxVector3 normal;
};
static_assert(sizeof(Vertex) == 24);  // read from file directly

struct Face {
  FaceType type                  = 0;
  Vec3<std::uint16_t> vertex_idx = {std::numeric_limits<std::uint16_t>::max(),
                                    std::numeric_limits<std::uint16_t>::max(),
                                    std::numeric_limits<std::uint16_t>::max()};
  std::int16_t texture_id        = kFtlTextureNone;
  ArxVector3 u                   = {};
  ArxVector3 v                   = {};
  float transval                 = 0;
  ArxVector3 norm                = {};
};

struct Header {
  std::uint32_t origin = std::numeric_limits<std::uint32_t>::max();
  char name[256]       = {};
};
static_assert(sizeof(Header) == 260);  // read from file directly

struct TextureContainer {
  char filename[256] = {};
};
static_assert(sizeof(TextureContainer) == 256);  // read from file directly

struct Group {
  char name[256]       = {};
  std::uint32_t origin = std::numeric_limits<std::uint32_t>::max();
  std::vector<std::int32_t> indices;
  float blob_shadow_size = 0.0f;
};

struct Action {
  char name[256]          = {};
  std::int32_t vertex_idx = std::numeric_limits<std::int32_t>::max();
  std::int32_t action     = 0;
  std::int32_t sfx        = 0;
};
static_assert(sizeof(Action) == 268);  // read from file directly

struct Selection {
  char name[64] = {};
  std::vector<std::int32_t> selected;
};

// Recomputed by validateFtl; not serialized
struct Extras {
  std::vector<std::int32_t> vertex_to_bone;  // [vi] = owning group; -1 none; last group wins
  std::vector<std::int32_t> parent_bone;     // [gi] = parent group; -1 root
  std::vector<ArxVector3> bone_world_pos;    // [gi] = world pos of bone's origin vertex
};

struct Data {
  Header header;
  std::vector<Vertex> vertices;
  std::vector<Face> faces;
  std::vector<TextureContainer> texture_containers;
  std::vector<Group> groups;
  std::vector<Action> actions;
  std::vector<Selection> selections;
  mutable Extras extras;
};

}  // namespace ftl

}  // namespace pistoris
