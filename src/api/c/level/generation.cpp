// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.h"

#include "api/c/internal.h"
#include "api/c/level/internal.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

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

ArxReturnCode arx_pistoris_level_generate_minimap(ArxLevel* level,
                                                  const ArxLevelMinimapGenerationOptions* options) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] {
    if (!options) return level->value.generateMinimap();
    return level->value.generateMinimap(pistoris::c_api::minimapGenerationOptions(*options));
  });
}

ArxReturnCode arx_pistoris_level_render_minimap_png(const ArxLevel* level, const ArxLevelMinimapRenderOptions* options,
                                                    uint8_t** out_data, size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = level->value.renderMinimapPng(
        {.projection_offset = options->projection_offset, .fill_color = options->fill_color}, rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_render_game_minimap_png(const ArxLevel* level,
                                                         const ArxLevelGameMinimapRenderOptions* options,
                                                         uint8_t** out_data, size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = level->value.renderGameMinimapPng({.projection_offset = options->projection_offset,
                                                                .fill_color = options->fill_color,
                                                                .border_color = options->border_color},
                                                               rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_render_compact_minimap_png(const ArxLevel* level, ArxVector2* out_projection_offset,
                                                            uint8_t** out_data, size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_projection_offset || !out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_projection_offset = {};
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    ArxVector2 projection_offset{};
    const ArxReturnCode rc = level->value.renderCompactMinimapPng(projection_offset, rendered);
    if (rc != ARX_OK) return rc;
    const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
    if (publish_rc == ARX_OK) *out_projection_offset = projection_offset;
    return publish_rc;
  });
}

ArxReturnCode arx_pistoris_level_render_loading_screen_png(const ArxLevel* level, uint8_t** out_data,
                                                           size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = level->value.renderLoadingScreenPng(rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_render_fullscreen_loading_screen_png(const ArxLevel* level, uint8_t** out_data,
                                                                      size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = level->value.renderFullscreenLoadingScreenPng(rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_transcode_loading_screen_png(const ArxLevel* level, uint8_t** out_data,
                                                              size_t* out_size) noexcept {
  if (!level) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = level->value.transcodeLoadingScreenPng(rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
