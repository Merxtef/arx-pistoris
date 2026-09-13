// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/string_view.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace test_support::equivalence {

inline std::string_view stringView(ArxStringView value) {
  return value.size == 0 ? std::string_view{} : std::string_view(value.data, value.size);
}

inline bool floatEquivalent(float lhs, float rhs, float epsilon) {
  return epsilon == 0.0f ? lhs == rhs : std::abs(lhs - rhs) <= epsilon * std::max({1.0f, std::abs(lhs), std::abs(rhs)});
}

inline void checkFloatEqual(float lhs, float rhs, float epsilon) {
  if (epsilon == 0.0f) {
    CHECK(lhs == rhs);
  } else {
    CHECK(lhs == doctest::Approx(rhs).epsilon(epsilon).scale(1.0));
  }
}

inline bool vectorEquivalent(const ArxVector2& lhs, const ArxVector2& rhs, float epsilon) {
  return floatEquivalent(lhs.x, rhs.x, epsilon) && floatEquivalent(lhs.y, rhs.y, epsilon);
}

inline bool vectorEquivalent(const ArxVector3& lhs, const ArxVector3& rhs, float epsilon) {
  return floatEquivalent(lhs.x, rhs.x, epsilon) && floatEquivalent(lhs.y, rhs.y, epsilon) &&
         floatEquivalent(lhs.z, rhs.z, epsilon);
}

inline bool colorEquivalent(const ArxColor3& lhs, const ArxColor3& rhs, float epsilon) {
  return floatEquivalent(lhs.r, rhs.r, epsilon) && floatEquivalent(lhs.g, rhs.g, epsilon) &&
         floatEquivalent(lhs.b, rhs.b, epsilon);
}

inline bool rotationEquivalent(const ArxQuat& lhs, const ArxQuat& rhs, float epsilon) {
  const bool same = floatEquivalent(lhs.w, rhs.w, epsilon) && floatEquivalent(lhs.x, rhs.x, epsilon) &&
                    floatEquivalent(lhs.y, rhs.y, epsilon) && floatEquivalent(lhs.z, rhs.z, epsilon);
  const bool negated = floatEquivalent(lhs.w, -rhs.w, epsilon) && floatEquivalent(lhs.x, -rhs.x, epsilon) &&
                       floatEquivalent(lhs.y, -rhs.y, epsilon) && floatEquivalent(lhs.z, -rhs.z, epsilon);
  return same || negated;
}

inline void checkVectorEqual(const ArxVector2& lhs, const ArxVector2& rhs, float epsilon) {
  checkFloatEqual(lhs.x, rhs.x, epsilon);
  checkFloatEqual(lhs.y, rhs.y, epsilon);
}

inline void checkVectorEqual(const ArxVector3& lhs, const ArxVector3& rhs, float epsilon) {
  checkFloatEqual(lhs.x, rhs.x, epsilon);
  checkFloatEqual(lhs.y, rhs.y, epsilon);
  checkFloatEqual(lhs.z, rhs.z, epsilon);
}

inline void checkColorEqual(const ArxColor3& lhs, const ArxColor3& rhs, float epsilon) {
  checkFloatEqual(lhs.r, rhs.r, epsilon);
  checkFloatEqual(lhs.g, rhs.g, epsilon);
  checkFloatEqual(lhs.b, rhs.b, epsilon);
}

inline void checkQuatEqual(const ArxQuat& lhs, const ArxQuat& rhs, float epsilon) {
  checkFloatEqual(lhs.w, rhs.w, epsilon);
  checkFloatEqual(lhs.x, rhs.x, epsilon);
  checkFloatEqual(lhs.y, rhs.y, epsilon);
  checkFloatEqual(lhs.z, rhs.z, epsilon);
}

inline void checkImageViewsEqual(ArxEncodedImageView lhs, ArxEncodedImageView rhs) {
  CHECK(lhs.size == rhs.size);
  if (lhs.size != rhs.size) return;
  if (lhs.size != 0) CHECK(std::equal(lhs.data, lhs.data + lhs.size, rhs.data));
}

