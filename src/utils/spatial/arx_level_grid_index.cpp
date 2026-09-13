// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/spatial/arx_level_grid_index.h"

#include "arx_pistoris/base/math.hpp"

#include "utils/math/finite.h"
#include "utils/spatial/arx_level_grid.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace pistoris::spatial {
namespace {

constexpr std::uint32_t kMaxCellsPerItem = 4096;

struct CellRectangle {
  ArxLevelGrid::Coord min_x = 0;
  ArxLevelGrid::Coord max_x = 0;
  ArxLevelGrid::Coord min_z = 0;
  ArxLevelGrid::Coord max_z = 0;
  std::uint32_t cell_count = 0;
};

bool validBounds(const ArxAabb& bounds) noexcept {
  return math::finite(bounds.min) && math::finite(bounds.max) && bounds.min.x <= bounds.max.x &&
         bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
}

std::optional<CellRectangle> cellRectangle(const ArxAabb& bounds) noexcept {
  if (!validBounds(bounds)) return std::nullopt;
  std::optional<ArxLevelGrid::Coord> min_x = ArxLevelGrid::cellCoord(bounds.min.x);
  std::optional<ArxLevelGrid::Coord> max_x = ArxLevelGrid::cellCoord(bounds.max.x);
  std::optional<ArxLevelGrid::Coord> min_z = ArxLevelGrid::cellCoord(bounds.min.z);
  std::optional<ArxLevelGrid::Coord> max_z = ArxLevelGrid::cellCoord(bounds.max.z);
  if (!min_x || !max_x || !min_z || !max_z) return std::nullopt;

  CellRectangle out;
  out.min_x = *min_x;
  out.max_x = *max_x;
  out.min_z = *min_z;
  out.max_z = *max_z;
  out.cell_count = (static_cast<std::uint32_t>(out.max_x) - out.min_x + 1U) *
                   (static_cast<std::uint32_t>(out.max_z) - out.min_z + 1U);
  return out;
}

template <typename Callback>
void forEachCell(const CellRectangle& cells, Callback&& callback) {
  for (std::uint16_t x = cells.min_x; x <= cells.max_x; ++x)
    for (std::uint16_t z = cells.min_z; z <= cells.max_z; ++z)
      callback(static_cast<ArxLevelGrid::Coord>(x), static_cast<ArxLevelGrid::Coord>(z));
}

void sortUnique(std::vector<std::uint32_t>& values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
}

}  // namespace

void ArxLevelGridIndex::add(std::uint32_t index, const ArxAabb& bounds) {
  std::optional<CellRectangle> cells = cellRectangle(bounds);
  if (!cells) return;

  indices_.push_back(index);
  if (cells->cell_count > kMaxCellsPerItem) {
    large_indices_.push_back(index);
    return;
  }

  forEachCell(*cells, [&](ArxLevelGrid::Coord x, ArxLevelGrid::Coord z) {
    buckets_[ArxLevelGrid::cellKey(x, z)].push_back(index);
  });
}

void ArxLevelGridIndex::visitPointCandidates(float x, float z, CandidateVisitor visitor, void* user_data) const {
  if (visitor == nullptr) return;
  std::optional<ArxLevelGrid::Key> key = ArxLevelGrid::cellKey(x, z);
  if (!key) return;

  auto bucket = buckets_.find(*key);
  if (bucket != buckets_.end())
    for (std::uint32_t index : bucket->second) visitor(index, user_data);
  for (std::uint32_t index : large_indices_) visitor(index, user_data);
}

bool ArxLevelGridIndex::hasAabbCandidates(const ArxAabb& bounds) const {
  std::optional<CellRectangle> cells = cellRectangle(bounds);
  if (!cells || indices_.empty()) return false;
  if (!large_indices_.empty()) return true;
  for (std::uint16_t x = cells->min_x; x <= cells->max_x; ++x) {
    for (std::uint16_t z = cells->min_z; z <= cells->max_z; ++z) {
      const auto bucket = buckets_.find(
          ArxLevelGrid::cellKey(static_cast<ArxLevelGrid::Coord>(x), static_cast<ArxLevelGrid::Coord>(z)));
      if (bucket != buckets_.end() && !bucket->second.empty()) return true;
    }
  }
  return false;
}

void ArxLevelGridIndex::findAabbCandidates(std::vector<std::uint32_t>& out, const ArxAabb& bounds) const {
  out.clear();
  std::optional<CellRectangle> cells = cellRectangle(bounds);
  if (!cells) return;

  const std::uint64_t query_cell_limit = std::min<std::uint64_t>(ArxLevelGrid::kCellCount, indices_.size());
  if (cells->cell_count > query_cell_limit) {
    out = indices_;
    sortUnique(out);
    return;
  }

  forEachCell(*cells, [&](ArxLevelGrid::Coord x, ArxLevelGrid::Coord z) {
    auto bucket = buckets_.find(ArxLevelGrid::cellKey(x, z));
    if (bucket != buckets_.end()) out.insert(out.end(), bucket->second.begin(), bucket->second.end());
  });
  out.insert(out.end(), large_indices_.begin(), large_indices_.end());
  sortUnique(out);
}

}  // namespace pistoris::spatial
