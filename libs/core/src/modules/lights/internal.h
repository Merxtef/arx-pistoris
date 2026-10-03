// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"

#include "utils/math/finite.h"

namespace pistoris::lights {

inline bool unitColor(const ArxColor3& color) noexcept {
  return math::finite(color) && color.r >= 0.0f && color.r <= 1.0f && color.g >= 0.0f && color.g <= 1.0f &&
         color.b >= 0.0f && color.b <= 1.0f;
}

}  // namespace pistoris::lights
