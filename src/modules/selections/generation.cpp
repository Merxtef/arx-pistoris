// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/selections.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace pistoris::selections {
namespace {

struct BoneSelectionCounts {
  std::size_t vertices = 0;
  std::array<std::size_t, 64> selected{};
};

}  // namespace

BoneMaskInferenceResult inferBoneMasksFromVertices(const SelectionsData& selections,
                                                   std::span<const BoneIndex> vertex_bones, std::size_t bone_count) {
  assert(selections.vertex_masks.size() == vertex_bones.size());

  std::vector<BoneSelectionCounts> counts(bone_count);
  const std::size_t vertex_count = std::min(selections.vertex_masks.size(), vertex_bones.size());
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    const BoneIndex bone = vertex_bones[vertex];
    if (bone == kInvalidBoneIndex) continue;
    const std::size_t bone_index = static_cast<std::size_t>(bone);
    assert(bone_index < bone_count);
    if (bone_index >= bone_count) continue;

    BoneSelectionCounts& bone_counts = counts[bone_index];
    ++bone_counts.vertices;
    SelectionMask mask = selections.vertex_masks[vertex];
    while (mask != 0) {
      const unsigned id = std::countr_zero(mask);
      ++bone_counts.selected[id];
      mask &= mask - 1;
    }
  }

  BoneMaskInferenceResult result;
  result.masks.resize(bone_count);
  for (std::size_t bone = 0; bone < bone_count; ++bone) {
    const BoneSelectionCounts& bone_counts = counts[bone];
    if (bone_counts.vertices == 0) {
      ++result.bones_without_vertices;
      continue;
    }

    const std::size_t required = bone_counts.vertices - bone_counts.vertices / 10;
    SelectionMask mask = 0;
    for (SelectionId id = 0; id < 64U; ++id) {
      if ((selections.occupied & bit(id)) != 0 && bone_counts.selected[id] >= required) mask |= bit(id);
    }
    result.masks[bone] = mask;
    result.membership_count += std::popcount(mask);
  }
  return result;
}

}  // namespace pistoris::selections
