// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/cinematic.hpp"

#include "modules/cinematic.h"
#include "modules/resource.h"
#include "modules/sounds.h"
#include "modules/textures.h"

namespace pistoris {

struct CinematicModules {  // NOLINT(bugprone-exception-escape): MSVC debug STL container move
  ResourceData resource;
  TexturesData textures;
  SoundsData sounds;
  CinematicData cinematic;
};

struct Cinematic::Data : CinematicModules {};

}  // namespace pistoris
