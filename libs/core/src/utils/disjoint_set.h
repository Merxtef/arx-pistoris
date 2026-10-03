// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace pistoris {

class DisjointSet {
 public:
  explicit DisjointSet(std::size_t count) : parent_(count), rank_(count, 0) {
    std::iota(parent_.begin(), parent_.end(), std::size_t{0});
  }

  std::size_t find(std::size_t value) {
    if (parent_[value] != value) parent_[value] = find(parent_[value]);
    return parent_[value];
  }

  bool unite(std::size_t first, std::size_t second) {
    first = find(first);
    second = find(second);
    if (first == second) return false;
    if (rank_[first] < rank_[second]) std::swap(first, second);
    parent_[second] = first;
    if (rank_[first] == rank_[second]) ++rank_[first];
    return true;
  }

 private:
  std::vector<std::size_t> parent_;
  std::vector<std::uint8_t> rank_;
};

}  // namespace pistoris
