// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/navigation/internal.h"

#include <array>
#include <cstddef>

namespace pistoris::navigation {
namespace {

constexpr float kDiagonalScale = 0.7071067811865475f;

}  // namespace

std::array<ArxVector3, 17> navigationProbeOffsets(float radius) {
  std::array<ArxVector3, 17> offsets{};
  std::size_t next = 1;
  const std::array<float, 2> distances = {radius * 0.25f, radius * 0.5f};
  for (float distance : distances) {
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
    for (const ArxVector3& offset : cardinals) offsets[next++] = offset;
    for (const ArxVector3& offset : diagonals) offsets[next++] = offset;
  }
  return offsets;
}

}  // namespace pistoris::navigation
