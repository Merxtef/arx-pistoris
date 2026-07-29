// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/conversion/level/api.h"
#include "arx/conversion/level/internal.h"
#include "arx/dlf.h"
#include "arx/fts.h"
#include "arx/llf.h"
#include "level/data.h"
#include "level/level.h"
#include "modules/lights.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "utils/log.h"

#include <cstddef>
#include <format>
#include <span>
#include <string>
#include <utility>

namespace pistoris::arx_level_conversion {
namespace {

void logBuildWarnings(const NativeBuildWarnings& warnings) {
  const bool has_repairs =
      warnings.regenerated_normals != 0 || warnings.normalized_normals != 0 || warnings.discarded_faces != 0 ||
      warnings.skipped_room0_faces != 0 || warnings.skipped_room0_portals != 0 || warnings.collapsed_zone_points != 0 ||
      warnings.empty_zone_ambiances != 0 || warnings.equal_light_falloffs != 0 ||
      warnings.discarded_out_of_bounds_anchors != 0 || warnings.room_distance_one_sided != 0 ||
      warnings.room_distance_asymmetric != 0 || warnings.room_distance_mismatched_portals != 0 ||
      warnings.room_distance_room0_positive != 0 || warnings.room_distance_reassigned_endpoints != 0 ||
      warnings.room_distance_discarded_positive_directions != 0 ||
      warnings.room_distance_direct_portal_fallbacks != 0 || warnings.room_distance_missing_connections != 0 ||
      warnings.renamed_duplicate_portals != 0 || warnings.renamed_duplicate_lights != 0 ||
      warnings.renamed_duplicate_fogs != 0 || warnings.renamed_duplicate_zones != 0 ||
      warnings.renamed_duplicate_paths != 0 || warnings.room_distance_no_positive_real_pairs;
  if (has_repairs) {
    std::string warning = "Native -> Level repairs:";
    if (warnings.regenerated_normals != 0)
      warning += std::format(" {} corner normal(s) regenerated;", warnings.regenerated_normals);
    if (warnings.normalized_normals != 0)
      warning += std::format(" {} corner normal(s) normalized;", warnings.normalized_normals);
    if (warnings.discarded_faces != 0)
      warning += std::format(" {} degenerate face(s) discarded;", warnings.discarded_faces);
    if (warnings.skipped_room0_faces != 0)
      warning += std::format(" {} room-0 face(s) skipped;", warnings.skipped_room0_faces);
    if (warnings.skipped_room0_portals != 0)
      warning += std::format(" {} room-0 portal(s) skipped;", warnings.skipped_room0_portals);
    if (warnings.collapsed_zone_points != 0)
      warning += std::format(" {} consecutive zone point(s) collapsed;", warnings.collapsed_zone_points);
    if (warnings.empty_zone_ambiances != 0)
      warning += std::format(" {} empty zone ambiance override(s) ignored;", warnings.empty_zone_ambiances);
    if (warnings.equal_light_falloffs != 0)
      warning += std::format(" {} equal light falloff range(s) narrowed;", warnings.equal_light_falloffs);
    if (warnings.discarded_out_of_bounds_anchors != 0)
      warning +=
          std::format(" {} anchor(s) outside native X/Z bounds discarded;", warnings.discarded_out_of_bounds_anchors);
    if (warnings.room_distance_one_sided != 0)
      warning += std::format(" {} one-sided room distance pair(s) imported;", warnings.room_distance_one_sided);
    if (warnings.room_distance_asymmetric != 0)
      warning += std::format(" {} positive room distance pair(s) had asymmetric distances;",
                             warnings.room_distance_asymmetric);
    if (warnings.room_distance_mismatched_portals != 0)
      warning += std::format(" {} room distance portal assignment mismatch(es) resolved;",
                             warnings.room_distance_mismatched_portals);
    if (warnings.room_distance_room0_positive != 0)
      warning +=
          std::format(" {} positive room-0 room distance value(s) ignored;", warnings.room_distance_room0_positive);
    if (warnings.room_distance_reassigned_endpoints != 0)
      warning += std::format(" {} room distance endpoint(s) snapped to the closest incident portal;",
                             warnings.room_distance_reassigned_endpoints);
    if (warnings.room_distance_discarded_positive_directions != 0)
      warning += std::format(
          " {} positive room distance direction(s) discarded because an endpoint could not be "
          "mapped to an incident portal;",
          warnings.room_distance_discarded_positive_directions);
    if (warnings.room_distance_direct_portal_fallbacks != 0)
      warning += std::format(" {} non-positive room distance pair(s) assigned to a fallback direct portal;",
                             warnings.room_distance_direct_portal_fallbacks);
    if (warnings.room_distance_missing_connections != 0)
      warning += std::format(" {} room distance pair(s) imported without a usable portal connection;",
                             warnings.room_distance_missing_connections);
    if (warnings.renamed_duplicate_portals != 0)
      warning += std::format(" {} duplicate portal name(s) renamed;", warnings.renamed_duplicate_portals);
    if (warnings.renamed_duplicate_lights != 0)
      warning += std::format(" {} duplicate light name(s) renamed;", warnings.renamed_duplicate_lights);
    if (warnings.renamed_duplicate_fogs != 0)
      warning += std::format(" {} duplicate fog name(s) renamed;", warnings.renamed_duplicate_fogs);
    if (warnings.renamed_duplicate_zones != 0)
      warning += std::format(" {} duplicate zone name(s) renamed;", warnings.renamed_duplicate_zones);
    if (warnings.renamed_duplicate_paths != 0)
      warning += std::format(" {} duplicate path name(s) renamed;", warnings.renamed_duplicate_paths);
    if (warnings.room_distance_no_positive_real_pairs)
      warning += " room distance matrix has no positive real-room distances;";
    warning.pop_back();
    log(ARX_LOG_WARN, warning);
  }
  if (warnings.outside_geometry_anchors != 0)
    log(ARX_LOG_WARN,
        std::format("Native -> Level: {} anchor(s) outside referenced geometry bounds retained",
                    warnings.outside_geometry_anchors));
}

}  // namespace

ArxReturnCode buildLevel(const NativeLevelSource& source, LevelModules& out, LevelValidationState* out_validation) {
  const fts::Data& fts = source.fts;
  ArxReturnCode rc = validateFts(&fts);
  if (rc != ARX_OK) return rc;
  if (source.llf) {
    rc = validateLlf(source.llf);
    if (rc != ARX_OK) return rc;
  }
  if (source.dlf) {
    rc = validateDlf(source.dlf);
    if (rc != ARX_OK) return rc;
  }
  LevelModules tmp;
  NativeBuildWarnings warnings;
  const std::size_t expected_colors = expectedFtsColorCount(fts);
  const bool use_llf_colors = source.llf && source.llf->colors.size() == expected_colors;
  const std::span<const ArxColor3> colors =
      use_llf_colors ? std::span<const ArxColor3>(source.llf->colors) : std::span<const ArxColor3>();

  rc = buildFtsModules(fts, colors, tmp, warnings);
  if (rc != ARX_OK) return rc;
  if (source.llf) buildLlfModules(fts, *source.llf, tmp.lighting, warnings);
  if (source.dlf) {
    rc = buildDlfModules(*source.dlf, fts.scene.Mscenepos, tmp.scene, warnings);
    if (rc != ARX_OK) return rc;
  }
  warnings.renamed_duplicate_portals += rooms::makePortalNamesUnique(tmp.rooms.portals);
  warnings.renamed_duplicate_lights += lights::makeLightNamesUnique(tmp.lighting.lights);
  warnings.renamed_duplicate_fogs += scene::makeFogNamesUnique(tmp.scene.fogs);
  warnings.renamed_duplicate_zones += scene::makeZoneNamesUnique(tmp.scene.zones);
  if (source.llf && !use_llf_colors) {
    log(ARX_LOG_WARN,
        std::format("FTS/LLF color count mismatch: expected {}, got {}; ignored all baked colors and preserved {} "
                    "light(s)",
                    expected_colors,
                    source.llf->colors.size(),
                    source.llf->lights.size()));
  }

  logBuildWarnings(warnings);
  LevelValidationState validation;
  rc = validateLevelModules(tmp, validation);
  if (rc != ARX_OK) return rc;
  out = std::move(tmp);
  if (out_validation) *out_validation = validation;
  return ARX_OK;
}

}  // namespace pistoris::arx_level_conversion
