// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "modules/animation.h"
#include "utils/math/quat.h"

#include <cassert>
#include <cstddef>

namespace pistoris::animation {

bool isIdentityTransform(const AnimationGroupTransform& transform) noexcept {
  return transform.rotation == math::kIdentityQuat && transform.translation == ArxVector3{} &&
         transform.scale == ArxVector3{1.0f, 1.0f, 1.0f};
}

bool isIdentityGroup(const AnimationData& animation, std::size_t group) noexcept {
  assert(group < animation.group_count);
  for (std::size_t frame = 0; frame < animation.keyframes.size(); ++frame)
    if (!isIdentityTransform(animation.group_transforms[frame * animation.group_count + group])) return false;
  return true;
}

bool isGroupClaimed(const AnimationData& animation, std::size_t group) noexcept {
  assert(group < animation.group_count);
  return animation.claimed_groups.test(group);
}

}  // namespace pistoris::animation
