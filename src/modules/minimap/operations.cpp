// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/minimap.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace pistoris::minimap {

void setImage(MinimapData& minimap, std::vector<std::uint8_t> encoded, const ArxRect& bounds) noexcept {
  minimap.encoded_image = std::move(encoded);
  minimap.world_xz_bounds = bounds;
}

void clear(MinimapData& minimap) noexcept { minimap = {}; }

bool translate(MinimapData& minimap, const ArxVector3& offset) noexcept {
  if (minimap.encoded_image.empty()) return true;
  const double min_x = static_cast<double>(minimap.world_xz_bounds.min.x) + offset.x;
  const double max_x = static_cast<double>(minimap.world_xz_bounds.max.x) + offset.x;
  const double min_z = static_cast<double>(minimap.world_xz_bounds.min.y) + offset.z;
  const double max_z = static_cast<double>(minimap.world_xz_bounds.max.y) + offset.z;
  constexpr double kLowest = std::numeric_limits<float>::lowest();
  constexpr double kHighest = std::numeric_limits<float>::max();
  if (!std::isfinite(min_x) || !std::isfinite(max_x) || !std::isfinite(min_z) || !std::isfinite(max_z) ||
      min_x < kLowest || min_x > kHighest || max_x < kLowest || max_x > kHighest || min_z < kLowest ||
      min_z > kHighest || max_z < kLowest || max_z > kHighest) {
    return false;
  }
  minimap.world_xz_bounds = {
      .min = {static_cast<float>(min_x), static_cast<float>(min_z)},
      .max = {static_cast<float>(max_x), static_cast<float>(max_z)},
  };
  return true;
}

}  // namespace pistoris::minimap
