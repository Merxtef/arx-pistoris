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

#include "arx/dlf.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/lighting_io.h"
#include "arx/llf.h"
#include "arx/resource_path.h"
#include "arx/write_metadata.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/mem_utils.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

constexpr char kDlfIdentity[] = "DANAE_FILE";
constexpr std::int16_t kZoneFlagAmbiance = 0x2;
constexpr std::int16_t kZoneFlagColor = 0x4;
constexpr std::int16_t kZoneFlagFarclip = 0x8;
constexpr std::int32_t kFogDirectional = 0x1;

struct NativeHeader {
  float version = kDlfVersion;
  char ident[16] = {};
  char lastuser[256] = {};
  std::int32_t time = 0;
  ArxVector3 pos_edit = {};
  ArxAngle angle_edit = {};
  std::int32_t num_scenes = 0;
  std::int32_t num_entities = 0;
  std::int32_t num_nodes = 0;
  std::int32_t num_node_links = 0;
  std::int32_t num_zones = 0;
  std::int32_t lighting = 0;
  std::int32_t bpad1[256] = {};
  std::int32_t num_lights = 0;
  std::int32_t num_fogs = 0;
  std::int32_t num_background_polys = 0;
  std::int32_t num_ignored_polys = 0;
  std::int32_t num_child_polys = 0;
  std::int32_t num_paths = 0;
  std::int32_t pad[250] = {};
  ArxVector3 offset = {};
  float fpad[253] = {};
  char cpad[4096] = {};
  std::int32_t bpad2[256] = {};
};
static_assert(sizeof(NativeHeader) == 8520);

struct NativeScene {
  char name[512] = {};
  std::int32_t pad[16] = {};
  float fpad[16] = {};
};
static_assert(sizeof(NativeScene) == 640);

struct NativeEntity {
  char name[512] = {};
  ArxVector3 position = {};
  ArxAngle angle = {};
  std::int32_t ident = -1;
  std::int32_t flags = 0;
  std::int32_t pad[14] = {};
  float fpad[16] = {};
};
static_assert(sizeof(NativeEntity) == 664);

struct NativeFog {
  ArxVector3 position = {};
  ArxColor3 color = {};
  float size = 0.0f;
  std::int32_t special = 0;
  float scale = 0.0f;
  ArxVector3 move = {};
  ArxAngle angle = {};
  float speed = 0.0f;
  float rotate_speed = 0.0f;
  std::int32_t lifetime_ms = 0;
  std::int32_t blend = 0;
  float frequency = 0.0f;
  float fpad[32] = {};
  std::int32_t lpad[32] = {};
  char cpad[256] = {};
};
static_assert(sizeof(NativeFog) == 592);

struct NativePath {
  char name[64] = {};
  std::int16_t idx = 0;
  std::int16_t flags = 0;
  ArxVector3 init_position = {};
  ArxVector3 position = {};
  std::int32_t num_pathways = 0;
  ArxColor3 color = {};
  float farclip = 0.0f;
  float reverb = 0.0f;
  float ambiance_volume = 0.0f;
  float fpad[26] = {};
  std::int32_t height = 0;
  std::int32_t lpad[31] = {};
  char ambiance[128] = {};
  char cpad[128] = {};
};
static_assert(sizeof(NativePath) == 608);

struct NativePathNode {
  ArxVector3 relative_position = {};
  std::int32_t flag = 0;
  std::uint32_t time_ms = 0;
  float fpad[2] = {};
  std::int32_t lpad[2] = {};
  char cpad[32] = {};
};
static_assert(sizeof(NativePathNode) == 68);

bool finite(float value) { return std::isfinite(value); }

bool finite(const ArxVector3& value) { return finite(value.x) && finite(value.y) && finite(value.z); }

bool finite(const ArxAngle& value) { return finite(value.pitch) && finite(value.yaw) && finite(value.roll); }

bool finite(const ArxColor3& value) { return finite(value.r) && finite(value.g) && finite(value.b); }

template <std::size_t N>
std::string fixedString(const char (&value)[N]) {
  const void* end = std::memchr(value, '\0', N);
  const std::size_t size =
      end ? static_cast<std::size_t>(static_cast<const char*>(end) - value) : static_cast<std::size_t>(N);
  return std::string(value, size);
}

