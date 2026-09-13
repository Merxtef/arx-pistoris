// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.hpp"

#include "modules/animation.h"
#include "modules/resource.h"
#include "modules/sounds.h"

#include <bitset>

namespace pistoris {

struct AnimationModules {
  ResourceData resource;
  SoundsData sounds;
  AnimationData animation;
};

struct AnimationGroupStateCache {
  std::bitset<animation::kMaxGroups> identity_known;
  std::bitset<animation::kMaxGroups> identity_value;
};

struct Animation::Data : AnimationModules {
  mutable AnimationGroupStateCache group_state_cache;
};

}  // namespace pistoris
