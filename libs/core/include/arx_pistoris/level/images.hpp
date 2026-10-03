// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace pistoris::level_images {

enum class MinimapRenderMode : std::uint8_t {
  kPlain,
  kGame,
};

enum class LoadingScreenLayout : std::uint8_t {
  // Preserve source dimensions
  kOriginal,
  // 320 x 390 game loading screen
  kNormal,
  // 640 x 480 fullscreen loading screen
  kFullscreen,
};

struct MinimapReprojectionOptions {
  MinimapRenderMode mode = MinimapRenderMode::kPlain;
  // Arx-unit projection offset represented by the input image
  ArxVector2 source_projection_offset{};
  // Arx-unit projection offset represented by the output image
  ArxVector2 target_projection_offset{};
  // Padding color
  ArxColor3 fill_color{};
  // Only valid in game mode; empty selects white
  std::optional<ArxColor3> border_color;
  ImageFormat format = ImageFormat::kPng;
};

struct LoadingScreenRenderOptions {
  LoadingScreenLayout layout = LoadingScreenLayout::kOriginal;
  ImageFormat format = ImageFormat::kPng;
};

[[nodiscard]] ArxReturnCode projectionOffsetFromMiniOffset(ArxVector2 mini_offset, ArxVector2& out) noexcept;
[[nodiscard]] ArxReturnCode miniOffsetFromProjectionOffset(ArxVector2 projection_offset, ArxVector2& out) noexcept;
// Applies target-level engine overrides before converting mini-offset units to Arx units
[[nodiscard]] ArxReturnCode projectionOffsetForLevel(std::uint32_t level, ArxVector2 stored_mini_offset,
                                                     ArxVector2& out) noexcept;
[[nodiscard]] ArxReturnCode reprojectMinimap(std::span<const std::uint8_t> encoded_image,
                                             const MinimapReprojectionOptions& options,
                                             std::vector<std::uint8_t>& out) noexcept;
[[nodiscard]] ArxReturnCode renderLoadingScreen(std::span<const std::uint8_t> encoded_image,
                                                const LoadingScreenRenderOptions& options,
                                                std::vector<std::uint8_t>& out) noexcept;

}  // namespace pistoris::level_images
