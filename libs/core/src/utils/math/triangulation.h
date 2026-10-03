// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::math {

enum class TriangulationResult : std::uint8_t {
  kSuccess,
  kNonSimple,
  kFailed,
};

TriangulationResult triangulateSimplePolygon(std::span<const ArxVector2> points, std::vector<std::uint32_t>& triangles);

}  // namespace pistoris::math
