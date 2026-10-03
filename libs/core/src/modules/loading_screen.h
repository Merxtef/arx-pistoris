// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "utils/encoded_image.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {

struct LoadingScreenData {
  std::vector<std::uint8_t> encoded_image;
};

namespace loading_screen {

inline constexpr std::uint32_t kWidth = 320;
inline constexpr std::uint32_t kHeight = 390;
inline constexpr std::uint32_t kFullscreenWidth = 640;
inline constexpr std::uint32_t kFullscreenHeight = 480;

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kBadImage,
  kOutOfMemory,
};

enum class Layout : std::uint8_t {
  kOriginal,
  kNormal,
  kFullscreen,
};

// --- Validation ---

Error validate(const LoadingScreenData& loading_screen) noexcept;
Error validateImage(std::span<const std::uint8_t> encoded) noexcept;

// --- Mutation ---

void setImage(LoadingScreenData& loading_screen, std::vector<std::uint8_t> encoded) noexcept;
void clear(LoadingScreenData& loading_screen) noexcept;

// --- Generation ---

Error render(const LoadingScreenData& loading_screen, Layout layout, image::Format format,
             std::vector<std::uint8_t>& out);
Error render(std::span<const std::uint8_t> encoded, Layout layout, image::Format format,
             std::vector<std::uint8_t>& out);

}  // namespace loading_screen
}  // namespace pistoris
