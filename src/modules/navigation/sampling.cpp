// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"

#include "modules/navigation/internal.h"

#include <array>
#include <vector>

namespace pistoris::navigation {
namespace {

constexpr float kDiagonalScale = 0.7071067811865475f;

}  // namespace

std::vector<ArxVector3> navigationProbeOffsets(float radius) {
  std::vector<ArxVector3> offsets{{0.0f, 0.0f, 0.0f}};
  const std::array<float, 2> distances = {radius * 0.25f, radius * 0.5f};
  for (float distance : distances) {
    if (distance <= 0.0f) continue;
    const std::array<ArxVector3, 4> cardinals = {
        ArxVector3{distance, 0.0f, 0.0f},
        ArxVector3{-distance, 0.0f, 0.0f},
        ArxVector3{0.0f, 0.0f, distance},
        ArxVector3{0.0f, 0.0f, -distance},
    };
    const std::array<ArxVector3, 4> diagonals = {
        ArxVector3{distance * kDiagonalScale, 0.0f, distance * kDiagonalScale},
        ArxVector3{distance * kDiagonalScale, 0.0f, -distance * kDiagonalScale},
        ArxVector3{-distance * kDiagonalScale, 0.0f, distance * kDiagonalScale},
        ArxVector3{-distance * kDiagonalScale, 0.0f, -distance * kDiagonalScale},
    };
    offsets.insert(offsets.end(), cardinals.begin(), cardinals.end());
    offsets.insert(offsets.end(), diagonals.begin(), diagonals.end());
  }
  return offsets;
}

}  // namespace pistoris::navigation
