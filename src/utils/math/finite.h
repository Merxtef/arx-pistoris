// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include <cmath>

namespace pistoris::math {

inline bool finite(float value) { return std::isfinite(value); }

inline bool finite(double value) { return std::isfinite(value); }

inline bool finite(const ArxVector2& value) { return finite(value.x) && finite(value.y); }

inline bool finite(const ArxVector3& value) { return finite(value.x) && finite(value.y) && finite(value.z); }

inline bool finite(const ArxAngle& value) { return finite(value.pitch) && finite(value.yaw) && finite(value.roll); }

inline bool finite(const ArxQuat& value) {
  return finite(value.w) && finite(value.x) && finite(value.y) && finite(value.z);
}

inline bool finite(const ArxColor3& value) { return finite(value.r) && finite(value.g) && finite(value.b); }

}  // namespace pistoris::math