struct SpatialCell {
  std::int64_t x;
  std::int64_t y;
  std::int64_t z;

  bool operator==(const SpatialCell&) const = default;
};

struct SpatialCellHash {
  std::size_t operator()(const SpatialCell& cell) const noexcept {
    std::size_t result = std::hash<std::int64_t>{}(cell.x);
    result ^= std::hash<std::int64_t>{}(cell.y) + 0x9e3779b9U + (result << 6U) + (result >> 2U);
    result ^= std::hash<std::int64_t>{}(cell.z) + 0x9e3779b9U + (result << 6U) + (result >> 2U);
    return result;
  }
};

inline SpatialCell spatialCell(const ArxVector3& position, double cell_size) {
  return {
      static_cast<std::int64_t>(std::floor(static_cast<double>(position.x) / cell_size)),
      static_cast<std::int64_t>(std::floor(static_cast<double>(position.y) / cell_size)),
      static_cast<std::int64_t>(std::floor(static_cast<double>(position.z) / cell_size)),
  };
}

template <class Item, class LhsPosition, class RhsPosition, class Equivalent>
bool spatialMultisetEquivalent(std::span<const Item> lhs, std::span<const Item> rhs, LhsPosition lhs_position,
                               RhsPosition rhs_position, Equivalent equivalent, float epsilon,
                               float coordinate_scale = 1.0f) {
  if (lhs.size() != rhs.size()) return false;
  if (lhs.empty()) return true;

  float scale = std::max(1.0f, std::abs(coordinate_scale));
  for (const Item& item : lhs) {
    const ArxVector3 point = lhs_position(item);
    scale = std::max({scale, std::abs(point.x), std::abs(point.y), std::abs(point.z)});
  }
  for (const Item& item : rhs) {
    const ArxVector3 point = rhs_position(item);
    scale = std::max({scale, std::abs(point.x), std::abs(point.y), std::abs(point.z)});
  }
  const double cell_size =
      epsilon == 0.0f ? 1.0 : std::max<double>(epsilon * scale, std::numeric_limits<float>::epsilon());

  std::unordered_multimap<SpatialCell, std::size_t, SpatialCellHash> rhs_cells;
  rhs_cells.reserve(rhs.size());
  for (std::size_t index = 0; index < rhs.size(); ++index)
    rhs_cells.emplace(spatialCell(rhs_position(rhs[index]), cell_size), index);

  std::vector<std::size_t> offsets;
  std::vector<std::size_t> candidates;
  offsets.reserve(lhs.size() + 1);
  offsets.push_back(0);
  for (const Item& item : lhs) {
    const SpatialCell center = spatialCell(lhs_position(item), cell_size);
    for (std::int64_t x = center.x - 1; x <= center.x + 1; ++x) {
      for (std::int64_t y = center.y - 1; y <= center.y + 1; ++y) {
        for (std::int64_t z = center.z - 1; z <= center.z + 1; ++z) {
          const auto [first, last] = rhs_cells.equal_range({x, y, z});
          for (auto candidate = first; candidate != last; ++candidate) {
            if (equivalent(item, rhs[candidate->second])) candidates.push_back(candidate->second);
          }
        }
      }
    }
    offsets.push_back(candidates.size());
  }

  std::vector<std::size_t> order(lhs.size());
  std::iota(order.begin(), order.end(), 0);
  std::ranges::sort(order, [&](std::size_t left, std::size_t right) {
    return offsets[left + 1] - offsets[left] < offsets[right + 1] - offsets[right];
  });

  constexpr std::size_t kUnmatched = std::numeric_limits<std::size_t>::max();
  std::vector<std::size_t> matched_lhs(rhs.size(), kUnmatched);
  std::vector<std::size_t> seen_rhs(rhs.size(), 0);
  const auto augment = [&](auto&& self, std::size_t lhs_index, std::size_t visit) -> bool {
    for (std::size_t candidate = offsets[lhs_index]; candidate < offsets[lhs_index + 1]; ++candidate) {
      const std::size_t rhs_index = candidates[candidate];
      if (matched_lhs[rhs_index] == kUnmatched) {
        seen_rhs[rhs_index] = visit;
        matched_lhs[rhs_index] = lhs_index;
        return true;
      }
    }
    for (std::size_t candidate = offsets[lhs_index]; candidate < offsets[lhs_index + 1]; ++candidate) {
      const std::size_t rhs_index = candidates[candidate];
      if (seen_rhs[rhs_index] == visit) continue;
      seen_rhs[rhs_index] = visit;
      if (self(self, matched_lhs[rhs_index], visit)) {
        matched_lhs[rhs_index] = lhs_index;
        return true;
      }
    }
    return false;
  };

  for (std::size_t index = 0; index < order.size(); ++index) {
    if (!augment(augment, order[index], index + 1)) return false;
  }
  return true;
}