template <std::size_t N>
bool copyFixed(char (&destination)[N], std::string_view source) {
  if (source.empty() || source.size() >= N) return false;
  std::memcpy(destination, source.data(), source.size());
  destination[source.size()] = '\0';
  return true;
}

template <std::size_t N>
bool copyFixedAllowEmpty(char (&destination)[N], std::string_view source) {
  if (source.size() >= N) return false;
  std::memcpy(destination, source.data(), source.size());
  destination[source.size()] = '\0';
  return true;
}

void lowerAscii(std::string& value) {
  for (char& c : value)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
}

bool countFits(std::int32_t count, std::size_t max) { return count >= 0 && static_cast<std::size_t>(count) <= max; }

bool productFits(std::size_t a, std::size_t b, std::size_t& result) {
  if (a != 0 && b > std::numeric_limits<std::size_t>::max() / a) return false;
  result = a * b;
  return true;
}

dlf::PathNodeType nodeType(std::int32_t value) {
  if (value == 1) return dlf::PathNodeType::kBezier;
  if (value == 2) return dlf::PathNodeType::kControlPoint;
  return dlf::PathNodeType::kStandard;
}

std::int32_t nativeNodeType(dlf::PathNodeType value) {
  switch (value) {
    case dlf::PathNodeType::kStandard:
      return 0;
    case dlf::PathNodeType::kBezier:
      return 1;
    case dlf::PathNodeType::kControlPoint:
      return 2;
  }
  return 0;
}

ArxReturnCode readEmbeddedLighting(const NativeHeader& header, std::optional<llf::Data>* output, ReadCursor& cursor) {
  llf::Data lighting;
  const bool has_colors = header.lighting != 0;
  const bool has_lights = header.num_lights != 0;

  if (has_colors) {
    native_lighting::Header lighting_header;
    cursor.read(lighting_header);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    if (!countFits(lighting_header.num_values, kLlfMaxColors)) return ARX_DLF_BAD_EMBEDDED_COLOR_COUNT;
    const std::size_t color_count = static_cast<std::size_t>(lighting_header.num_values);
    if (!output) {
      cursor.skip(color_count * sizeof(std::uint32_t));
      if (!cursor) return ARX_UNEXPECTED_EOF;
    } else {
      if (!tryResize(lighting.colors, color_count)) return ARX_BAD_ALLOC;
      for (ArxColor3& color : lighting.colors) {
        std::uint32_t packed = 0;
        cursor.read(packed);
        if (!cursor) return ARX_UNEXPECTED_EOF;
        color = native_lighting::decodeColor(packed);
      }
    }
  }

  const std::size_t light_count = static_cast<std::size_t>(header.num_lights);
  if (!output) {
    cursor.skip(light_count * sizeof(native_lighting::Light));
    if (!cursor) return ARX_UNEXPECTED_EOF;
  } else {
    if (!tryResize(lighting.lights, light_count)) return ARX_BAD_ALLOC;
    for (llf::Light& light : lighting.lights) {
      native_lighting::Light native;
      cursor.read(native);
      if (!cursor) return ARX_UNEXPECTED_EOF;
      light = native_lighting::decode(native);
    }
  }

  if (!output) return ARX_OK;
  if (!has_colors && !has_lights) {
    output->reset();
    return ARX_OK;
  }

  ArxReturnCode rc = validateLlf(&lighting);
  if (rc != ARX_OK) {
    log(ARX_LOG_WARN,
        std::format("DLF embedded lighting discarded as semantically invalid (code {})", static_cast<int>(rc)));
    output->reset();
    return ARX_OK;
  }

  *output = std::move(lighting);
  return ARX_OK;
}

ArxReturnCode readEntities(const NativeHeader& header, dlf::Data& data, ReadCursor& cursor,
                           std::uint64_t& normalized_legacy_teo) {
  if (!tryResize(data.entities, static_cast<std::size_t>(header.num_entities))) return ARX_BAD_ALLOC;
  for (dlf::Entity& entity : data.entities) {
    NativeEntity native;
    cursor.read(native);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    const std::string source = fixedString(native.name);
    std::string_view removed_extension;
    if (!normalizeEntityClassPath(source, entity.class_path, removed_extension)) return ARX_DLF_BAD_ENTITY_CLASS_PATH;
    if (isLegacyTeoExtension(removed_extension)) ++normalized_legacy_teo;
    entity.ident = native.ident;
    entity.position = native.position;
    entity.angle = native.angle;
  }
  return ARX_OK;
}

