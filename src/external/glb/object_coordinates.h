// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/glb.hpp"

#include "utils/math/rotation.h"

#include <cmath>

namespace pistoris::glb_object {

inline bool validUnits(float value) noexcept {
  return std::isfinite(value) && value >= glb::kMinArxUnitsPerUnit && value <= glb::kMaxArxUnitsPerUnit;
}

inline ArxVector3 toGlbPoint(const ArxVector3& value, float units) { return math::rotateHalfTurnX(value) / units; }

inline ArxVector3 toArxPoint(const ArxVector3& value, float units) { return math::rotateHalfTurnX(value) * units; }

inline ArxVector3 toGlbDirection(const ArxVector3& value) { return math::rotateHalfTurnX(value); }
inline ArxVector3 toArxDirection(const ArxVector3& value) { return math::rotateHalfTurnX(value); }

}  // namespace pistoris::glb_object
