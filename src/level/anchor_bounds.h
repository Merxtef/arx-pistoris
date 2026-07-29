// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pistoris::level_anchor_bounds {

inline bool insideNativeMap(const ArxVector3& position) noexcept {
  return position.x >= kLevelMinXZ && position.x <= kLevelMaxXZ && position.z >= kLevelMinXZ &&
         position.z <= kLevelMaxXZ;
}

inline bool materiallyOutsideGeometry(const ArxAabb& bounds, const ArxVector3& position) noexcept {
  const float magnitude = std::max({1.0f,
                                    std::abs(bounds.min.x),
                                    std::abs(bounds.min.y),
                                    std::abs(bounds.min.z),
                                    std::abs(bounds.max.x),
                                    std::abs(bounds.max.y),
                                    std::abs(bounds.max.z),
                                    std::abs(position.x),
                                    std::abs(position.y),
                                    std::abs(position.z)});
  const float tolerance = std::max(1.0e-4f, 8.0f * std::numeric_limits<float>::epsilon() * magnitude);
  return position.x < bounds.min.x - tolerance || position.x > bounds.max.x + tolerance ||
         position.y < bounds.min.y - tolerance || position.y > bounds.max.y + tolerance ||
         position.z < bounds.min.z - tolerance || position.z > bounds.max.z + tolerance;
}

}  // namespace pistoris::level_anchor_bounds
