// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level/images.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level/images.hpp"

#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_image_projection_offset_from_mini_offset(ArxVector2 mini_offset,
                                                                          ArxVector2* out_projection_offset) noexcept {
  if (!out_projection_offset) return ARX_INVALID_DATA_POINTER;
  *out_projection_offset = {};
  return pistoris::level_images::projectionOffsetFromMiniOffset(mini_offset, *out_projection_offset);
}

ArxReturnCode arx_pistoris_level_image_mini_offset_from_projection_offset(ArxVector2 projection_offset,
                                                                          ArxVector2* out_mini_offset) noexcept {
  if (!out_mini_offset) return ARX_INVALID_DATA_POINTER;
  *out_mini_offset = {};
  return pistoris::level_images::miniOffsetFromProjectionOffset(projection_offset, *out_mini_offset);
}

ArxReturnCode arx_pistoris_level_image_reproject_minimap_png(ArxEncodedImageView encoded_image,
                                                             const ArxLevelMinimapReprojectionOptions* options,
                                                             uint8_t** out_data, size_t* out_size) noexcept {
  if (!options) return ARX_INVALID_OPTIONS;
  if ((!encoded_image.data && encoded_image.size != 0) || !out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc =
        pistoris::level_images::reprojectMinimapPng(std::span(encoded_image.data, encoded_image.size),
                                                    {.source_projection_offset = options->source_projection_offset,
                                                     .target_projection_offset = options->target_projection_offset,
                                                     .fill_color = options->fill_color},
                                                    rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_image_reproject_game_minimap_png(ArxEncodedImageView encoded_image,
                                                                  const ArxLevelGameMinimapReprojectionOptions* options,
                                                                  uint8_t** out_data, size_t* out_size) noexcept {
  if (!options) return ARX_INVALID_OPTIONS;
  if ((!encoded_image.data && encoded_image.size != 0) || !out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc =
        pistoris::level_images::reprojectGameMinimapPng(std::span(encoded_image.data, encoded_image.size),
                                                        {.source_projection_offset = options->source_projection_offset,
                                                         .target_projection_offset = options->target_projection_offset,
                                                         .fill_color = options->fill_color,
                                                         .border_color = options->border_color},
                                                        rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_image_render_loading_screen_png(ArxEncodedImageView encoded_image,
                                                                 ArxLevelLoadingScreenLayout layout, uint8_t** out_data,
                                                                 size_t* out_size) noexcept {
  if ((!encoded_image.data && encoded_image.size != 0) || !out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  if (layout > ARX_LEVEL_LOADING_SCREEN_LAYOUT_FULLSCREEN) return ARX_INVALID_OPTIONS;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc =
        pistoris::level_images::renderLoadingScreenPng(std::span(encoded_image.data, encoded_image.size),
                                                       static_cast<pistoris::level_images::LoadingScreenLayout>(layout),
                                                       rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
