// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/geometry.h"

namespace pistoris::geometry {

inline bool validPositionWeldMetric(PositionWeldMetric metric) noexcept {
  switch (metric) {
    case PositionWeldMetric::kEuclidean:
    case PositionWeldMetric::kAxisAligned:
      return true;
  }
  return false;
}

}  // namespace pistoris::geometry
