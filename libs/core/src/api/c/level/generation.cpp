// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level/images.h"
#include "arx_pistoris/level/images.hpp"

#include "api/c/internal.h"
#include "api/c/level/internal.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_generate_nav_surface(ArxLevel* level, const ArxLevelNavSurfaceGenOptions* options,
                                                      ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.generateNavSurface(), error);
    return pistoris::c_api::publish(level->value.generateNavSurface(pistoris::c_api::navGenOptions(*options)), error);
  });
}

ArxReturnCode arx_pistoris_level_set_nav_surface_from_floor(ArxLevel* level,
                                                            const ArxLevelNavSurfaceSourceOptions* options,
                                                            ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.setNavSurfaceFromFloor(), error);
    return pistoris::c_api::publish(level->value.setNavSurfaceFromFloor(pistoris::c_api::navSourceOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_prune_nav_surface_islands(ArxLevel* level,
                                                           const ArxLevelNavSurfacePruneOptions* options,
                                                           ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.pruneNavSurfaceIslands(), error);
    return pistoris::c_api::publish(level->value.pruneNavSurfaceIslands(pistoris::c_api::navPruneOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_generate_anchors(ArxLevel* level, const ArxLevelAnchorGenOptions* options,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.generateAnchors(), error);
    return pistoris::c_api::publish(level->value.generateAnchors(pistoris::c_api::anchorGenOptions(*options)), error);
  });
}

ArxReturnCode arx_pistoris_level_generate_anchor_connections(ArxLevel* level,
                                                             const ArxLevelAnchorConnectionGenOptions* options,
                                                             ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.generateAnchorConnections(), error);
    return pistoris::c_api::publish(
        level->value.generateAnchorConnections(pistoris::c_api::anchorConnectionOptions(*options)), error);
  });
}

ArxReturnCode arx_pistoris_level_prune_anchor_islands(ArxLevel* level, const ArxLevelAnchorPruneOptions* options,
                                                      ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.pruneAnchorIslands(), error);
    return pistoris::c_api::publish(level->value.pruneAnchorIslands(pistoris::c_api::anchorPruneOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_generate_room_distances(ArxLevel* level, const ArxLevelRoomDistanceGenOptions* options,
                                                         ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.generateRoomDistances(), error);
    return pistoris::c_api::publish(level->value.generateRoomDistances(pistoris::c_api::roomDistanceOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_generate_static_lighting(ArxLevel* level,
                                                          const ArxLevelStaticLightingGenOptions* options,
                                                          ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.generateStaticLighting(), error);
    return pistoris::c_api::publish(level->value.generateStaticLighting(pistoris::c_api::lightingOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_generate_minimap(ArxLevel* level, const ArxLevelMinimapGenerationOptions* options,
                                                  ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::guard(error, [&] {
    if (!options) return pistoris::c_api::publish(level->value.generateMinimap(), error);
    return pistoris::c_api::publish(level->value.generateMinimap(pistoris::c_api::minimapGenerationOptions(*options)),
                                    error);
  });
}

ArxReturnCode arx_pistoris_level_render_minimap(const ArxLevel* level, const ArxLevelMinimapRenderOptions* options,
                                                ArxVector2* out_projection_offset, uint8_t** out_data, size_t* out_size,
                                                ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_projection_offset || !out_data || !out_size)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_projection_offset = {};
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard(error, [&] {
    pistoris::Level::MinimapRenderOptions converted{
        .mode = static_cast<pistoris::level_images::MinimapRenderMode>(options->mode),
        .projection_offset =
            options->has_projection_offset != 0 ? std::optional{options->projection_offset} : std::nullopt,
        .fill_color = options->fill_color,
        .border_color = options->has_border_color != 0 ? std::optional{options->border_color} : std::nullopt,
        .format = static_cast<pistoris::ImageFormat>(options->format),
    };
    auto rendered = level->value.renderMinimap(converted);
    if (!rendered) return pistoris::c_api::publish(rendered, error);
    const ArxVector2 projection_offset = rendered->projection_offset;
    const ArxReturnCode publish_rc =
        pistoris::c_api::publishBytes(std::move(rendered->encoded_image), out_data, out_size);
    if (publish_rc == ARX_OK) *out_projection_offset = projection_offset;
    return publish_rc;
  });
}

ArxReturnCode arx_pistoris_level_render_loading_screen(const ArxLevel* level,
                                                       const ArxLevelLoadingScreenRenderOptions* options,
                                                       uint8_t** out_data, size_t* out_size, ArxError* error) noexcept {
  if (!level) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_data || !out_size) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard(error, [&] {
    auto rendered = level->value.renderLoadingScreen(
        {.layout = static_cast<pistoris::level_images::LoadingScreenLayout>(options->layout),
         .format = static_cast<pistoris::ImageFormat>(options->format)});
    if (!rendered) return pistoris::c_api::publish(rendered, error);
    return pistoris::c_api::publishBytes(std::move(*rendered), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
