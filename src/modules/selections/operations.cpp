// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/selections.h"
#include "utils/identifier.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::selections {

bool occupied(const SelectionsData& selections, SelectionId id) noexcept {
  return id < 64U && (selections.occupied & bit(id)) != 0;
}

namespace {

SelectionId firstFree(const SelectionsData& selections) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!occupied(selections, id)) return id;
  }
  return kInvalidSelectionId;
}

}  // namespace

SelectionId addSelection(SelectionsData& selections, Selection selection) {
  const SelectionId id = firstFree(selections);
  assert(id != kInvalidSelectionId);
  selections.slots[id] = std::move(selection);
  selections.occupied |= bit(id);
  return id;
}

void setSelection(SelectionsData& selections, SelectionId id, Selection selection) noexcept {
  assert(occupied(selections, id));
  selections.slots[id] = std::move(selection);
}

void repairNames(const SelectionsData& selections, std::span<Selection> candidates, SelectionId ignored) {
  IdentifierUniquifier names({.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength});
  names.reserve(candidates.size(), 64U);
  for (SelectionId id = 0; id < 64U; ++id) {
    if (id == ignored || !occupied(selections, id)) continue;
    names.occupy(selections.slots[id].name);
  }
  for (Selection& selection : candidates) names.add(selection.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

void reserveVertexCapacity(SelectionsData& selections, std::size_t capacity) {
  selections.vertex_masks.reserve(capacity);
}

void reserveBoneCapacity(SelectionsData& selections, std::size_t capacity) { selections.bone_masks.reserve(capacity); }

void reserveActionPointCapacity(SelectionsData& selections, std::size_t capacity) {
  selections.action_point_masks.reserve(capacity);
}

void appendEmptyVertexMask(SelectionsData& selections) { selections.vertex_masks.push_back(0); }

void appendEmptyBoneMask(SelectionsData& selections) { selections.bone_masks.push_back(0); }

void appendEmptyActionPointMask(SelectionsData& selections) { selections.action_point_masks.push_back(0); }

void truncateVertexMasks(SelectionsData& selections, std::size_t size) noexcept {
  while (selections.vertex_masks.size() > size) selections.vertex_masks.pop_back();
}

void removeBoneMask(SelectionsData& selections, BoneIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < selections.bone_masks.size());
  selections.bone_masks.erase(selections.bone_masks.begin() + static_cast<std::ptrdiff_t>(index));
}

void removeActionPointMask(SelectionsData& selections, ActionPointIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < selections.action_point_masks.size());
  selections.action_point_masks.erase(selections.action_point_masks.begin() + static_cast<std::ptrdiff_t>(index));
}

void replaceVertexMasks(SelectionsData& selections, std::vector<SelectionMask>&& masks) noexcept {
  selections.vertex_masks = std::move(masks);
}

void replaceBoneMasks(SelectionsData& selections, std::vector<SelectionMask>&& masks) noexcept {
  selections.bone_masks = std::move(masks);
}

void replaceActionPointMasks(SelectionsData& selections, std::vector<SelectionMask>&& masks) noexcept {
  selections.action_point_masks = std::move(masks);
}

void clearVertexMasks(SelectionsData& selections) noexcept { selections.vertex_masks.clear(); }

void clearBoneMasks(SelectionsData& selections) noexcept { selections.bone_masks.clear(); }

void clearActionPointMasks(SelectionsData& selections) noexcept { selections.action_point_masks.clear(); }

void clearOriginMask(SelectionsData& selections) noexcept { selections.origin_mask = 0; }

bool leadingVerticesReferenceBone(const SelectionsData& selections, BoneIndex bone) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!occupied(selections, id)) continue;
    const std::optional<SelectionLeadingVertex>& leading = selections.slots[id].leading_vertex;
    if (leading && leading->bone == bone) return true;
  }
  return false;
}

void remapLeadingBoneIndicesAfterRemoval(SelectionsData& selections, BoneIndex removed) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!occupied(selections, id)) continue;
    std::optional<SelectionLeadingVertex>& leading = selections.slots[id].leading_vertex;
    if (leading && leading->bone != kInvalidBoneIndex && leading->bone > removed) --leading->bone;
  }
}

void clearLeadingBoneReferences(SelectionsData& selections) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!occupied(selections, id)) continue;
    std::optional<SelectionLeadingVertex>& leading = selections.slots[id].leading_vertex;
    if (leading) leading->bone = kInvalidBoneIndex;
  }
}

void remapVertexMasks(SelectionsData& selections, std::span<const VertexIndex> vertex_remap) noexcept {
  if (vertex_remap.empty()) return;
  assert(selections.vertex_masks.size() == vertex_remap.size());
  std::size_t next = 0;
  for (std::size_t old = 0; old < vertex_remap.size(); ++old) {
    const VertexIndex mapped = vertex_remap[old];
    if (mapped == kInvalidVertexIndex) continue;
    assert(mapped == next);
    if (next != old) selections.vertex_masks[next] = selections.vertex_masks[old];
    ++next;
  }
  while (selections.vertex_masks.size() > next) selections.vertex_masks.pop_back();
}

