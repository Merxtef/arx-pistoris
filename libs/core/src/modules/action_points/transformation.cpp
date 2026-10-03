// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/action_points.h"
#include "utils/math/finite.h"
#include "utils/math/mat3.h"

namespace pistoris::action_points {

Error validateScale(const ActionPointsData& actions, float factor) noexcept {
  for (const ActionPoint& point : actions.points)
    if (!math::finite(point.position * factor)) return Error::kBadPosition;
  return Error::kNone;
}

void applyScale(ActionPointsData& actions, float factor) noexcept {
  for (ActionPoint& point : actions.points) point.position = point.position * factor;
}

Error validateRotation(const ActionPointsData& actions, const ArxMat3& rotation) noexcept {
  for (const ActionPoint& point : actions.points)
    if (!math::finite(rotation * point.position)) return Error::kBadPosition;
  return Error::kNone;
}

void applyRotation(ActionPointsData& actions, const ArxMat3& rotation) noexcept {
  for (ActionPoint& point : actions.points) point.position = rotation * point.position;
}

Error validateTranslation(const ActionPointsData& actions, const ArxVector3& offset) noexcept {
  for (const ActionPoint& point : actions.points)
    if (!math::finite(point.position + offset)) return Error::kBadPosition;
  return Error::kNone;
}

void applyTranslation(ActionPointsData& actions, const ArxVector3& offset) noexcept {
  for (ActionPoint& point : actions.points) point.position = point.position + offset;
}

}  // namespace pistoris::action_points
