// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_LEVEL_IMAGES_H
#define ARX_PISTORIS_LEVEL_IMAGES_H

#include "arx_pistoris/base/abi.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"

#include <stddef.h>
#include <stdint.h>

// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef uint8_t ArxLevelLoadingScreenLayout;
enum {
  // Preserve source dimensions
  ARX_LEVEL_LOADING_SCREEN_LAYOUT_ORIGINAL = 0,
  // 320 x 390 game loading screen
  ARX_LEVEL_LOADING_SCREEN_LAYOUT_NORMAL,
  // 640 x 480 fullscreen loading screen
  ARX_LEVEL_LOADING_SCREEN_LAYOUT_FULLSCREEN,
};

typedef uint8_t ArxLevelMinimapRenderMode;
enum {
  ARX_LEVEL_MINIMAP_RENDER_PLAIN = 0,
  ARX_LEVEL_MINIMAP_RENDER_GAME,
};

typedef struct ArxLevelMinimapReprojectionOptions {
  ArxLevelMinimapRenderMode mode;
  // Arx-unit projection offset represented by the input image
  ArxVector2 source_projection_offset;
  // Arx-unit projection offset represented by the output image
  ArxVector2 target_projection_offset;
  // Padding color
  ArxColor3 fill_color;
  uint8_t has_border_color;
  ArxColor3 border_color;
  ArxImageFormat format;
} ArxLevelMinimapReprojectionOptions;

#define ARX_LEVEL_MINIMAP_REPROJECTION_OPTIONS_INIT \
  {ARX_LEVEL_MINIMAP_RENDER_PLAIN,                  \
   {0.0f, 0.0f},                                    \
   {0.0f, 0.0f},                                    \
   {0.0f, 0.0f, 0.0f},                              \
   0U,                                              \
   {1.0f, 1.0f, 1.0f},                              \
   ARX_IMAGE_FORMAT_PNG}

typedef struct ArxLevelLoadingScreenRenderOptions {
  ArxLevelLoadingScreenLayout layout;
  ArxImageFormat format;
} ArxLevelLoadingScreenRenderOptions;

#define ARX_LEVEL_LOADING_SCREEN_RENDER_OPTIONS_INIT {ARX_LEVEL_LOADING_SCREEN_LAYOUT_ORIGINAL, ARX_IMAGE_FORMAT_PNG}

ARX_EXTERN_C_BEGIN

ARX_API ArxReturnCode arx_pistoris_level_image_projection_offset_from_mini_offset(ArxVector2 mini_offset,
                                                                                  ArxVector2* out_projection_offset,
                                                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_image_mini_offset_from_projection_offset(ArxVector2 projection_offset,
                                                                                  ArxVector2* out_mini_offset,
                                                                                  ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_image_projection_offset_for_level(uint32_t level,
                                                                           ArxVector2 stored_mini_offset,
                                                                           ArxVector2* out_projection_offset,
                                                                           ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_image_reproject_minimap(ArxEncodedImageView encoded_image,
                                                                 const ArxLevelMinimapReprojectionOptions* options,
                                                                 uint8_t** out_data, size_t* out_size,
                                                                 ArxError* error) ARX_NOEXCEPT;
ARX_API ArxReturnCode arx_pistoris_level_image_render_loading_screen(ArxEncodedImageView encoded_image,
                                                                     const ArxLevelLoadingScreenRenderOptions* options,
                                                                     uint8_t** out_data, size_t* out_size,
                                                                     ArxError* error) ARX_NOEXCEPT;

ARX_EXTERN_C_END

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_LEVEL_IMAGES_H */
