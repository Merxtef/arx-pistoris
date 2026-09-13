// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/action_points.h"
#include "utils/identifier.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string>
#include <utility>

namespace pistoris::action_points {

ActionPointIndex addActionPoint(ActionPointsData& actions, ActionPoint point) {
  assert(actions.points.size() < static_cast<std::size_t>(kInvalidActionPointIndex));
  const std::size_t index = actions.points.size();
  actions.points.push_back(std::move(point));
  return static_cast<ActionPointIndex>(index);
}

void setActionPoint(ActionPointsData& actions, ActionPointIndex index, ActionPoint point) noexcept {
  assert(static_cast<std::size_t>(index) < actions.points.size());
  actions.points[index] = std::move(point);
}

void removeActionPoint(ActionPointsData& actions, ActionPointIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < actions.points.size());
  actions.points.erase(actions.points.begin() + static_cast<std::ptrdiff_t>(index));
}

void replace(ActionPointsData& actions, ActionPointsData&& replacement) noexcept { actions = std::move(replacement); }

void clear(ActionPointsData& actions) noexcept { actions.points.clear(); }

bool referencesBone(const ActionPointsData& actions, BoneIndex bone) noexcept {
  return std::ranges::any_of(actions.points, [bone](const ActionPoint& point) { return point.bone == bone; });
}

void remapBoneIndicesAfterRemoval(ActionPointsData& actions, BoneIndex removed) noexcept {
  for (ActionPoint& point : actions.points)
    if (point.bone != kInvalidBoneIndex && point.bone > removed) --point.bone;
}

void clearBoneReferences(ActionPointsData& actions) noexcept {
  for (ActionPoint& point : actions.points) point.bone = kInvalidBoneIndex;
}

void reserveActionPointCapacity(ActionPointsData& actions, std::size_t capacity) { actions.points.reserve(capacity); }

IdentifierRepair repairName(std::string& name) {
  return repairIdentifier(name, {.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength});
}

}  // namespace pistoris::action_points
