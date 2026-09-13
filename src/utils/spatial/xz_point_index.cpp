// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/spatial/xz_point_index.h"

#include "utils/spatial/hash_grid.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pistoris::spatial {

void XzPointIndex::findNeighborCellCandidates(std::vector<std::uint32_t>& out, float x, float z) const {
  out.clear();
  if (!(cell_size_ > 0.0f) || !std::isfinite(cell_size_) || !std::isfinite(x) || !std::isfinite(z)) return;

  const int query_x = cellCoord(x, cell_size_);
  const int query_z = cellCoord(z, cell_size_);
  for (int dz = -1; dz <= 1; ++dz) {
    const int row = query_z + dz;
    const int min_x = query_x - 1;
    const int max_x = query_x + 1;
    auto entry = std::lower_bound(entries_.begin(), entries_.end(), Cell{min_x, row}, entryBeforeCell);
    while (entry != entries_.end() && entry->cell_z == row && entry->cell_x <= max_x) {
      out.push_back(entry->source_index);
      ++entry;
    }
  }
  std::sort(out.begin(), out.end());
}

}  // namespace pistoris::spatial
