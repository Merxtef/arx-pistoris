// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.h"
#include "arx_pistoris/pistoris_types.h"

#include "api/c/level_internal.h"
#include "api/c_api_internal.h"

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_generate_nav_surface(ArxLevel* level,
                                                      const ArxLevelNavSurfaceGenOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.generateNavSurface();
    return level->value.generateNavSurface(pistoris::c_api::navGenOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_set_nav_surface_from_floor(ArxLevel* level,
                                                            const ArxLevelNavSurfaceSourceOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.setNavSurfaceFromFloor();
    return level->value.setNavSurfaceFromFloor(pistoris::c_api::navSourceOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_prune_nav_surface_islands(ArxLevel* level,
                                                           const ArxLevelNavSurfacePruneOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.pruneNavSurfaceIslands();
    return level->value.pruneNavSurfaceIslands(pistoris::c_api::navPruneOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_generate_anchors(ArxLevel* level, const ArxLevelAnchorGenOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.generateAnchors();
    return level->value.generateAnchors(pistoris::c_api::anchorGenOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_generate_anchor_connections(
    ArxLevel* level, const ArxLevelAnchorConnectionGenOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.generateAnchorConnections();
    return level->value.generateAnchorConnections(pistoris::c_api::anchorConnectionOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_prune_anchor_islands(ArxLevel* level,
                                                      const ArxLevelAnchorPruneOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.pruneAnchorIslands();
    return level->value.pruneAnchorIslands(pistoris::c_api::anchorPruneOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_generate_room_distances(ArxLevel* level,
                                                         const ArxLevelRoomDistanceGenOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.generateRoomDistances();
    return level->value.generateRoomDistances(pistoris::c_api::roomDistanceOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_generate_static_lighting(ArxLevel* level,
                                                          const ArxLevelStaticLightingGenOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.generateStaticLighting();
    return level->value.generateStaticLighting(pistoris::c_api::lightingOptions(*options));
  });
}

// NOLINTEND(readability-identifier-naming)
