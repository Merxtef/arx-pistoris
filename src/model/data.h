// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/model.hpp"

#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/inventory_icon.h"
#include "modules/resource.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "modules/textures.h"

namespace pistoris {

struct ModelModules {
  ResourceData resource;
  InventoryIconData inventory_icon;
  TexturesData textures;
  GeometryData geometry;
  SkeletonData skeleton;
  ActionPointsData action_points;
  SelectionsData selections;
};

struct Model::Data : ModelModules {};

}  // namespace pistoris
