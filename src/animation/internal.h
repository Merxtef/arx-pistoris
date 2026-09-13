// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/status.h"

#include "modules/animation.h"
#include "modules/sounds.h"

namespace pistoris {

struct AnimationModules;

namespace animation_detail {

ArxReturnCode errorCode(animation::Error error) noexcept;
ArxReturnCode soundErrorCode(sounds::Error error) noexcept;
ArxReturnCode validateStructure(const AnimationModules& modules) noexcept;
ArxAnimationKeyframe publicKeyframe(const AnimationKeyframe& source) noexcept;
ArxAnimationGroupTransform publicTransform(const AnimationGroupTransform& source) noexcept;

}  // namespace animation_detail
}  // namespace pistoris
