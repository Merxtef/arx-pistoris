// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "utils/spatial/hash_grid.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace pistoris::spatial {

class XzPointIndex {
 public:
  template <class PositionAt>
  void rebuild(std::size_t count, float cell_size, PositionAt&& position_at) {
    assert(cell_size > 0.0f && std::isfinite(cell_size));
    assert(count <= std::numeric_limits<std::uint32_t>::max());
    cell_size_ = cell_size;
    entries_.clear();
    entries_.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      const ArxVector3 position = position_at(index);
      assert(std::isfinite(position.x) && std::isfinite(position.z));
      entries_.push_back(
          {cellCoord(position.x, cell_size), cellCoord(position.z, cell_size), static_cast<std::uint32_t>(index)});
    }
    std::sort(entries_.begin(), entries_.end(), entryLess);
  }

  void findNeighborCellCandidates(std::vector<std::uint32_t>& out, float x, float z) const;

 private:
  struct Cell {
    int x = 0;
    int z = 0;
  };

  struct Entry {
    int cell_x = 0;
    int cell_z = 0;
    std::uint32_t source_index = 0;
  };

  static bool entryLess(const Entry& left, const Entry& right) noexcept {
    if (left.cell_z != right.cell_z) return left.cell_z < right.cell_z;
    if (left.cell_x != right.cell_x) return left.cell_x < right.cell_x;
    return left.source_index < right.source_index;
  }

  static bool entryBeforeCell(const Entry& entry, const Cell& cell) noexcept {
    return entry.cell_z < cell.z || (entry.cell_z == cell.z && entry.cell_x < cell.x);
  }

  float cell_size_ = 0.0f;
  std::vector<Entry> entries_;
};

}  // namespace pistoris::spatial
