// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.hpp"

#include "modules/ambiance.h"
#include "modules/resource.h"
#include "modules/sounds.h"

namespace pistoris {

struct AmbianceModules {
  ResourceData resource;
  SoundsData sounds;
  AmbianceData ambiance;
};

struct Ambiance::Data : AmbianceModules {};

}  // namespace pistoris
