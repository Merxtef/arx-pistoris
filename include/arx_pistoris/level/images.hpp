// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::level_images {

enum class LoadingScreenLayout : std::uint8_t {
  // Preserve source dimensions
  kOriginal,
  // 320 x 390 game loading screen
  kNormal,
  // 640 x 480 fullscreen loading screen
  kFullscreen,
};

struct MinimapReprojectionOptions {
  // Arx-unit projection offset represented by the input image
  ArxVector2 source_projection_offset{};
  // Arx-unit projection offset represented by the output image
  ArxVector2 target_projection_offset{};
  // Padding color
  ArxColor3 fill_color{};
};

struct GameMinimapReprojectionOptions {
  // Arx-unit projection offset represented by the input image
  ArxVector2 source_projection_offset{};
  // Arx-unit projection offset represented by the output image
  ArxVector2 target_projection_offset{};
  // Padding color
  ArxColor3 fill_color{};
  ArxColor3 border_color{1.0f, 1.0f, 1.0f};
};

[[nodiscard]] ArxReturnCode projectionOffsetFromMiniOffset(ArxVector2 mini_offset, ArxVector2& out) noexcept;
[[nodiscard]] ArxReturnCode miniOffsetFromProjectionOffset(ArxVector2 projection_offset, ArxVector2& out) noexcept;
[[nodiscard]] ArxReturnCode reprojectMinimapPng(std::span<const std::uint8_t> encoded_image,
                                                const MinimapReprojectionOptions& options,
                                                std::vector<std::uint8_t>& out) noexcept;
[[nodiscard]] ArxReturnCode reprojectGameMinimapPng(std::span<const std::uint8_t> encoded_image,
                                                    const GameMinimapReprojectionOptions& options,
                                                    std::vector<std::uint8_t>& out) noexcept;
[[nodiscard]] ArxReturnCode renderLoadingScreenPng(std::span<const std::uint8_t> encoded_image,
                                                   LoadingScreenLayout layout, std::vector<std::uint8_t>& out) noexcept;

}  // namespace pistoris::level_images
