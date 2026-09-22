// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pistoris::cinematic_constraints {

struct GridVertexDimensions {
  std::uint64_t columns = 0;
  std::uint64_t rows = 0;

  bool operator==(const GridVertexDimensions&) const = default;
};

inline GridVertexDimensions gridVertexDimensions(std::uint32_t width, std::uint32_t height,
                                                 std::int32_t scale) noexcept {
  if (width == 0 || height == 0 || scale <= 0) return {};
  constexpr std::uint64_t kTileSize = 256;
  const std::uint64_t columns = (static_cast<std::uint64_t>(width) - 1U) / kTileSize + 1U;
  const std::uint64_t rows = (static_cast<std::uint64_t>(height) - 1U) / kTileSize + 1U;
  return {columns * static_cast<std::uint64_t>(scale) + 1U, rows * static_cast<std::uint64_t>(scale) + 1U};
}

inline bool safeGrid(std::uint32_t width, std::uint32_t height, std::int32_t scale, bool dream) noexcept {
  // The game stores draw vertices and dream offsets in fixed-size buffers.
  constexpr std::uint64_t kDrawVertices = 40000;
  constexpr std::uint64_t kDreamVertices = 64 * 64;
  const GridVertexDimensions grid = gridVertexDimensions(width, height, scale);
  const std::uint64_t limit = dream ? kDreamVertices : kDrawVertices;
  return grid.columns != 0 && grid.columns <= limit && grid.rows <= limit / grid.columns;
}

template <typename Keyframe>
bool validDuration(float fps, std::span<const Keyframe> keys) noexcept {
  float duration = 0.0f;
  for (std::size_t index = 0; index + 1U < keys.size(); ++index) {
    const float rate = fps * keys[index].outgoing_speed;
    if (!std::isfinite(rate) || rate <= 0.0f) return false;
    const float segment = static_cast<float>(keys[index + 1U].frame - keys[index].frame) / rate;
    if (!std::isfinite(segment) || segment <= 0.0f) return false;
    duration += segment;
    if (!std::isfinite(duration)) return false;
  }
  return duration > 0.0f;
}

}  // namespace pistoris::cinematic_constraints
