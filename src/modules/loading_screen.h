// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

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
  kBadImage,
  kOutOfMemory,
};

// --- Validation ---

Error validate(const LoadingScreenData& loading_screen) noexcept;
Error validateImage(std::span<const std::uint8_t> encoded) noexcept;

// --- Mutation ---

void setImage(LoadingScreenData& loading_screen, std::vector<std::uint8_t> encoded) noexcept;
void clear(LoadingScreenData& loading_screen) noexcept;

// --- Generation ---

Error renderPng(const LoadingScreenData& loading_screen, bool fullscreen, std::vector<std::uint8_t>& out);
Error transcodePng(const LoadingScreenData& loading_screen, std::vector<std::uint8_t>& out);
Error renderPng(std::span<const std::uint8_t> encoded, bool fullscreen, std::vector<std::uint8_t>& out);
Error transcodePng(std::span<const std::uint8_t> encoded, std::vector<std::uint8_t>& out);

}  // namespace loading_screen
}  // namespace pistoris
