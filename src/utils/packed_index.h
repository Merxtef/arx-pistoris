// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <algorithm>
#include <cstddef>

namespace pistoris::packed {

inline std::size_t symmetricPairCount(std::size_t count) { return count * (count - 1U) / 2U; }

inline std::size_t symmetricPairIndex(std::size_t a, std::size_t b) {
  if (a > b) std::swap(a, b);
  return b * (b - 1U) / 2U + a;
}

}  // namespace pistoris::packed
