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

#include "arx_pistoris/native/llf.hpp"

#include <cstdint>

namespace pistoris::native_lighting {

struct Light {
  ArxVector3 position = {};
  ArxColor3 color = {};
  float fallstart = 0.0f;
  float fallend = 0.0f;
  float intensity = 0.0f;
  float legacy_i = 0.0f;
  ArxColor3 flicker = {};
  float effect_radius = 0.0f;
  float effect_frequency = 0.0f;
  float effect_size = 0.0f;
  float effect_speed = 0.0f;
  float flare_size = 0.0f;
  float fpad[24] = {};
  std::int32_t flags = 0;
  std::int32_t lpad[31] = {};
};
static_assert(sizeof(Light) == 296);

struct Header {
  std::int32_t num_values = 0;
  std::int32_t view_mode = 0;
  std::int32_t mode_light = 0;
  std::int32_t pad = 0;
};
static_assert(sizeof(Header) == 16);

llf::Light decode(const Light& source);
Light encode(const llf::Light& source);
ArxColor3 decodeColor(std::uint32_t bgra);
std::uint32_t encodeColor(const ArxColor3& color);

}  // namespace pistoris::native_lighting
