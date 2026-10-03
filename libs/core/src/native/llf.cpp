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
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "native/binary_location.h"
#include "native/lighting_layout.h"
#include "native/write_metadata.h"
#include "utils/container_allocation.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>
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

LlfBinaryResult<llf::Data> loadLlf(ReadCursor& cursor, NativeBinaryRegion region) {
  auto fail = [&](ArxReturnCode code, const CursorLocation& location) {
    return api_detail::llfBinaryFailure<llf::Data>(code, native_binary::location<LlfElement>(location, region));
  };
  auto fail_cursor = [&](ArxReturnCode code) {
    return api_detail::llfBinaryFailure<llf::Data>(code, native_binary::location<LlfElement>(cursor, region));
  };
  auto member_location = [](CursorLocation location, std::size_t offset, std::string_view field) {
    location.offset += offset;
    location.field = field;
    return location;
  };

  NativeHeader header;
  const CursorLocation header_location = cursor.mark(LlfElement::kHeader, "header");
  cursor.read(header);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (std::memcmp(header.ident, kLlfIdentity, sizeof(kLlfIdentity)) != 0)
    return fail(ARX_INVALID_IDENTIFIER, member_location(header_location, offsetof(NativeHeader, ident), "identity"));
  if (!countFits(header.num_lights, kLlfMaxLights))
    return fail(ARX_LLF_BAD_LIGHT_COUNT,
                member_location(header_location, offsetof(NativeHeader, num_lights), "light_count"));

  llf::Data tmp;
  tmp.version = header.version;
  if (!tryResize(tmp.lights, static_cast<std::size_t>(header.num_lights)))
    return fail(ARX_BAD_ALLOC, member_location(header_location, offsetof(NativeHeader, num_lights), "light_count"));
  for (std::size_t index = 0; index < tmp.lights.size(); ++index) {
    llf::Light& light = tmp.lights[index];
    native_lighting::Light native;
    cursor.locate(LlfElement::kLight, "light", index);
    cursor.read(native);
    if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
    light = native_lighting::decode(native);
  }

  native_lighting::Header lighting;
  const CursorLocation lighting_location = cursor.mark(LlfElement::kHeader, "lighting_header");
  cursor.read(lighting);
  if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
  if (!countFits(lighting.num_values, kLlfMaxColors))
    return fail(ARX_LLF_BAD_BAKED_COLOR_COUNT, member_location(lighting_location, 0, "baked_color_count"));
  if (!tryResize(tmp.colors, static_cast<std::size_t>(lighting.num_values)))
    return fail(ARX_BAD_ALLOC, member_location(lighting_location, 0, "baked_color_count"));
  for (std::size_t index = 0; index < tmp.colors.size(); ++index) {
    ArxColor3& color = tmp.colors[index];
    std::uint32_t packed = 0;
    cursor.locate(LlfElement::kVertexColor, "vertex_color", index);
    cursor.read(packed);
    if (!cursor) return fail_cursor(ARX_UNEXPECTED_EOF);
    color = native_lighting::decodeColor(packed);
  }

  LlfLocation validation_location;
  if (const ArxReturnCode code = validateLlf(&tmp, &validation_location); code != ARX_OK)
    return api_detail::llfBinaryFailure<llf::Data>(code, native_binary::semanticLocation(validation_location, region));

  log(ARX_LOG_INFO, "LLF loaded: {} lights, {} colors", tmp.lights.size(), tmp.colors.size());
  return LlfBinaryResult<llf::Data>::success(std::move(tmp));
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

ArxReturnCode validateLlf(const llf::Data* data, LlfLocation* failure_location) {
  auto fail = [failure_location](ArxReturnCode code,
                                 LlfElement element = LlfElement::kHeader,
                                 std::size_t index = kNoElementIndex,
                                 std::string field = {}) {
    if (failure_location) *failure_location = {.element = element, .index = index, .field = std::move(field)};
    return code;
  };
  if (!data) return fail(ARX_INVALID_DATA_POINTER);
  if (data->lights.size() > kLlfMaxLights)
    return fail(ARX_LLF_BAD_LIGHT_COUNT, LlfElement::kHeader, kNoElementIndex, "lights");
  if (data->colors.size() > kLlfMaxColors)
    return fail(ARX_LLF_BAD_BAKED_COLOR_COUNT, LlfElement::kHeader, kNoElementIndex, "colors");

  for (std::size_t index = 0; index < data->lights.size(); ++index) {
    const llf::Light& light = data->lights[index];
    if (!math::finite(light.position)) return fail(ARX_LLF_BAD_LIGHT_POSITION, LlfElement::kLight, index, "position");
    if (!unitColor(light.color)) return fail(ARX_LLF_BAD_LIGHT_COLOR, LlfElement::kLight, index, "color");
    if (!math::finite(light.fallstart) || light.fallstart < 0.0f)
      return fail(ARX_LLF_BAD_LIGHT_FALLOFF, LlfElement::kLight, index, "fallstart");
    if (!math::finite(light.fallend) || light.fallend < light.fallstart)
      return fail(ARX_LLF_BAD_LIGHT_FALLOFF, LlfElement::kLight, index, "fallend");
    if (!math::finite(light.intensity) || light.intensity < 0.0f)
      return fail(ARX_LLF_BAD_LIGHT_INTENSITY, LlfElement::kLight, index, "intensity");
    if (!math::finite(light.flicker)) return fail(ARX_LLF_BAD_LIGHT_EFFECT, LlfElement::kLight, index, "flicker");
    if (!math::finite(light.effect_radius))
      return fail(ARX_LLF_BAD_LIGHT_EFFECT, LlfElement::kLight, index, "effect_radius");
    if (!math::finite(light.effect_frequency))
      return fail(ARX_LLF_BAD_LIGHT_EFFECT, LlfElement::kLight, index, "effect_frequency");
    if (!math::finite(light.effect_size))
      return fail(ARX_LLF_BAD_LIGHT_EFFECT, LlfElement::kLight, index, "effect_size");
    if (!math::finite(light.effect_speed))
      return fail(ARX_LLF_BAD_LIGHT_EFFECT, LlfElement::kLight, index, "effect_speed");
    if (!math::finite(light.flare_size)) return fail(ARX_LLF_BAD_LIGHT_EFFECT, LlfElement::kLight, index, "flare_size");
    if ((light.flags & ~kLightFlagsAll) != 0) return fail(ARX_LLF_BAD_LIGHT_FLAGS, LlfElement::kLight, index, "flags");
  }

  for (std::size_t index = 0; index < data->colors.size(); ++index) {
    const ArxColor3& color = data->colors[index];
    if (!unitColor(color)) {
      const auto valid_component = [](float value) { return math::finite(value) && value >= 0.0f && value <= 1.0f; };
      const std::string field = !valid_component(color.r) ? "r" : !valid_component(color.g) ? "g" : "b";
      return fail(ARX_LLF_BAD_BAKED_COLOR, LlfElement::kVertexColor, index, field);
    }
  }

  return ARX_OK;
}

}  // namespace pistoris
