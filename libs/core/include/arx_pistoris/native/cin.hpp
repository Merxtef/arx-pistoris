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
// Sources:
// https://github.com/arx/ArxLibertatis/blob/1dfb6c4006fc63e837c074ade5d2305063b2bc41/src/cinematic/CinematicFormat.h
// https://github.com/arx/ArxLibertatis/blob/1dfb6c4006fc63e837c074ade5d2305063b2bc41/src/cinematic/CinematicLoad.cpp
/*
 * Modified for arx-pistoris:
 * Copyright (C) 2026 Merxtef
 */

#pragma once

#include "arx_pistoris/base/math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pistoris {

inline constexpr std::array<char, 4> kCinMagic = {'K', 'F', 'A', '\0'};
constexpr std::int32_t kCinVersion175 = 0x0001004b;
constexpr std::int32_t kCinVersion = 0x0001004c;
constexpr std::size_t kCinMaxSounds = 256;

namespace cin {

struct Bitmap {
  std::int32_t subdivision_scale = 1;
  std::string path;
};

struct Sound {
  std::string path;
  bool speech = false;
};

struct Light {
  ArxVector3 position = {};
  float fall_in = 0.0f;
  float fall_out = 0.0f;
  ArxColor3 color = {};
  float intensity = -1.0f;
  float random_intensity = 0.0f;
};

struct Keyframe {
  std::int32_t frame = 0;
  std::int32_t bitmap = 0;
  std::uint32_t effects = 0;
  std::int16_t interpolation = 1;
  std::int16_t crossfade = 0;
  ArxVector3 camera_position = {};
  float camera_roll = 0.0f;
  std::uint32_t color = 0xffffffffU;
  std::uint32_t secondary_color = 0xffffffffU;
  std::uint32_t flash_color = 0xffffffffU;
  float flash_decay = 0.0f;
  Light light;
  ArxVector3 bitmap_position = {};
  float bitmap_roll = 0.0f;
  float outgoing_speed = 1.0f;
  std::int32_t sound = -1;
};

struct Data {
  std::vector<Bitmap> bitmaps;
  std::vector<Sound> sounds;
  std::int32_t end_frame = 0;
  float fps = 25.0f;
  std::vector<Keyframe> keyframes;
};

}  // namespace cin

using Cin = cin::Data;

}  // namespace pistoris
