// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstdint>

namespace pistoris {

enum class AnimationElement : std::uint8_t {
  kResource,
  kKeyframe,
  kGroup,
  kGroupTransform,
  kSound,
};

using AnimationLocation = ResourceLocation<AnimationElement>;

template <class T>
using AnimationResult = Result<T, AnimationLocation>;

}  // namespace pistoris
