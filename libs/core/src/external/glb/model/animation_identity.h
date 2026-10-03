// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "modules/animation.h"
#include "utils/math/quat.h"

#include <cmath>

namespace pistoris::glb_model {

inline bool isAnimationGlbIdentity(const AnimationGroupTransform& transform) noexcept {
  constexpr float kTolerance = 1.0e-4f;
  const auto zero = [](float value) { return std::abs(value) <= kTolerance; };
  const auto one = [](float value) { return std::abs(value - 1.0f) <= kTolerance; };
  const ArxQuat rotation = math::normalize(transform.rotation);
  return one(rotation.w) && zero(rotation.x) && zero(rotation.y) && zero(rotation.z) && zero(transform.translation.x) &&
         zero(transform.translation.y) && zero(transform.translation.z) && one(transform.scale.x) &&
         one(transform.scale.y) && one(transform.scale.z);
}

}  // namespace pistoris::glb_model
