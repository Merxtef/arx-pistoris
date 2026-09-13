// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/action_points.h"
#include "modules/skeleton.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>

namespace pistoris::action_points {

Error validateCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidActionPointIndex) ? Error::kTooManyActionPoints : Error::kNone;
}

Error validatePoint(const ActionPoint& point, std::size_t bone_count) noexcept {
  if (!isIdentifier(point.name, {.letter_case = IdentifierCase::kLower, .max_length = kMaxNameLength}))
    return Error::kBadName;
  if (!math::finite(point.position)) return Error::kBadPosition;
  if (!skeleton::validBoneIndex(point.bone, bone_count)) return Error::kBadBone;
  return Error::kNone;
}

Error validateBoneReferences(const ActionPointsData& actions, std::size_t bone_count) noexcept {
  for (const ActionPoint& point : actions.points) {
    if (!skeleton::validBoneIndex(point.bone, bone_count)) return Error::kBadBone;
  }
  return Error::kNone;
}

Error validate(const ActionPointsData& actions, std::size_t bone_count) noexcept {
  const Error count_error = validateCount(actions.points.size());
  if (count_error != Error::kNone) {
    log(ARX_LOG_DEBUG,
        "Action-point validation: count {} exceeds limit {}",
        actions.points.size(),
        static_cast<std::size_t>(kInvalidActionPointIndex));
    return count_error;
  }

  for (std::size_t index = 0; index < actions.points.size(); ++index) {
    const ActionPoint& point = actions.points[index];
    Error error = validatePoint(point, bone_count);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Action-point validation: point {} '{}' is invalid: bone {}, error {}",
          index,
          point.name,
          point.bone,
          static_cast<int>(error));
      return error;
    }
  }
  return Error::kNone;
}

}  // namespace pistoris::action_points
