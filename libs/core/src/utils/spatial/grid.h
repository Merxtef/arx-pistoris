// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cmath>

namespace pistoris::spatial {

inline float firstCellCenter(float min_value, float spacing) {
  float start = std::floor(min_value / spacing) * spacing + spacing * 0.5f;
  while (start < min_value) start += spacing;
  return start;
}

}  // namespace pistoris::spatial
