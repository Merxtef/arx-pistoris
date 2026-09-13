// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/selections.h"
#include "modules/skeleton.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace pistoris::selections {
namespace {

template <class Index>
bool validMembers(std::span<const Index> members, std::size_t size) noexcept {
  return std::all_of(
      members.begin(), members.end(), [size](Index index) { return static_cast<std::size_t>(index) < size; });
}

bool validMasks(const std::vector<SelectionMask>& masks, SelectionMask occupied_mask) noexcept {
  for (SelectionMask mask : masks) {
    if ((mask & occupied_mask) != mask) return false;
  }
  return true;
}

}  // namespace

Error validateMemberUpdate(const SelectionsData& selections, SelectionId id,
                           std::optional<std::span<const VertexIndex>> vertices,
                           std::optional<std::span<const BoneIndex>> bones,
                           std::optional<std::span<const ActionPointIndex>> action_points) noexcept {
  if (!occupied(selections, id)) return Error::kBadId;
  if (vertices && !validMembers(*vertices, selections.vertex_masks.size())) return Error::kBadVertexMember;
  if (bones && !validMembers(*bones, selections.bone_masks.size())) return Error::kBadBoneMember;
  if (action_points && !validMembers(*action_points, selections.action_point_masks.size()))
    return Error::kBadActionPointMember;
  return Error::kNone;
}

bool validName(std::string_view name) noexcept {
  return isIdentifier(name, {.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength});
}

Error validateSelectionAppend(const SelectionsData& selections) noexcept {
  return selections.occupied == std::numeric_limits<SelectionMask>::max() ? Error::kTooManySelections : Error::kNone;
}

Error validateSelection(const Selection& selection, std::size_t bone_count) noexcept {
  if (!validName(selection.name)) return Error::kBadName;
  if (!selection.leading_vertex) return Error::kNone;
  if (!math::finite(selection.leading_vertex->position)) return Error::kBadLeadingPosition;
  if (!skeleton::validBoneIndex(selection.leading_vertex->bone, bone_count)) return Error::kBadLeadingBone;
  return Error::kNone;
}

Error validateBoneReferences(const SelectionsData& selections, std::size_t bone_count) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    if (!occupied(selections, id)) continue;
    const std::optional<SelectionLeadingVertex>& leading = selections.slots[id].leading_vertex;
    if (leading && !skeleton::validBoneIndex(leading->bone, bone_count)) return Error::kBadLeadingBone;
  }
  return Error::kNone;
}

Error validate(const SelectionsData& selections, std::size_t vertex_count, std::size_t bone_count,
               std::size_t action_point_count) {
  if (selections.vertex_masks.size() != vertex_count || selections.bone_masks.size() != bone_count ||
      selections.action_point_masks.size() != action_point_count) {
    log(ARX_LOG_DEBUG,
        "Selection validation: membership counts vertices {}/{}, bones {}/{}, action points {}/{}",
        selections.vertex_masks.size(),
        vertex_count,
        selections.bone_masks.size(),
        bone_count,
        selections.action_point_masks.size(),
        action_point_count);
    return Error::kBadCount;
  }
  if (!validMasks(selections.vertex_masks, selections.occupied)) {
    log(ARX_LOG_DEBUG, "Selection validation: vertex membership references an unoccupied slot");
    return Error::kBadMask;
  }
  if (!validMasks(selections.bone_masks, selections.occupied)) {
    log(ARX_LOG_DEBUG, "Selection validation: bone membership references an unoccupied slot");
    return Error::kBadMask;
  }
  if (!validMasks(selections.action_point_masks, selections.occupied)) {
    log(ARX_LOG_DEBUG, "Selection validation: action-point membership references an unoccupied slot");
    return Error::kBadMask;
  }
  if ((selections.origin_mask & selections.occupied) != selections.origin_mask) {
    log(ARX_LOG_DEBUG, "Selection validation: origin membership references an unoccupied slot");
    return Error::kBadMask;
  }

  std::unordered_set<std::string_view> names;
  names.reserve(64);
  for (SelectionId id = 0; id < 64U; ++id) {
    const Selection& selection = selections.slots[id];
    if (!occupied(selections, id)) {
      if (!selection.name.empty() || selection.leading_vertex) {
        log(ARX_LOG_DEBUG, "Selection validation: unoccupied slot {} retains data", id);
        return Error::kBadMask;
      }
      continue;
    }
    const Error error = validateSelection(selection, bone_count);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Selection validation: slot {} '{}' is invalid: error {}",
          id,
          selection.name,
          static_cast<int>(error));
      return error;
    }
    if (!names.insert(selection.name).second) {
      log(ARX_LOG_DEBUG, "Selection validation: slot {} duplicates name '{}'", id, selection.name);
      return Error::kDuplicateName;
    }
  }
  return Error::kNone;
}

}  // namespace pistoris::selections