template <class Item, class Position, class Equivalent>
bool spatialMultisetEquivalent(std::span<const Item> lhs, std::span<const Item> rhs, Position position,
                               Equivalent equivalent, float epsilon, float coordinate_scale = 1.0f) {
  return spatialMultisetEquivalent(lhs, rhs, position, position, equivalent, epsilon, coordinate_scale);
}

template <class Item, class Position, class Equivalent>
bool spatialSetEquivalent(std::span<const Item> lhs, std::span<const Item> rhs, Position position,
                          Equivalent equivalent, float epsilon, float coordinate_scale = 1.0f) {
  if (lhs.empty() || rhs.empty()) return lhs.empty() && rhs.empty();

  float scale = std::max(1.0f, std::abs(coordinate_scale));
  for (const Item& item : lhs) {
    const ArxVector3 point = position(item);
    scale = std::max({scale, std::abs(point.x), std::abs(point.y), std::abs(point.z)});
  }
  for (const Item& item : rhs) {
    const ArxVector3 point = position(item);
    scale = std::max({scale, std::abs(point.x), std::abs(point.y), std::abs(point.z)});
  }
  const double cell_size =
      epsilon == 0.0f ? 1.0 : std::max<double>(epsilon * scale, std::numeric_limits<float>::epsilon());

  const auto is_subset = [&](std::span<const Item> values, std::span<const Item> candidates) {
    std::unordered_multimap<SpatialCell, std::size_t, SpatialCellHash> candidate_cells;
    candidate_cells.reserve(candidates.size());
    for (std::size_t index = 0; index < candidates.size(); ++index)
      candidate_cells.emplace(spatialCell(position(candidates[index]), cell_size), index);

    for (const Item& value : values) {
      const SpatialCell center = spatialCell(position(value), cell_size);
      bool matched = false;
      for (std::int64_t x = center.x - 1; x <= center.x + 1 && !matched; ++x) {
        for (std::int64_t y = center.y - 1; y <= center.y + 1 && !matched; ++y) {
          for (std::int64_t z = center.z - 1; z <= center.z + 1 && !matched; ++z) {
            const auto [first, last] = candidate_cells.equal_range({x, y, z});
            for (auto candidate = first; candidate != last; ++candidate) {
              if (!equivalent(value, candidates[candidate->second])) continue;
              matched = true;
              break;
            }
          }
        }
      }
      if (!matched) return false;
    }
    return true;
  };

  return is_subset(lhs, rhs) && is_subset(rhs, lhs);
}

template <class Item, class Position>
float maxAbsCoordinate(std::span<const Item> items, Position position) {
  float result = 1.0f;
  for (const Item& item : items) {
    const ArxVector3 point = position(item);
    result = std::max({result, std::abs(point.x), std::abs(point.y), std::abs(point.z)});
  }
  return result;
}

}  // namespace test_support::equivalence
