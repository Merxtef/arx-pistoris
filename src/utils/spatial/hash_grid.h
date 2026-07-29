// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace pistoris::spatial {

inline constexpr int kMinCellCoord = std::numeric_limits<int>::min() + 1;
inline constexpr int kMaxCellCoord = std::numeric_limits<int>::max() - 1;

inline int cellCoord(float value, float cell_size) noexcept {
  if (!std::isfinite(value) || !std::isfinite(cell_size) || cell_size <= 0.0f) return 0;
  const double cell = std::floor(static_cast<double>(value) / static_cast<double>(cell_size));
  return static_cast<int>(std::clamp(cell, static_cast<double>(kMinCellCoord), static_cast<double>(kMaxCellCoord)));
}

using CellKey = std::uint64_t;

inline CellKey cellKey(int x, int z) {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(z);
}

inline CellKey cellKey(float x, float z, float cell_size) {
  return cellKey(cellCoord(x, cell_size), cellCoord(z, cell_size));
}

}  // namespace pistoris::spatial
