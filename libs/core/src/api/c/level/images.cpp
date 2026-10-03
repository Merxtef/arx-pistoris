// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level/images.h"

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level/images.hpp"

#include "api/c/internal.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_level_image_projection_offset_from_mini_offset(ArxVector2 mini_offset,
                                                                          ArxVector2* out_projection_offset,
                                                                          ArxError* error) noexcept {
  if (!out_projection_offset) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_projection_offset = {};
  return pistoris::c_api::publishCode(
      pistoris::level_images::projectionOffsetFromMiniOffset(mini_offset, *out_projection_offset), error);
}

ArxReturnCode arx_pistoris_level_image_mini_offset_from_projection_offset(ArxVector2 projection_offset,
                                                                          ArxVector2* out_mini_offset,
                                                                          ArxError* error) noexcept {
  if (!out_mini_offset) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_mini_offset = {};
  return pistoris::c_api::publishCode(
      pistoris::level_images::miniOffsetFromProjectionOffset(projection_offset, *out_mini_offset), error);
}

ArxReturnCode arx_pistoris_level_image_projection_offset_for_level(uint32_t level, ArxVector2 stored_mini_offset,
                                                                   ArxVector2* out_projection_offset,
                                                                   ArxError* error) noexcept {
  if (!out_projection_offset) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_projection_offset = {};
  return pistoris::c_api::publishCode(
      pistoris::level_images::projectionOffsetForLevel(level, stored_mini_offset, *out_projection_offset), error);
}

ArxReturnCode arx_pistoris_level_image_reproject_minimap(ArxEncodedImageView encoded_image,
                                                         const ArxLevelMinimapReprojectionOptions* options,
                                                         uint8_t** out_data, size_t* out_size,
                                                         ArxError* error) noexcept {
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if ((!encoded_image.data && encoded_image.size != 0) || !out_data || !out_size)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard(error, [&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = pistoris::level_images::reprojectMinimap(
        std::span(encoded_image.data, encoded_image.size),
        {.mode = static_cast<pistoris::level_images::MinimapRenderMode>(options->mode),
         .source_projection_offset = options->source_projection_offset,
         .target_projection_offset = options->target_projection_offset,
         .fill_color = options->fill_color,
         .border_color = options->has_border_color != 0 ? std::optional{options->border_color} : std::nullopt,
         .format = static_cast<pistoris::ImageFormat>(options->format)},
        rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_level_image_render_loading_screen(ArxEncodedImageView encoded_image,
                                                             const ArxLevelLoadingScreenRenderOptions* options,
                                                             uint8_t** out_data, size_t* out_size,
                                                             ArxError* error) noexcept {
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if ((!encoded_image.data && encoded_image.size != 0) || !out_data || !out_size)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard(error, [&] {
    std::vector<std::uint8_t> rendered;
    const ArxReturnCode rc = pistoris::level_images::renderLoadingScreen(
        std::span(encoded_image.data, encoded_image.size),
        {.layout = static_cast<pistoris::level_images::LoadingScreenLayout>(options->layout),
         .format = static_cast<pistoris::ImageFormat>(options->format)},
        rendered);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(rendered), out_data, out_size);
  });
}

// NOLINTEND(readability-identifier-naming)
