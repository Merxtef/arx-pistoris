// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level/images.hpp"

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "level/validation.h"
#include "modules/loading_screen.h"
#include "modules/minimap.h"
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

ArxReturnCode reprojectMinimap(std::span<const std::uint8_t> encoded_image, ArxVector2 source_projection_offset,
                               const minimap::RenderOptions& options, std::vector<std::uint8_t>& out) {
  minimap::RenderInfo info;
  const ArxReturnCode rc = level_validation::minimapError(
      minimap::reprojectPng(encoded_image, source_projection_offset, options, out, &info));
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

ArxReturnCode reprojectMinimapPng(std::span<const std::uint8_t> encoded_image,
                                  const MinimapReprojectionOptions& options, std::vector<std::uint8_t>& out) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return reprojectMinimap(encoded_image,
                            options.source_projection_offset,
                            {.projection_offset = options.target_projection_offset,
                             .fill_color = options.fill_color,
                             .border_color = std::nullopt},
                            out);
  });
}

ArxReturnCode reprojectGameMinimapPng(std::span<const std::uint8_t> encoded_image,
                                      const GameMinimapReprojectionOptions& options,
                                      std::vector<std::uint8_t>& out) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    return reprojectMinimap(encoded_image,
                            options.source_projection_offset,
                            {.projection_offset = options.target_projection_offset,
                             .fill_color = options.fill_color,
                             .border_color = options.border_color},
                            out);
  });
}

ArxReturnCode renderLoadingScreenPng(std::span<const std::uint8_t> encoded_image, LoadingScreenLayout layout,
                                     std::vector<std::uint8_t>& out) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    switch (layout) {
      case LoadingScreenLayout::kOriginal:
        return level_validation::loadingScreenError(loading_screen::transcodePng(encoded_image, out));
      case LoadingScreenLayout::kNormal:
        return level_validation::loadingScreenError(loading_screen::renderPng(encoded_image, false, out));
      case LoadingScreenLayout::kFullscreen:
        return level_validation::loadingScreenError(loading_screen::renderPng(encoded_image, true, out));
    }
    return ARX_INVALID_OPTIONS;
  });
}

}  // namespace pistoris::level_images
