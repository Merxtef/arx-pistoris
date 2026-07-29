// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "cgltf/cgltf.h"

#include <cstdint>
#include <string_view>

namespace pistoris::glb_level {

inline constexpr char kPlayerSpawnRootName[] = "arx_player_spawn";
inline constexpr char kExportPlayerSpawnRootName[] = "arx_player_spawn__spawn";
inline constexpr char kNavSurfaceRootName[] = "arx_nav_surface";
inline constexpr char kExportNavSurfaceRootName[] = "arx_nav_surface__surface";

enum class LevelObjectKind : std::uint8_t {
  kNone,
  kRoom,
  kPortal,
  kAnchor,
  kLight,
  kPlayerSpawn,
  kEntity,
  kFog,
  kZone,
  kPath,
  kNavSurface,
};

bool isPlayerSpawnRootName(std::string_view name);
bool isNavSurfaceRootName(std::string_view name);
LevelObjectKind levelObjectKind(const cgltf_node& node);
bool directLevelHelper(LevelObjectKind owner, std::string_view name);
bool levelDiagnosticRootName(std::string_view name);

}  // namespace pistoris::glb_level
