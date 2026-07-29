// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/paths.hpp"

#include "arx/resource_path.h"
#include "modules/scene.h"
#include "utils/math/finite.h"
#include "utils/name_tokens.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pistoris::scene {
namespace {

std::string asciiLower(std::string_view value) {
  std::string result(value);
  for (char& character : result) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
  }
  return result;
}

constexpr double kRotationMinimumLength = 1.0e-6;
constexpr double kRotationUnitTolerance = 1.0e-4;

double rotationLengthSquared(const ArxQuat& rotation) noexcept {
  return static_cast<double>(rotation.w) * rotation.w + static_cast<double>(rotation.x) * rotation.x +
         static_cast<double>(rotation.y) * rotation.y + static_cast<double>(rotation.z) * rotation.z;
}

bool fallbackPlayerSpawnValue(const PlayerSpawn& player_spawn) noexcept {
  return player_spawn.position.x == 0.0f && player_spawn.position.y == 0.0f && player_spawn.position.z == 0.0f &&
         player_spawn.rotation.w == 1.0f && player_spawn.rotation.x == 0.0f && player_spawn.rotation.y == 0.0f &&
         player_spawn.rotation.z == 0.0f;
}

constexpr double kZoneZeroEdgeTolerance = 1.0e-4;

using math::finite;

bool validPathNodeType(PathNodeType type) {
  switch (type) {
    case PathNodeType::kStandard:
    case PathNodeType::kBezier:
    case PathNodeType::kControlPoint:
      return true;
  }
  return false;
}

bool validRotation(const ArxQuat& rotation) noexcept {
  if (!finite(rotation)) return false;
  double length_squared = rotationLengthSquared(rotation);
  if (!std::isfinite(length_squared)) return false;
  return std::abs(std::sqrt(length_squared) - 1.0) <= kRotationUnitTolerance;
}

}  // namespace

bool normalizeRotation(ArxQuat& rotation) noexcept {
  if (!finite(rotation)) return false;
  double length_squared = rotationLengthSquared(rotation);
  if (!std::isfinite(length_squared) || length_squared <= kRotationMinimumLength * kRotationMinimumLength) return false;
  double inverse_length = 1.0 / std::sqrt(length_squared);
  rotation = {static_cast<float>(static_cast<double>(rotation.w) * inverse_length),
              static_cast<float>(static_cast<double>(rotation.x) * inverse_length),
              static_cast<float>(static_cast<double>(rotation.y) * inverse_length),
              static_cast<float>(static_cast<double>(rotation.z) * inverse_length)};
  return finite(rotation);
}

Error validatePlayerSpawn(const PlayerSpawn& player_spawn) {
  if (!finite(player_spawn.position) || !validRotation(player_spawn.rotation)) return Error::kBadPlayerSpawn;
  return Error::kNone;
}

Error validatePlayerSpawn(const SceneData& scene) {
  Error error = validatePlayerSpawn(scene.player_spawn);
  if (error != Error::kNone) return error;
  if (scene.player_spawn_is_fallback && !fallbackPlayerSpawnValue(scene.player_spawn)) return Error::kBadPlayerSpawn;
  return Error::kNone;
}

Error setPlayerSpawn(SceneData& scene, PlayerSpawn player_spawn) noexcept {
  if (!normalizeRotation(player_spawn.rotation)) return Error::kBadPlayerSpawn;
  Error error = validatePlayerSpawn(player_spawn);
  if (error != Error::kNone) return error;
  scene.player_spawn = player_spawn;
  scene.player_spawn_is_fallback = false;
  return Error::kNone;
}

void clearPlayerSpawn(SceneData& scene) noexcept {
  scene.player_spawn = {};
  scene.player_spawn_is_fallback = true;
}

Error validateEntity(const Entity& entity) {
  if (!validSemanticString(entity.name)) return Error::kBadEntityName;
  if (!validSemanticString(entity.class_path)) return Error::kBadEntityClassPath;
  std::string normalized_path;
  std::string_view removed_extension;
  if (!normalizeEntityClassPath(entity.class_path, normalized_path, removed_extension) ||
      normalized_path != entity.class_path)
    return Error::kBadEntityClassPath;
  if (!finite(entity.position)) return Error::kBadEntityPosition;
  if (!validRotation(entity.rotation)) return Error::kBadEntityRotation;
  return Error::kNone;
}

Error validateEntities(std::span<const Entity> entities) {
  if (entities.size() > static_cast<std::size_t>(kInvalidEntityIndex)) return Error::kTooManyEntities;
  std::unordered_set<std::string_view> names;
  names.reserve(entities.size());
  for (const Entity& entity : entities) {
    Error error = validateEntity(entity);
    if (error != Error::kNone) return error;
    if (entity.name.empty()) return Error::kBadEntityName;
    if (!names.insert(entity.name).second) return Error::kDuplicateEntityName;
  }
  return Error::kNone;
}