namespace {

std::size_t memberCount(std::span<const SelectionMask> masks, SelectionId id) noexcept {
  const SelectionMask member = bit(id);
  return static_cast<std::size_t>(
      std::count_if(masks.begin(), masks.end(), [member](SelectionMask mask) { return (mask & member) != 0; }));
}

template <class Index>
void copyMembers(std::span<const SelectionMask> masks, SelectionId id, std::size_t offset, std::size_t count,
                 Index* out) noexcept {
  const SelectionMask member = bit(id);
  std::size_t visible = 0;
  std::size_t copied = 0;
  for (std::size_t index = 0; index < masks.size() && copied < count; ++index) {
    if ((masks[index] & member) == 0) continue;
    if (visible++ < offset) continue;
    out[copied++] = static_cast<Index>(index);
  }
}

template <class Index>
void replaceMembers(std::vector<SelectionMask>& masks, SelectionId id, std::span<const Index> members) noexcept {
  const SelectionMask member = bit(id);
  const SelectionMask keep = ~member;
  for (SelectionMask& mask : masks) mask &= keep;
  for (Index index : members) masks[static_cast<std::size_t>(index)] |= member;
}

void clearMembers(std::vector<SelectionMask>& masks, SelectionId id) noexcept {
  const SelectionMask keep = ~bit(id);
  for (SelectionMask& mask : masks) mask &= keep;
}

}  // namespace

std::size_t vertexMemberCount(const SelectionsData& selections, SelectionId id) noexcept {
  if (!occupied(selections, id)) return 0;
  return memberCount(selections.vertex_masks, id);
}

std::size_t boneMemberCount(const SelectionsData& selections, SelectionId id) noexcept {
  if (!occupied(selections, id)) return 0;
  return memberCount(selections.bone_masks, id);
}

std::size_t actionPointMemberCount(const SelectionsData& selections, SelectionId id) noexcept {
  if (!occupied(selections, id)) return 0;
  return memberCount(selections.action_point_masks, id);
}

void copyVertexMembers(const SelectionsData& selections, SelectionId id, std::size_t offset, std::size_t count,
                       VertexIndex* out) noexcept {
  if (!occupied(selections, id)) return;
  copyMembers(selections.vertex_masks, id, offset, count, out);
}

void copyBoneMembers(const SelectionsData& selections, SelectionId id, std::size_t offset, std::size_t count,
                     BoneIndex* out) noexcept {
  if (!occupied(selections, id)) return;
  copyMembers(selections.bone_masks, id, offset, count, out);
}

void copyActionPointMembers(const SelectionsData& selections, SelectionId id, std::size_t offset, std::size_t count,
                            ActionPointIndex* out) noexcept {
  if (!occupied(selections, id)) return;
  copyMembers(selections.action_point_masks, id, offset, count, out);
}

void updateMembers(SelectionsData& selections, SelectionId id, std::optional<std::span<const VertexIndex>> vertices,
                   std::optional<std::span<const BoneIndex>> bones,
                   std::optional<std::span<const ActionPointIndex>> action_points) noexcept {
  assert(occupied(selections, id));
  if (vertices) replaceMembers(selections.vertex_masks, id, *vertices);
  if (bones) replaceMembers(selections.bone_masks, id, *bones);
  if (action_points) replaceMembers(selections.action_point_masks, id, *action_points);
}

void clearVertexMembers(SelectionsData& selections, SelectionId id) noexcept {
  assert(occupied(selections, id));
  clearMembers(selections.vertex_masks, id);
}

void clearBoneMembers(SelectionsData& selections, SelectionId id) noexcept {
  assert(occupied(selections, id));
  clearMembers(selections.bone_masks, id);
}

void clearActionPointMembers(SelectionsData& selections, SelectionId id) noexcept {
  assert(occupied(selections, id));
  clearMembers(selections.action_point_masks, id);
}

bool includesOrigin(const SelectionsData& selections, SelectionId id) noexcept {
  if (!occupied(selections, id)) return false;
  return (selections.origin_mask & bit(id)) != 0;
}

void setIncludesOrigin(SelectionsData& selections, SelectionId id, bool includes) noexcept {
  assert(occupied(selections, id));
  if (includes)
    selections.origin_mask |= bit(id);
  else
    selections.origin_mask &= ~bit(id);
}

void removeSelection(SelectionsData& selections, SelectionId id) noexcept {
  assert(occupied(selections, id));
  const SelectionMask keep = ~bit(id);
  for (SelectionMask& mask : selections.vertex_masks) mask &= keep;
  for (SelectionMask& mask : selections.bone_masks) mask &= keep;
  for (SelectionMask& mask : selections.action_point_masks) mask &= keep;
  selections.origin_mask &= keep;
  selections.slots[id] = {};
  selections.occupied &= keep;
}

void clearSelections(SelectionsData& selections) noexcept {
  selections.slots = {};
  selections.occupied = 0;
  std::fill(selections.vertex_masks.begin(), selections.vertex_masks.end(), 0);
  std::fill(selections.bone_masks.begin(), selections.bone_masks.end(), 0);
  std::fill(selections.action_point_masks.begin(), selections.action_point_masks.end(), 0);
  selections.origin_mask = 0;
}

}  // namespace pistoris::selections
