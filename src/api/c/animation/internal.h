// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation.hpp"  // IWYU pragma: export

#include "api/c/internal.h"

#include <memory>
#include <vector>

struct arx_pistoris_animation {
  pistoris::Animation value;
};

struct arx_pistoris_animation_list {
  std::vector<std::unique_ptr<ArxAnimation>> value;
};

namespace pistoris::c_api {

std::unique_ptr<ArxAnimationList> makeAnimationList(std::vector<std::unique_ptr<pistoris::Animation>>&& animations);

inline bool valid(const ArxAnimationKeyframe&) noexcept { return true; }

inline bool valid(const ArxAnimationKeyframeInput& keyframe) noexcept {
  return valid(keyframe.keyframe) && valid(keyframe.group_transforms, keyframe.group_count);
}

}  // namespace pistoris::c_api
