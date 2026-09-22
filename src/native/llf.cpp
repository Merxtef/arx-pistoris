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

#include "native/llf.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/runtime/types.h"

#include "native/lighting_layout.h"
#include "native/write_metadata.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstdint>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

constexpr char kLlfIdentity[] = "DANAE_LLH_FILE";

struct NativeHeader {
  float version = kLlfVersion;
  char ident[16] = {};
  char lastuser[256] = {};
  std::int32_t time = 0;
  std::int32_t num_lights = 0;
  std::int32_t num_shadow_polys = 0;
  std::int32_t num_ignored_polys = 0;
  std::int32_t num_background_polys = 0;
  std::int32_t pad[256] = {};
  float fpad[256] = {};
  char cpad[4096] = {};
  std::int32_t bpad[256] = {};
};
static_assert(sizeof(NativeHeader) == 7464);

bool unitColor(const ArxColor3& color) {
  return math::finite(color) && color.r >= 0.0f && color.r <= 1.0f && color.g >= 0.0f && color.g <= 1.0f &&
         color.b >= 0.0f && color.b <= 1.0f;
}

bool countFits(std::int32_t value, std::size_t max) { return value >= 0 && static_cast<std::size_t>(value) <= max; }

}  // namespace

ArxReturnCode loadLlf(llf::Data* data, ReadCursor& cursor) {
  if (!data) return ARX_INVALID_DATA_POINTER;

  NativeHeader header;
  cursor.read(header);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (std::memcmp(header.ident, kLlfIdentity, sizeof(kLlfIdentity)) != 0) return ARX_INVALID_IDENTIFIER;
  if (!countFits(header.num_lights, kLlfMaxLights)) return ARX_LLF_BAD_LIGHT_COUNT;

  llf::Data tmp;
  tmp.version = header.version;
  if (!tryResize(tmp.lights, static_cast<std::size_t>(header.num_lights))) return ARX_BAD_ALLOC;
  for (llf::Light& light : tmp.lights) {
    native_lighting::Light native;
    cursor.read(native);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    light = native_lighting::decode(native);
  }

  native_lighting::Header lighting;
  cursor.read(lighting);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (!countFits(lighting.num_values, kLlfMaxColors)) return ARX_LLF_BAD_BAKED_COLOR_COUNT;
  if (!tryResize(tmp.colors, static_cast<std::size_t>(lighting.num_values))) return ARX_BAD_ALLOC;
  for (ArxColor3& color : tmp.colors) {
    std::uint32_t packed = 0;
    cursor.read(packed);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    color = native_lighting::decodeColor(packed);
  }

  ArxReturnCode rc = validateLlf(&tmp);
  if (rc != ARX_OK) return rc;

  log(ARX_LOG_INFO, "LLF loaded: {} lights, {} colors", tmp.lights.size(), tmp.colors.size());
  *data = std::move(tmp);
  return ARX_OK;
}

ArxReturnCode saveLlf(const llf::Data* data, std::string_view signer, WriteCursor& cursor) {
  ArxReturnCode rc = validateLlf(data);
  if (rc != ARX_OK) return rc;

  log(ARX_LOG_INFO, "LLF saving: {} lights, {} colors", data->lights.size(), data->colors.size());

  NativeHeader header;
  NativeWriteMetadata metadata;
  rc = nativeWriteMetadata(signer, metadata);
  if (rc != ARX_OK) return rc;
  header.version = data->version;
  std::memcpy(header.lastuser, metadata.last_user.data(), metadata.last_user.size());
  header.time = metadata.modified_at;
  header.num_lights = static_cast<std::int32_t>(data->lights.size());
  std::memcpy(header.ident, kLlfIdentity, sizeof(kLlfIdentity));
  cursor.write(header);
  for (const llf::Light& light : data->lights) cursor.write(native_lighting::encode(light));

  native_lighting::Header lighting;
  lighting.num_values = static_cast<std::int32_t>(data->colors.size());
  cursor.write(lighting);
  for (const ArxColor3& color : data->colors) cursor.write(native_lighting::encodeColor(color));

  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode validateLlf(const llf::Data* data) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (data->lights.size() > kLlfMaxLights) return ARX_LLF_BAD_LIGHT_COUNT;
  if (data->colors.size() > kLlfMaxColors) return ARX_LLF_BAD_BAKED_COLOR_COUNT;

  for (const llf::Light& light : data->lights) {
    if (!math::finite(light.position)) return ARX_LLF_BAD_LIGHT_POSITION;
    if (!unitColor(light.color)) return ARX_LLF_BAD_LIGHT_COLOR;
    if (!math::finite(light.fallstart) || light.fallstart < 0.0f || !math::finite(light.fallend) ||
        light.fallend < light.fallstart)
      return ARX_LLF_BAD_LIGHT_FALLOFF;
    if (!math::finite(light.intensity) || light.intensity < 0.0f) return ARX_LLF_BAD_LIGHT_INTENSITY;
    if (!math::finite(light.flicker) || !math::finite(light.effect_radius) || !math::finite(light.effect_frequency) ||
        !math::finite(light.effect_size) || !math::finite(light.effect_speed) || !math::finite(light.flare_size))
      return ARX_LLF_BAD_LIGHT_EFFECT;
    if ((light.flags & ~kLightFlagsAll) != 0) return ARX_LLF_BAD_LIGHT_FLAGS;
  }

  for (const ArxColor3& color : data->colors)
    if (!unitColor(color)) return ARX_LLF_BAD_BAKED_COLOR;

  return ARX_OK;
}

}  // namespace pistoris
