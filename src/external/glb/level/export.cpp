// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/runtime/types.h"

#include "../writer.h"
#include "api.h"
#include "coordinates.h"
#include "entities.h"
#include "external/glb/level/export/internal.h"
#include "external/glb/utils/texture.h"
#include "fogs.h"
#include "level/data.h"
#include "level/validation.h"
#include "minimap.h"
#include "palette.h"
#include "paths.h"
#include "player_spawn.h"
#include "utils/log.h"
#include "zones.h"

#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

using glb::Builder;
using glb_level_export::buildRoomProjection;
using glb_level_export::RoomProjection;
using glb_level_export::TextureRegistry;

ArxReturnCode exportLevelToGlb(const LevelModules& level, const ArxAabb& referenced_bounds,
                               const Level::GlbExportOptions& options,
                               std::span<const ModelModules* const> model_previews, ArxLevelModelPreviewReport* report,
                               std::vector<std::uint8_t>& out) {
  ArxLevelModelPreviewReport local_report{};
  TextureRegistry texture_registry;
  ArxReturnCode rc = buildTextureRegistry(level, texture_registry);
  if (rc != ARX_OK) return rc;

  Builder builder;
  glb_level::Palette palette(builder);
  rc = registerTextures(level, texture_registry, builder);
  if (rc != ARX_OK) return rc;
  rc = glb_level::configureGlbExportCoordinates(builder, options);
  if (rc != ARX_OK) return rc;
  const RoomProjection room_projection = buildRoomProjection(level);
  std::uint64_t nonstandard_transval = 0;
  rc = glb_level_export::exportRoomGeometry(
      level, referenced_bounds, texture_registry, room_projection, builder, nonstandard_transval);
  if (rc != ARX_OK) return rc;

  glb_level_export::exportPortals(level, referenced_bounds, room_projection, palette, builder);
  glb_level_export::exportNavigation(level, referenced_bounds, options, palette, builder);
  const std::uint64_t defaulted_light_fallstarts =
      glb_level_export::exportLights(level, referenced_bounds, options, builder);

  glb_level::exportPlayerSpawn(level, builder);
  rc = glb_level::exportEntities(level, referenced_bounds, model_previews, local_report, builder);
  if (rc != ARX_OK) return rc;
  rc = glb_level::exportZones(level, referenced_bounds, options, builder, palette);
  if (rc != ARX_OK) return rc;
  glb_level::exportPaths(level, referenced_bounds, builder);
  glb_level::exportFogs(level, referenced_bounds, options, builder);
  rc = glb_level::exportMinimap(level.minimap, referenced_bounds, builder);
  if (rc != ARX_OK) return rc;

  if (defaulted_light_fallstarts != 0 || room_projection.discarded_portals != 0 ||
      room_projection.discarded_positive_distances != 0)
    logLazy(ARX_LOG_WARN, [&] {
      std::string warning = "Level -> GLB repairs:";
      if (defaulted_light_fallstarts != 0)
        warning += std::format(" {} light fallstart value(s) defaulted;", defaulted_light_fallstarts);
      if (room_projection.discarded_portals != 0 || room_projection.discarded_positive_distances != 0)
        warning += std::format(
            " {} empty room(s), {} referencing portal(s), and {} positive room-distance pair(s) "
            "discarded;",
            room_projection.empty_rooms,
            room_projection.discarded_portals,
            room_projection.discarded_positive_distances);
      warning.pop_back();
      return warning;
    });
  if (nonstandard_transval != 0)
    log(ARX_LOG_WARN,
        "Level -> GLB: {} transparent face(s) use nonstandard Arx blend modes; raw transval is "
        "preserved in material names and previewed with alpha 1",
        nonstandard_transval);
  logUnusedTextures(level, texture_registry);
  rc = builder.write(out);
  if (rc == ARX_OK && report) *report = local_report;
  return rc;
}

}  // namespace pistoris
