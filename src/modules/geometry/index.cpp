// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"

#include "modules/geometry.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/spatial/arx_level_grid.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace pistoris::geometry {
namespace {

constexpr std::uint32_t kMaxCellsPerTriangle = 4096;

struct CellRectangle {
  spatial::ArxLevelGrid::Coord min_x = 0;
  spatial::ArxLevelGrid::Coord max_x = 0;
  spatial::ArxLevelGrid::Coord min_z = 0;
  spatial::ArxLevelGrid::Coord max_z = 0;
  std::uint32_t cell_count = 0;
};

void sortUnique(std::vector<std::uint32_t>& values) {
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
}

bool validBounds(const ArxAabb& bounds) noexcept {
  return math::finite(bounds.min) && math::finite(bounds.max) && bounds.min.x <= bounds.max.x &&
         bounds.min.y <= bounds.max.y && bounds.min.z <= bounds.max.z;
}

std::optional<CellRectangle> cellRectangle(const ArxAabb& bounds) noexcept {
  if (!validBounds(bounds)) return std::nullopt;
  std::optional<spatial::ArxLevelGrid::Coord> min_x = spatial::ArxLevelGrid::cellCoord(bounds.min.x);
  std::optional<spatial::ArxLevelGrid::Coord> max_x = spatial::ArxLevelGrid::cellCoord(bounds.max.x);
  std::optional<spatial::ArxLevelGrid::Coord> min_z = spatial::ArxLevelGrid::cellCoord(bounds.min.z);
  std::optional<spatial::ArxLevelGrid::Coord> max_z = spatial::ArxLevelGrid::cellCoord(bounds.max.z);
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
      callback(static_cast<spatial::ArxLevelGrid::Coord>(x), static_cast<spatial::ArxLevelGrid::Coord>(z));
}

bool containsXz(const ArxAabb& bounds, float x, float z) noexcept {
  return x >= bounds.min.x && x <= bounds.max.x && z >= bounds.min.z && z <= bounds.max.z;
}

ArxAabb segmentBounds(const ArxVector3& a, const ArxVector3& b) {
  ArxAabb out;
  out.min = math::componentMin(a, b);
  out.max = math::componentMax(a, b);
  return out;
}

}  // namespace

TriangleIndex::TriangleIndex(std::span<const IndexedTriangle> triangles)
    : triangles_(triangles.begin(), triangles.end()) {
  for (std::size_t index = 0; index < triangles_.size(); ++index) {
    if (index > std::numeric_limits<std::uint32_t>::max()) break;
    const std::uint32_t triangle_index = static_cast<std::uint32_t>(index);
    const IndexedTriangle& triangle = triangles_[triangle_index];
    std::optional<CellRectangle> cells = cellRectangle(triangle.bounds);
    if (!cells.has_value()) continue;
    if (cells->cell_count > kMaxCellsPerTriangle) {
      large_triangle_indices_.push_back(triangle_index);
      continue;
    }
    forEachCell(*cells, [&](spatial::ArxLevelGrid::Coord x, spatial::ArxLevelGrid::Coord z) {
      buckets_[spatial::ArxLevelGrid::cellKey(x, z)].push_back(triangle_index);
    });
  }
}

const IndexedTriangle& TriangleIndex::triangle(std::uint32_t index) const { return triangles_[index]; }

std::vector<std::uint32_t> TriangleIndex::candidatesForXz(float x, float z) const {
  std::vector<std::uint32_t> out;
  std::optional<spatial::ArxLevelGrid::Key> key = spatial::ArxLevelGrid::cellKey(x, z);
  if (!key) return out;
  auto it = buckets_.find(*key);
  if (it != buckets_.end()) out = it->second;
  for (std::uint32_t index : large_triangle_indices_)
    if (containsXz(triangles_[index].bounds, x, z)) out.push_back(index);
  sortUnique(out);
  out.erase(
      std::remove_if(
          out.begin(), out.end(), [&](std::uint32_t index) { return !containsXz(triangles_[index].bounds, x, z); }),
      out.end());
  return out;
}

std::vector<std::uint32_t> TriangleIndex::candidatesForAabb(const ArxAabb& bounds) const {
  std::vector<std::uint32_t> out;
  std::optional<CellRectangle> cells = cellRectangle(bounds);
  if (!cells.has_value()) return out;

  const std::uint64_t query_cell_limit = std::min<std::uint64_t>(spatial::ArxLevelGrid::kCellCount, triangles_.size());
  if (cells->cell_count > query_cell_limit) {
    for (std::size_t index = 0; index < triangles_.size(); ++index) {
      if (index > std::numeric_limits<std::uint32_t>::max()) break;
      if (validBounds(triangles_[index].bounds) && math::overlapsInclusive(triangles_[index].bounds, bounds))
        out.push_back(static_cast<std::uint32_t>(index));
    }
    return out;
  }

  forEachCell(*cells, [&](spatial::ArxLevelGrid::Coord x, spatial::ArxLevelGrid::Coord z) {
    auto it = buckets_.find(spatial::ArxLevelGrid::cellKey(x, z));
    if (it == buckets_.end()) return;
    out.insert(out.end(), it->second.begin(), it->second.end());
  });
  out.insert(out.end(), large_triangle_indices_.begin(), large_triangle_indices_.end());
  sortUnique(out);
  out.erase(std::remove_if(out.begin(),
                           out.end(),
                           [&](std::uint32_t index) {
                             return !validBounds(triangles_[index].bounds) ||
                                    !math::overlapsInclusive(triangles_[index].bounds, bounds);
                           }),
            out.end());
  return out;
}

std::vector<std::uint32_t> TriangleIndex::candidatesForSegment(const ArxVector3& a, const ArxVector3& b) const {
  return candidatesForAabb(segmentBounds(a, b));
}

}  // namespace pistoris::geometry
