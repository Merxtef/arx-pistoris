// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"

#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/loading_screen.h"
#include "modules/minimap.h"
#include "modules/navigation.h"
#include "modules/resource.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "modules/textures.h"

#include <cstdint>
#include <optional>

namespace pistoris {

struct LevelModules {
  ResourceData resource;
  TexturesData textures;
  GeometryData geometry;
  RoomsData rooms;
  NavigationData navigation;
  LightingData lighting;
  SceneData scene;
  MinimapData minimap;
  LoadingScreenData loading_screen;
};

enum class LevelValidation : std::uint32_t {
  kNone = 0,
  kVertices = 1U << 0,
  kTextures = 1U << 1,
  kFaces = 1U << 2,
  kFaceRooms = 1U << 3,
  kCornerColors = 1U << 4,
  kRooms = 1U << 5,
  kPortals = 1U << 6,
  kRoomDistances = 1U << 7,
  kNavSurface = 1U << 8,
  kAnchors = 1U << 9,
  kAnchorConnections = 1U << 10,
  kLightSources = 1U << 11,
  kPlayerSpawn = 1U << 12,
  kEntities = 1U << 13,
  kFogs = 1U << 14,
  kZones = 1U << 15,
  kPaths = 1U << 16,
  kMinimap = 1U << 17,
  kLoadingScreen = 1U << 18,
};

struct LevelDerivedState {
  std::optional<ArxAabb> bounds;
  std::optional<ArxAabb> referenced_bounds;
};

struct LevelValidationState {
  LevelValidation valid = LevelValidation::kNone;
  LevelDerivedState derived;
};

struct Level::Data : LevelModules {
  mutable LevelValidationState validation;
};

}  // namespace pistoris
