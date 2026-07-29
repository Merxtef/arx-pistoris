// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include <algorithm>

namespace pistoris::math {

inline void expand(ArxRect& bounds, const ArxVector2& point) {
  bounds.min.x = std::min(bounds.min.x, point.x);
  bounds.min.y = std::min(bounds.min.y, point.y);
  bounds.max.x = std::max(bounds.max.x, point.x);
  bounds.max.y = std::max(bounds.max.y, point.y);
}

inline void expand(ArxAabb& bounds, const ArxVector3& point) {
  bounds.min.x = std::min(bounds.min.x, point.x);
  bounds.min.y = std::min(bounds.min.y, point.y);
  bounds.min.z = std::min(bounds.min.z, point.z);
  bounds.max.x = std::max(bounds.max.x, point.x);
  bounds.max.y = std::max(bounds.max.y, point.y);
  bounds.max.z = std::max(bounds.max.z, point.z);
}

inline bool containsInclusive(const ArxRect& bounds, const ArxVector2& point) {
  return point.x >= bounds.min.x && point.x <= bounds.max.x && point.y >= bounds.min.y && point.y <= bounds.max.y;
}

inline bool containsInclusive(const ArxAabb& bounds, const ArxVector3& point) {
  return point.x >= bounds.min.x && point.x <= bounds.max.x && point.y >= bounds.min.y && point.y <= bounds.max.y &&
         point.z >= bounds.min.z && point.z <= bounds.max.z;
}

inline bool overlapsInclusive(const ArxRect& first, const ArxRect& second) {
  return first.min.x <= second.max.x && first.max.x >= second.min.x && first.min.y <= second.max.y &&
         first.max.y >= second.min.y;
}

inline bool overlapsInclusive(const ArxAabb& first, const ArxAabb& second) {
  return first.min.x <= second.max.x && first.max.x >= second.min.x && first.min.y <= second.max.y &&
         first.max.y >= second.min.y && first.min.z <= second.max.z && first.max.z >= second.min.z;
}

}  // namespace pistoris::math