ArxReturnCode readFogs(const NativeHeader& header, dlf::Data& data, ReadCursor& cursor) {
  if (!tryResize(data.fogs, static_cast<std::size_t>(header.num_fogs))) return ARX_BAD_ALLOC;
  for (dlf::Fog& fog : data.fogs) {
    NativeFog native;
    cursor.read(native);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    fog = {
        native.position,
        native.color,
        native.size,
        (native.special & kFogDirectional) != 0,
        native.scale,
        native.angle,
        native.speed,
        native.rotate_speed,
        native.lifetime_ms,
        native.frequency,
    };
  }
  return ARX_OK;
}

ArxReturnCode readPaths(const NativeHeader& header, dlf::Data& data, ReadCursor& cursor) {
  if (!tryReserve(data.paths, static_cast<std::size_t>(header.num_paths)) ||
      !tryReserve(data.zones, static_cast<std::size_t>(header.num_paths)))
    return ARX_BAD_ALLOC;

  for (std::int32_t i = 0; i < header.num_paths; ++i) {
    NativePath native;
    cursor.read(native);
    if (!cursor) return ARX_UNEXPECTED_EOF;
    if (!countFits(native.num_pathways, kDlfMaxPathNodes))
      return native.height != 0 ? ARX_DLF_BAD_ZONE_POINT_COUNT : ARX_DLF_BAD_PATH_NODE_COUNT;

    std::vector<NativePathNode> nodes;
    if (!tryResize(nodes, static_cast<std::size_t>(native.num_pathways))) return ARX_BAD_ALLOC;
    cursor.readArray(nodes);
    if (!cursor) return ARX_UNEXPECTED_EOF;

    std::string name = fixedString(native.name);
    lowerAscii(name);
    if (native.height != 0) {
      dlf::Zone zone;
      zone.name = std::move(name);
      zone.position = native.position;
      zone.height = native.height;
      if (!tryReserve(zone.points, nodes.size())) return ARX_BAD_ALLOC;
      for (const NativePathNode& node : nodes) zone.points.push_back(node.relative_position);
      if ((native.flags & kZoneFlagColor) != 0) zone.color = native.color;
      if ((native.flags & kZoneFlagFarclip) != 0) zone.farclip = native.farclip;
      if ((native.flags & kZoneFlagAmbiance) != 0) {
        std::string ambiance = fixedString(native.ambiance);
        lowerAscii(ambiance);
        zone.ambiance =
            dlf::ZoneAmbiance{std::move(ambiance), native.ambiance_volume <= 1.0f ? 100.0f : native.ambiance_volume};
      }
      data.zones.push_back(std::move(zone));
    } else {
      dlf::Path path;
      path.name = std::move(name);
      path.position = native.position;
      if (!tryReserve(path.nodes, nodes.size())) return ARX_BAD_ALLOC;
      for (const NativePathNode& node : nodes)
        path.nodes.push_back({node.relative_position, nodeType(node.flag), node.time_ms});
      if (!path.nodes.empty()) {
        path.nodes.front().relative_position = {};
        path.nodes.front().time_ms = 0;
      }
      data.paths.push_back(std::move(path));
    }
  }
  return ARX_OK;
}

