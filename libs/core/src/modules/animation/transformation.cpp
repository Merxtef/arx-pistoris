// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/animation.h"
#include "utils/math/finite.h"
#include "utils/math/mat3.h"
#include "utils/math/quat.h"
#include "utils/math/rotation.h"

#include <cassert>

namespace pistoris::animation {

Error validateScale(const AnimationData& animation, float factor) noexcept {
  for (const AnimationKeyframe& keyframe : animation.keyframes)
    if (!math::finite(keyframe.root_translation * factor)) return Error::kBadRootTransform;
  for (const AnimationGroupTransform& transform : animation.group_transforms)
    if (!math::finite(transform.translation * factor)) return Error::kBadGroupTransform;
  return Error::kNone;
}

void applyScale(AnimationData& animation, float factor) noexcept {
  for (AnimationKeyframe& keyframe : animation.keyframes)
    keyframe.root_translation = keyframe.root_translation * factor;
  for (AnimationGroupTransform& transform : animation.group_transforms)
    transform.translation = transform.translation * factor;
}

Error validateRotation(const AnimationData& animation, ArxQuat rotation) noexcept {
  assert(math::validRotation(rotation));
  const ArxMat3 matrix = math::quatToRotation(rotation);
  const ArxQuat inverse = math::conjugate(rotation);
  for (const AnimationKeyframe& keyframe : animation.keyframes) {
    if (!math::finite(matrix * keyframe.root_translation) || !math::finite(rotation * keyframe.root_rotation * inverse))
      return Error::kBadRootTransform;
  }
  for (const AnimationGroupTransform& transform : animation.group_transforms) {
    if (!math::finite(matrix * transform.translation) || !math::finite(rotation * transform.rotation * inverse))
      return Error::kBadGroupTransform;
  }
  return Error::kNone;
}

void applyRotation(AnimationData& animation, ArxQuat rotation) noexcept {
  assert(math::validRotation(rotation));
  const ArxMat3 matrix = math::quatToRotation(rotation);
  const ArxQuat inverse = math::conjugate(rotation);
  for (AnimationKeyframe& keyframe : animation.keyframes) {
    keyframe.root_translation = matrix * keyframe.root_translation;
    keyframe.root_rotation = rotation * keyframe.root_rotation * inverse;
  }
  for (AnimationGroupTransform& transform : animation.group_transforms) {
    transform.translation = matrix * transform.translation;
    transform.rotation = math::canonicalizeQuaternionSign(rotation * transform.rotation * inverse);
  }
}

}  // namespace pistoris::animation
