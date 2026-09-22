// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "names.h"

#include "external/glb/level/lighting.h"
#include "external/glb/level/objects.h"
#include "external/glb/level/zones.h"
#include "external/glb/utils/names.h"

#include <span>
#include <string_view>

namespace pistoris::glb_level {
namespace {

bool singletonRootName(std::string_view name, std::string_view root, glb::ParsedLabel* label) {
  bool parsed = false;
  if (!glb::parseRecoverableLabel(name, parsed, label, {}, [root](std::span<const std::string_view> tokens, bool& out) {
        if (tokens.size() != 1 || tokens.front() != root) return false;
        out = true;
        return true;
      }))
    return false;
  return parsed;
}

bool singletonRootAttempt(std::string_view name, std::string_view root) {
  return name == root || (name.starts_with(root) && name.substr(root.size()).starts_with("__"));
}

}  // namespace

bool isPlayerSpawnRootName(std::string_view name, glb::ParsedLabel* label) {
  return singletonRootName(name, kPlayerSpawnRootName, label);
}

bool isNavSurfaceRootName(std::string_view name, glb::ParsedLabel* label) {
  return singletonRootName(name, kNavSurfaceRootName, label);
}

bool isMinimapRootName(std::string_view name, glb::ParsedLabel* label) {
  return singletonRootName(name, kMinimapRootName, label);
}

LevelObjectKind levelObjectKind(const cgltf_node& node) {
  const std::string_view name = node.name != nullptr ? node.name : "";
  if (isReservedRoomName(name)) return LevelObjectKind::kRoom;
  if (isReservedPortalName(name)) return LevelObjectKind::kPortal;
  if (isReservedAnchorName(name)) return LevelObjectKind::kAnchor;
  if (isReservedLightName(name)) return LevelObjectKind::kLight;
  if (isReservedZoneName(name)) return LevelObjectKind::kZone;
  if (singletonRootAttempt(name, kPlayerSpawnRootName)) return LevelObjectKind::kPlayerSpawn;
  if (name.starts_with("arx_entity__")) return LevelObjectKind::kEntity;
  if (name.starts_with("arx_fog__")) return LevelObjectKind::kFog;
  if (name.starts_with("arx_path__")) return LevelObjectKind::kPath;
  if (singletonRootAttempt(name, kNavSurfaceRootName)) return LevelObjectKind::kNavSurface;
  if (singletonRootAttempt(name, kMinimapRootName)) return LevelObjectKind::kMinimap;
  if (node.light != nullptr && node.light->type == cgltf_light_type_point) return LevelObjectKind::kLight;
  return LevelObjectKind::kNone;
}

bool directLevelHelper(LevelObjectKind owner, std::string_view name) {
  switch (owner) {
    case LevelObjectKind::kLight:
      return name.starts_with("SETTINGS__") || name.starts_with("FLAGS__") || name.starts_with("EFFECT__");
    case LevelObjectKind::kEntity:
      return name.starts_with("CLASS_");
    case LevelObjectKind::kFog:
      return name.starts_with("SETTINGS__") || name == "DIRECTION" || name.starts_with("DIRECTION__");
    case LevelObjectKind::kZone:
      return name.starts_with("SETTINGS__") || name.starts_with("AMBIANCE_");
    case LevelObjectKind::kPath:
      return true;
    default:
      return false;
  }
}

bool levelDiagnosticRootName(std::string_view name) {
  return name.starts_with("fts_cell__") || name.starts_with("fts_room__") || name.starts_with("fts_portal__") ||
         name == "level_render_splits_debug" || name == "level_floor_debug" || name == "navigation_debug" ||
         name == "room_distance_debug";
}

}  // namespace pistoris::glb_level
