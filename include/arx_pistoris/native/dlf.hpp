/*
 * Copyright 2011-2019 Arx Libertatis Team (see the AUTHORS file)
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
// Source: https://github.com/arx/ArxLibertatis/blob/5b95e4c5ca9d583f1b11c085326979772645e0f3/src/scene/LevelFormat.h
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#pragma once

#include "arx_pistoris/base/math.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace pistoris {

constexpr float kDlfVersion = 1.44f;
constexpr std::size_t kDlfMaxEntities = 0x10000;
constexpr std::size_t kDlfMaxAiNodes = 0x100000;
constexpr std::size_t kDlfMaxNodeLinks = 0x10000;
constexpr std::size_t kDlfMaxFogs = 0x10000;
constexpr std::size_t kDlfMaxPaths = 0x10000;
constexpr std::size_t kDlfMaxPathNodes = 0x100000;
constexpr std::size_t kDlfScenePathCapacity = 512;
constexpr std::size_t kDlfEntityClassPathCapacity = 512;
constexpr std::size_t kDlfZoneNameCapacity = 64;
constexpr std::size_t kDlfZoneAmbianceCapacity = 128;
constexpr std::size_t kDlfPathNameCapacity = 64;

namespace dlf {

struct PlayerSpawn {
  ArxVector3 position = {};
  ArxAngle angle = {};
};

struct Entity {
  char class_path[kDlfEntityClassPathCapacity] = {};
  std::int32_t ident = -1;
  ArxVector3 position = {};
  ArxAngle angle = {};
};

struct Fog {
  ArxVector3 position = {};
  ArxColor3 color = {};
  float size = 0.0f;
  bool directional = false;
  float scale = 0.0f;
  ArxAngle angle = {};
  float speed = 0.0f;
  float rotate_speed = 0.0f;
  std::int32_t lifetime_ms = 0;
  float frequency = 0.0f;
};

struct ZoneAmbiance {
  char name[kDlfZoneAmbianceCapacity] = {};
  float volume = 100.0f;
};

struct Zone {
  char name[kDlfZoneNameCapacity] = {};
  ArxVector3 position = {};
  std::vector<ArxVector3> points;
  std::int32_t height = 0;
  std::optional<ArxColor3> color;
  std::optional<float> farclip;
  std::optional<ZoneAmbiance> ambiance;
};

enum class PathNodeType : std::uint8_t {
  kStandard = 0,
  kBezier = 1,
  kControlPoint = 2,
};

struct PathNode {
  ArxVector3 relative_position = {};
  PathNodeType type = PathNodeType::kStandard;
  std::uint32_t time_ms = 0;
};

struct Path {
  char name[kDlfPathNameCapacity] = {};
  ArxVector3 position = {};
  std::vector<PathNode> nodes;
};

struct Data {
  float version = kDlfVersion;
  PlayerSpawn player_spawn;
  char scene_path[kDlfScenePathCapacity] = {};
  std::vector<Entity> entities;
  std::vector<Fog> fogs;
  std::vector<Zone> zones;
  std::vector<Path> paths;
};

}  // namespace dlf

using Dlf = dlf::Data;

}  // namespace pistoris
