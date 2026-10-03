// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace pistoris::spatial {

class ArxLevelGrid {
 public:
  using Coord = std::uint8_t;
  using Key = std::uint16_t;

  static constexpr Coord kWorldCellCount = 160;
  static constexpr Coord kGridSize = kWorldCellCount + 2;
  static constexpr float kCellSize = 100.0f;
  static constexpr float kWorldMax = kWorldCellCount * kCellSize;
  static constexpr std::uint32_t kCellCount = static_cast<std::uint32_t>(kGridSize) * kGridSize;

  [[nodiscard]] static std::optional<Coord> cellCoord(float value) noexcept {
    if (!std::isfinite(value)) return std::nullopt;
    if (value < 0.0f) return 0;
    if (value > kWorldMax) return static_cast<Coord>(kGridSize - 1);
    if (value == kWorldMax) return kWorldCellCount;
    return static_cast<Coord>(std::floor(value / kCellSize) + 1.0f);
  }

  [[nodiscard]] static constexpr Key cellKey(Coord x, Coord z) noexcept {
    return static_cast<Key>(static_cast<Key>(z) * kGridSize + x);
  }

  [[nodiscard]] static std::optional<Key> cellKey(float x, float z) noexcept {
    std::optional<Coord> cell_x = cellCoord(x);
    std::optional<Coord> cell_z = cellCoord(z);
    if (!cell_x || !cell_z) return std::nullopt;
    return cellKey(*cell_x, *cell_z);
  }
};

}  // namespace pistoris::spatial