Error validateFog(const Fog& fog) {
  if (!validSemanticString(fog.name)) return Error::kBadFogName;
  if (!finite(fog.position)) return Error::kBadFogPosition;
  if (!validRotation(fog.rotation)) return Error::kBadFogRotation;
  if (!finite(fog.color)) return Error::kBadFogColor;
  if (!finite(fog.size) || !finite(fog.scale) || !finite(fog.speed) || !finite(fog.rotate_speed) ||
      !finite(fog.frequency))
    return Error::kBadFogEffect;
  return Error::kNone;
}

Error validateFogs(std::span<const Fog> fogs) {
  if (fogs.size() > static_cast<std::size_t>(kInvalidFogIndex)) return Error::kTooManyFogs;
  std::unordered_set<std::string_view> names;
  names.reserve(fogs.size());
  for (const Fog& fog : fogs) {
    Error error = validateFog(fog);
    if (error != Error::kNone) return error;
    if (!fog.name.empty() && !names.insert(fog.name).second) return Error::kDuplicateFogName;
  }
  return Error::kNone;
}

Error validateZone(const Zone& zone) {
  constexpr double kZeroEdgeSquared = kZoneZeroEdgeTolerance * kZoneZeroEdgeTolerance;
  if (zone.name.empty() || !validSemanticString(zone.name)) return Error::kBadZoneName;
  if (zone.perimeter_xz.size() < 3 || !finite(zone.reference_y)) return Error::kBadZonePerimeter;
  for (std::size_t i = 0; i < zone.perimeter_xz.size(); ++i) {
    const ArxVector2& current = zone.perimeter_xz[i];
    const ArxVector2& next = zone.perimeter_xz[(i + 1) % zone.perimeter_xz.size()];
    if (!finite(current) || math::lengthSquared(next - current) <= kZeroEdgeSquared) return Error::kBadZonePerimeter;
  }
  switch (zone.height_mode) {
    case ZoneHeightMode::kFinite:
      if (!finite(zone.height) || zone.height <= 0.0f) return Error::kBadZoneHeight;
      break;
    case ZoneHeightMode::kInfinite:
      break;
    default:
      return Error::kBadZoneHeightMode;
  }
  const auto& color = zone.color;
  if (color && !finite(*color)) return Error::kBadZoneColor;
  const auto& farclip = zone.farclip;
  if (farclip && !finite(*farclip)) return Error::kBadZoneFarclip;
  const auto& ambiance = zone.ambiance;
  if (ambiance) {
    std::string normalized;
    if (!paths::normalizeZoneAmbiance(ambiance->name, normalized) || normalized != ambiance->name ||
        !finite(ambiance->volume))
      return Error::kBadZoneAmbiance;
  }
  return Error::kNone;
}

Error validateZones(std::span<const Zone> zones) {
  if (zones.size() > static_cast<std::size_t>(kInvalidZoneIndex)) return Error::kTooManyZones;
  std::unordered_set<std::string> names;
  names.reserve(zones.size());
  for (const Zone& zone : zones) {
    Error error = validateZone(zone);
    if (error != Error::kNone) return error;
    if (!names.insert(asciiLower(zone.name)).second) return Error::kDuplicateZoneName;
  }
  return Error::kNone;
}

Error validatePath(const Path& path) {
  if (path.name.empty() || !validSemanticString(path.name)) return Error::kBadPathName;
  if (!finite(path.position)) return Error::kBadPathPosition;
  if (path.nodes.empty()) return Error::kBadPathNodeCount;
  for (const PathNode& node : path.nodes) {
    if (!finite(node.relative_position)) return Error::kBadPathNodePosition;
    if (!validPathNodeType(node.type)) return Error::kBadPathNodeType;
  }
  const PathNode& first = path.nodes.front();
  if (first.relative_position.x != 0.0f || first.relative_position.y != 0.0f || first.relative_position.z != 0.0f ||
      first.time_ms != 0)
    return Error::kBadPathFirstNode;
  return Error::kNone;
}

Error validatePaths(std::span<const Path> paths) {
  if (paths.size() > static_cast<std::size_t>(kInvalidPathIndex)) return Error::kTooManyPaths;
  std::unordered_set<std::string> names;
  names.reserve(paths.size());
  for (const Path& path : paths) {
    Error error = validatePath(path);
    if (error != Error::kNone) return error;
    if (!names.insert(asciiLower(path.name)).second) return Error::kDuplicatePathName;
  }
  return Error::kNone;
}

Error validate(const SceneData& scene) {
  Error error = validatePlayerSpawn(scene);
  if (error != Error::kNone) return error;
  error = validateEntities(scene.entities);
  if (error != Error::kNone) return error;
  error = validateFogs(scene.fogs);
  if (error != Error::kNone) return error;
  error = validateZones(scene.zones);
  if (error != Error::kNone) return error;
  return validatePaths(scene.paths);
}

}  // namespace pistoris::scene
