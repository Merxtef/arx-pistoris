// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "utils/math/rotation.h"

namespace pistoris::obj_coordinates {

inline ArxVector3 toModelPoint(const ArxVector3& value) noexcept { return math::rotateHalfTurnX(value); }
inline ArxVector3 toObjPoint(const ArxVector3& value) noexcept { return math::rotateHalfTurnX(value); }

inline ArxVector3 toModelDirection(const ArxVector3& value) noexcept { return math::rotateHalfTurnX(value); }
inline ArxVector3 toObjDirection(const ArxVector3& value) noexcept { return math::rotateHalfTurnX(value); }

inline ArxVector2 toModelTexcoord(const ArxVector2& value) noexcept { return {value.x, 1.0f - value.y}; }
inline ArxVector2 toObjTexcoord(const ArxVector2& value) noexcept { return {value.x, 1.0f - value.y}; }

}  // namespace pistoris::obj_coordinates