ArxReturnCode writeEmbeddedLighting(const llf::Data* lighting, WriteCursor& cursor) {
  if (!lighting) return ARX_OK;

  if (!lighting->colors.empty()) {
    native_lighting::Header header;
    header.num_values = static_cast<std::int32_t>(lighting->colors.size());
    cursor.write(header);
    for (const ArxColor3& color : lighting->colors) cursor.write(native_lighting::encodeColor(color));
  }

  for (const llf::Light& light : lighting->lights) cursor.write(native_lighting::encode(light));
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode writeEntities(const dlf::Data& data, WriteCursor& cursor) {
  for (const dlf::Entity& entity : data.entities) {
    NativeEntity native;
    if (!copyFixed(native.name, entity.class_path)) return ARX_DLF_BAD_ENTITY_CLASS_PATH;
    native.position = entity.position;
    native.angle = entity.angle;
    native.ident = entity.ident;
    cursor.write(native);
  }
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode writeFogs(const dlf::Data& data, WriteCursor& cursor) {
  for (const dlf::Fog& fog : data.fogs) {
    NativeFog native;
    native.position = fog.position;
    native.color = fog.color;
    native.size = fog.size;
    native.special = fog.directional ? kFogDirectional : 0;
    native.scale = fog.scale;
    native.angle = fog.angle;
    native.speed = fog.speed;
    native.rotate_speed = fog.rotate_speed;
    native.lifetime_ms = fog.lifetime_ms;
    native.frequency = fog.frequency;
    cursor.write(native);
  }
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode writeZone(const dlf::Zone& zone, WriteCursor& cursor) {
  NativePath native;
  if (!copyFixed(native.name, zone.name)) return ARX_DLF_BAD_ZONE_NAME;
  native.position = zone.position;
  native.num_pathways = static_cast<std::int32_t>(zone.points.size());
  native.height = zone.height;
  const auto& color = zone.color;
  if (color) {
    native.flags |= kZoneFlagColor;
    native.color = *color;
  }
  const auto& farclip = zone.farclip;
  if (farclip) {
    native.flags |= kZoneFlagFarclip;
    native.farclip = *farclip;
  }
  const auto& ambiance = zone.ambiance;
  if (ambiance) {
    native.flags |= kZoneFlagAmbiance;
    if (!copyFixedAllowEmpty(native.ambiance, ambiance->name)) return ARX_DLF_BAD_ZONE_AMBIANCE;
    native.ambiance_volume = ambiance->volume;
  }
  cursor.write(native);
  for (const ArxVector3& point : zone.points) {
    NativePathNode node;
    node.relative_position = point;
    cursor.write(node);
  }
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode writePath(const dlf::Path& path, WriteCursor& cursor) {
  NativePath native;
  if (!copyFixed(native.name, path.name)) return ARX_DLF_BAD_PATH_NAME;
  native.position = path.position;
  native.num_pathways = static_cast<std::int32_t>(path.nodes.size());
  cursor.write(native);
  for (const dlf::PathNode& source : path.nodes) {
    NativePathNode node;
    node.relative_position = source.relative_position;
    node.flag = nativeNodeType(source.type);
    node.time_ms = source.time_ms;
    cursor.write(node);
  }
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

}  // namespace

bool validDlfScenePath(std::string_view scene_path) noexcept {
  return !scene_path.empty() && scene_path.size() < sizeof(NativeScene::name) &&
         scene_path.find('\0') == std::string_view::npos;
}

ArxReturnCode loadDlf(dlf::Data* data, std::optional<llf::Data>* embedded_lighting, ReadCursor& cursor) {
  if (!data) return ARX_INVALID_DATA_POINTER;

  NativeHeader header;
  cursor.read(header);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  if (header.version != kDlfVersion) return ARX_DLF_BAD_VERSION;
  if (std::memcmp(header.ident, kDlfIdentity, sizeof(kDlfIdentity)) != 0) return ARX_INVALID_IDENTIFIER;
  if (header.num_scenes != 1) return ARX_DLF_BAD_SCENE_COUNT;
  if (!countFits(header.num_entities, kDlfMaxEntities)) return ARX_DLF_BAD_ENTITY_COUNT;
  if (!countFits(header.num_nodes, kDlfMaxAiNodes)) return ARX_DLF_BAD_AI_NODE_COUNT;
  if (!countFits(header.num_node_links, kDlfMaxNodeLinks)) return ARX_DLF_BAD_AI_NODE_LINK_COUNT;
  if (!countFits(header.num_lights, kLlfMaxLights)) return ARX_DLF_BAD_EMBEDDED_LIGHT_COUNT;
  if (!countFits(header.num_fogs, kDlfMaxFogs)) return ARX_DLF_BAD_FOG_COUNT;
  if (!countFits(header.num_paths, kDlfMaxPaths)) return ARX_DLF_BAD_PATH_RECORD_COUNT;

  dlf::Data tmp;
  tmp.version = header.version;
  tmp.player_spawn.position = header.pos_edit;
  tmp.player_spawn.angle = header.angle_edit;

  NativeScene scene;
  cursor.read(scene);
  if (!cursor) return ARX_UNEXPECTED_EOF;
  tmp.scene_path = fixedString(scene.name);

  std::uint64_t normalized_legacy_teo = 0;
  ArxReturnCode rc = readEntities(header, tmp, cursor, normalized_legacy_teo);
  if (rc != ARX_OK) return rc;

  std::optional<llf::Data> lighting;
  rc = readEmbeddedLighting(header, embedded_lighting ? &lighting : nullptr, cursor);
  if (rc != ARX_OK) return rc;

  rc = readFogs(header, tmp, cursor);
  if (rc != ARX_OK) return rc;

  std::size_t link_bytes = 0;
  std::size_t node_size = 0;
  if (!productFits(static_cast<std::size_t>(header.num_node_links), 64, link_bytes) ||
      link_bytes > std::numeric_limits<std::size_t>::max() - 204 ||
      !productFits(static_cast<std::size_t>(header.num_nodes), 204 + link_bytes, node_size))
    return ARX_DLF_BAD_AI_NODE_COUNT;
  cursor.skip(node_size);
  if (!cursor) return ARX_UNEXPECTED_EOF;

  rc = readPaths(header, tmp, cursor);
  if (rc != ARX_OK) return rc;
  rc = validateDlf(&tmp);
  if (rc != ARX_OK) return rc;

  if (normalized_legacy_teo != 0) {
    log(ARX_LOG_WARN, std::format("DLF import: normalized {} legacy .teo entity class path(s)", normalized_legacy_teo));
  }
  log(ARX_LOG_INFO,
      std::format("DLF loaded: {} entities, {} fogs, {} zones, {} paths",
                  tmp.entities.size(),
                  tmp.fogs.size(),
                  tmp.zones.size(),
                  tmp.paths.size()));
  *data = std::move(tmp);
  if (embedded_lighting) *embedded_lighting = std::move(lighting);
  return ARX_OK;
}

ArxReturnCode saveDlf(const dlf::Data* data, const llf::Data* embedded_lighting, std::string_view signer,
                      WriteCursor& cursor) {
  ArxReturnCode rc = validateDlf(data);
  if (rc != ARX_OK) return rc;
  if (embedded_lighting) {
    rc = validateLlf(embedded_lighting);
    if (rc != ARX_OK) return rc;
  }

  NativeHeader header;
  const NativeWriteMetadata metadata = nativeWriteMetadata(signer);
  std::memcpy(header.ident, kDlfIdentity, sizeof(kDlfIdentity));
  std::memcpy(header.lastuser, metadata.last_user.data(), metadata.last_user.size());
  header.time = metadata.modified_at;
  header.pos_edit = data->player_spawn.position;
  header.angle_edit = data->player_spawn.angle;
  header.num_scenes = 1;
  header.num_entities = static_cast<std::int32_t>(data->entities.size());
  header.num_zones = static_cast<std::int32_t>(data->zones.size());
  header.lighting = embedded_lighting && !embedded_lighting->colors.empty() ? 1 : 0;
  header.num_lights = embedded_lighting ? static_cast<std::int32_t>(embedded_lighting->lights.size()) : 0;
  header.num_fogs = static_cast<std::int32_t>(data->fogs.size());
  header.num_paths = static_cast<std::int32_t>(data->zones.size() + data->paths.size());
  cursor.write(header);

  NativeScene scene;
  if (!copyFixed(scene.name, data->scene_path)) return ARX_DLF_BAD_SCENE_PATH;
  cursor.write(scene);

  rc = writeEntities(*data, cursor);
  if (rc != ARX_OK) return rc;
  rc = writeEmbeddedLighting(embedded_lighting, cursor);
  if (rc != ARX_OK) return rc;
  rc = writeFogs(*data, cursor);
  if (rc != ARX_OK) return rc;
  for (const dlf::Zone& zone : data->zones) {
    rc = writeZone(zone, cursor);
    if (rc != ARX_OK) return rc;
  }
  for (const dlf::Path& path : data->paths) {
    rc = writePath(path, cursor);
    if (rc != ARX_OK) return rc;
  }

  log(ARX_LOG_INFO,
      std::format("DLF saving: {} entities, {} fogs, {} zones, {} paths",
                  data->entities.size(),
                  data->fogs.size(),
                  data->zones.size(),
                  data->paths.size()));
  return cursor ? ARX_OK : ARX_BAD_ALLOC;
}

ArxReturnCode validateDlf(const dlf::Data* data) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (data->version != kDlfVersion) return ARX_DLF_BAD_VERSION;
  if (!validDlfScenePath(data->scene_path)) return ARX_DLF_BAD_SCENE_PATH;
  if (!finite(data->player_spawn.position) || !finite(data->player_spawn.angle)) return ARX_DLF_BAD_PLAYER_SPAWN;
  if (data->entities.size() > kDlfMaxEntities) return ARX_DLF_BAD_ENTITY_COUNT;
  if (data->fogs.size() > kDlfMaxFogs) return ARX_DLF_BAD_FOG_COUNT;
  if (data->zones.size() > kDlfMaxPaths || data->paths.size() > kDlfMaxPaths - data->zones.size())
    return ARX_DLF_BAD_PATH_RECORD_COUNT;

  for (const dlf::Entity& entity : data->entities) {
    std::string normalized;
    std::string_view removed_extension;
    if (entity.class_path.size() >= sizeof(NativeEntity::name) ||
        !normalizeEntityClassPath(entity.class_path, normalized, removed_extension) || normalized != entity.class_path)
      return ARX_DLF_BAD_ENTITY_CLASS_PATH;
    if (!finite(entity.position)) return ARX_DLF_BAD_ENTITY_POSITION;
    if (!finite(entity.angle)) return ARX_DLF_BAD_ENTITY_ANGLE;
  }

  for (const dlf::Fog& fog : data->fogs) {
    if (!finite(fog.position)) return ARX_DLF_BAD_FOG_POSITION;
    if (!finite(fog.color)) return ARX_DLF_BAD_FOG_COLOR;
    if (!finite(fog.angle)) return ARX_DLF_BAD_FOG_ANGLE;
    if (!finite(fog.size) || !finite(fog.scale) || !finite(fog.speed) || !finite(fog.rotate_speed) ||
        !finite(fog.frequency))
      return ARX_DLF_BAD_FOG_EFFECT;
  }

  for (const dlf::Zone& zone : data->zones) {
    if (zone.name.empty() || zone.name.size() >= sizeof(NativePath::name)) return ARX_DLF_BAD_ZONE_NAME;
    if (!finite(zone.position)) return ARX_DLF_BAD_ZONE_POSITION;
    if (zone.points.size() < 3 || zone.points.size() > kDlfMaxPathNodes) return ARX_DLF_BAD_ZONE_POINT_COUNT;
    if (zone.height == 0) return ARX_DLF_BAD_ZONE_HEIGHT;
    for (const ArxVector3& point : zone.points)
      if (!finite(point)) return ARX_DLF_BAD_ZONE_POINT;
    if (const auto& color = zone.color; color.has_value() && !finite(*color)) return ARX_DLF_BAD_ZONE_COLOR;
    if (const auto& farclip = zone.farclip; farclip.has_value() && !finite(*farclip)) return ARX_DLF_BAD_ZONE_FARCLIP;
    if (const auto& ambiance = zone.ambiance; ambiance.has_value()) {
      const dlf::ZoneAmbiance& value = *ambiance;
      if (value.name.size() >= sizeof(NativePath::ambiance) || !finite(value.volume)) return ARX_DLF_BAD_ZONE_AMBIANCE;
    }
  }

  for (const dlf::Path& path : data->paths) {
    if (path.name.empty() || path.name.size() >= sizeof(NativePath::name)) return ARX_DLF_BAD_PATH_NAME;
    if (!finite(path.position)) return ARX_DLF_BAD_PATH_POSITION;
    if (path.nodes.empty() || path.nodes.size() > kDlfMaxPathNodes) return ARX_DLF_BAD_PATH_NODE_COUNT;
    for (const dlf::PathNode& node : path.nodes) {
      if (!finite(node.relative_position)) return ARX_DLF_BAD_PATH_NODE_POSITION;
      switch (node.type) {
        case dlf::PathNodeType::kStandard:
        case dlf::PathNodeType::kBezier:
        case dlf::PathNodeType::kControlPoint:
          break;
        default:
          return ARX_DLF_BAD_PATH_NODE_TYPE;
      }
    }
    if (path.nodes.front().relative_position.x != 0.0f || path.nodes.front().relative_position.y != 0.0f ||
        path.nodes.front().relative_position.z != 0.0f || path.nodes.front().time_ms != 0)
      return ARX_DLF_BAD_PATH_FIRST_NODE;
  }

  return ARX_OK;
}

}  // namespace pistoris
