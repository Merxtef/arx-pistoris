// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level/images.hpp"

#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "level/validation.h"
#include "modules/loading_screen.h"
#include "modules/minimap.h"
#include "utils/encoded_image.h"
#include "utils/log.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace pistoris::level_images {
namespace {

constexpr double kMiniOffsetArxX = 65.0;
constexpr double kMiniOffsetArxY = 62.0;
constexpr ArxColor3 kDefaultMinimapBorderColor{1.0f, 1.0f, 1.0f};

std::optional<image::Format> imageFormat(ImageFormat format) noexcept {
  switch (format) {
    case ImageFormat::kPng:
      return image::Format::kPng;
    case ImageFormat::kBmp:
      return image::Format::kBmp;
    case ImageFormat::kTga:
      return image::Format::kTga;
    case ImageFormat::kUnknown:
    case ImageFormat::kJpeg:
      return std::nullopt;
  }
  return std::nullopt;
}

ArxReturnCode scaleOffset(ArxVector2 value, double x_scale, double y_scale, ArxVector2& out) noexcept {
  const double x = static_cast<double>(value.x) * x_scale;
  const double y = static_cast<double>(value.y) * y_scale;
  constexpr double kLowest = std::numeric_limits<float>::lowest();
  constexpr double kHighest = std::numeric_limits<float>::max();
  if (!std::isfinite(x) || !std::isfinite(y) || x < kLowest || x > kHighest || y < kLowest || y > kHighest)
    return ARX_INVALID_OPTIONS;
  out = {static_cast<float>(x), static_cast<float>(y)};
  return ARX_OK;
}

ArxReturnCode reproject(std::span<const std::uint8_t> encoded_image, ArxVector2 source_projection_offset,
                        const minimap::RenderOptions& options, std::vector<std::uint8_t>& out) {
  minimap::RenderInfo info;
  const ArxReturnCode rc =
      level_validation::minimapError(minimap::reproject(encoded_image, source_projection_offset, options, out, &info));
  if (rc != ARX_OK) return rc;
  if (info.invisible) {
    log(ARX_LOG_WARN, "Level minimap is outside the requested projection; output omitted");
  } else if (info.cropped) {
    log(ARX_LOG_WARN, "Level minimap was cropped by the requested projection");
  } else if (info.padded) {
    log(ARX_LOG_DEBUG, "Level minimap was padded for the requested projection");
  }
  return ARX_OK;
}

}  // namespace

ArxReturnCode projectionOffsetFromMiniOffset(ArxVector2 mini_offset, ArxVector2& out) noexcept {
  return scaleOffset(mini_offset, kMiniOffsetArxX, kMiniOffsetArxY, out);
}

ArxReturnCode miniOffsetFromProjectionOffset(ArxVector2 projection_offset, ArxVector2& out) noexcept {
  return scaleOffset(projection_offset, 1.0 / kMiniOffsetArxX, 1.0 / kMiniOffsetArxY, out);
}

ArxReturnCode projectionOffsetForLevel(std::uint32_t level, ArxVector2 stored_mini_offset, ArxVector2& out) noexcept {
  switch (level) {
    case 0:
      stored_mini_offset = {0.0f, -0.5f};
      break;
    case 1:
      stored_mini_offset = {};
      break;
    case 14:
      stored_mini_offset = {130.0f, 0.0f};
      break;
    case 15:
      stored_mini_offset = {31.0f, -3.5f};
      break;
    default:
      break;
  }
  return projectionOffsetFromMiniOffset(stored_mini_offset, out);
}

ArxReturnCode reprojectMinimap(std::span<const std::uint8_t> encoded_image, const MinimapReprojectionOptions& options,
                               std::vector<std::uint8_t>& out) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const std::optional<image::Format> format = imageFormat(options.format);
    if (!format) return ARX_INVALID_OPTIONS;
    std::optional<ArxColor3> border_color;
    switch (options.mode) {
      case MinimapRenderMode::kPlain:
        if (options.border_color) return ARX_INVALID_OPTIONS;
        break;
      case MinimapRenderMode::kGame:
        border_color = options.border_color.value_or(kDefaultMinimapBorderColor);
        break;
      default:
        return ARX_INVALID_OPTIONS;
    }
    return reproject(encoded_image,
                     options.source_projection_offset,
                     {.projection_offset = options.target_projection_offset,
                      .fill_color = options.fill_color,
                      .border_color = border_color,
                      .format = *format},
                     out);
  });
}

ArxReturnCode renderLoadingScreen(std::span<const std::uint8_t> encoded_image,
                                  const LoadingScreenRenderOptions& options, std::vector<std::uint8_t>& out) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const std::optional<image::Format> format = imageFormat(options.format);
    if (!format) return ARX_INVALID_OPTIONS;
    loading_screen::Layout layout = loading_screen::Layout::kOriginal;
    switch (options.layout) {
      case LoadingScreenLayout::kOriginal:
        layout = loading_screen::Layout::kOriginal;
        break;
      case LoadingScreenLayout::kNormal:
        layout = loading_screen::Layout::kNormal;
        break;
      case LoadingScreenLayout::kFullscreen:
        layout = loading_screen::Layout::kFullscreen;
        break;
      default:
        return ARX_INVALID_OPTIONS;
    }
    return level_validation::loadingScreenError(loading_screen::render(encoded_image, layout, *format, out));
  });
}

}  // namespace pistoris::level_images
