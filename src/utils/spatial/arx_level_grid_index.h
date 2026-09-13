// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "utils/spatial/arx_level_grid.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace pistoris::spatial {

class ArxLevelGridIndex {  // NOLINT(bugprone-exception-escape): MSVC debug STL misreports container moves
 public:
  using CandidateVisitor = void (*)(std::uint32_t index, void* user_data);

  void add(std::uint32_t index, const ArxAabb& bounds);
  void visitPointCandidates(float x, float z, CandidateVisitor visitor, void* user_data) const;
  [[nodiscard]] bool hasAabbCandidates(const ArxAabb& bounds) const;
  void findAabbCandidates(std::vector<std::uint32_t>& out, const ArxAabb& bounds) const;

 private:
  std::unordered_map<ArxLevelGrid::Key, std::vector<std::uint32_t>> buckets_;
  std::vector<std::uint32_t> indices_;
  std::vector<std::uint32_t> large_indices_;
};

}  // namespace pistoris::spatial
