// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime/types.h"

#include "modules/scene.h"
#include "paths/entity_class.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/rotation.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pistoris::scene {
namespace {

constexpr double kZoneZeroEdgeTolerance = 1.0e-4;

using math::finite;

bool validPathNodeType(PathNodeType type) {
  switch (type) {
    case PathNodeType::kStandard:
    case PathNodeType::kBezier:
      return true;
  }
  return false;
}

}  // namespace

Error validatePlayerSpawn(const PlayerSpawn& player_spawn) {
  if (!finite(player_spawn.position) || !math::validRotation(player_spawn.rotation)) return Error::kBadPlayerSpawn;
  return Error::kNone;
}

Error validatePlayerSpawn(const SceneData& scene) {
  return scene.player_spawn.has_value() ? validatePlayerSpawn(scene.player_spawn.value()) : Error::kNone;
}

Error validateEntity(const Entity& entity) {
  if (!isIdentifier(entity.name, {.allow_empty = true})) return Error::kBadEntityName;
  std::string normalized_path;
  std::string_view removed_extension;
  if (!normalizeEntityClassPath(entity.class_path, normalized_path, removed_extension) ||
      normalized_path != entity.class_path)
    return Error::kBadEntityClassPath;
  if (!finite(entity.position)) return Error::kBadEntityPosition;
  if (!math::validRotation(entity.rotation)) return Error::kBadEntityRotation;
  return Error::kNone;
}

Error validateEntityCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidEntityIndex) ? Error::kTooManyEntities : Error::kNone;
}

Error validateEntities(std::span<const Entity> entities) {
  if (entities.size() > static_cast<std::size_t>(kInvalidEntityIndex)) {
    log(ARX_LOG_DEBUG,
        "Scene validation: entity count {} exceeds limit {}",
        entities.size(),
        static_cast<std::size_t>(kInvalidEntityIndex));
    return Error::kTooManyEntities;
  }
  std::unordered_set<std::string_view> names;
  names.reserve(entities.size());
  for (std::size_t index = 0; index < entities.size(); ++index) {
    const Entity& entity = entities[index];
    Error error = validateEntity(entity);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Scene validation: entity {} '{}' with class path '{}' is invalid: error {}",
          index,
          entity.name,
          entity.class_path,
          static_cast<int>(error));
      return error;
    }
    if (entity.name.empty()) {
      log(ARX_LOG_DEBUG, "Scene validation: entity {} has empty name", index);
      return Error::kBadEntityName;
    }
    if (!names.insert(entity.name).second) {
      log(ARX_LOG_DEBUG, "Scene validation: entity {} duplicates name '{}'", index, entity.name);
      return Error::kDuplicateEntityName;
    }
  }
  return Error::kNone;
}

Error validateFog(const Fog& fog) {
  if (!isIdentifier(fog.name, {.allow_empty = true})) return Error::kBadFogName;
  if (!finite(fog.position)) return Error::kBadFogPosition;
  if (!math::validRotation(fog.rotation)) return Error::kBadFogRotation;
  if (!finite(fog.color)) return Error::kBadFogColor;
  if (!finite(fog.size) || !finite(fog.scale) || !finite(fog.speed) || !finite(fog.rotate_speed) ||
      !finite(fog.frequency))
    return Error::kBadFogEffect;
  return Error::kNone;
}

Error validateFogCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidFogIndex) ? Error::kTooManyFogs : Error::kNone;
}

Error validateFogs(std::span<const Fog> fogs) {
  if (fogs.size() > static_cast<std::size_t>(kInvalidFogIndex)) {
    log(ARX_LOG_DEBUG,
        "Scene validation: fog count {} exceeds limit {}",
        fogs.size(),
        static_cast<std::size_t>(kInvalidFogIndex));
    return Error::kTooManyFogs;
  }
  std::unordered_set<std::string_view> names;
  names.reserve(fogs.size());
  for (std::size_t index = 0; index < fogs.size(); ++index) {
    const Fog& fog = fogs[index];
    Error error = validateFog(fog);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Scene validation: fog {} '{}' is invalid: error {}",
          index,
          fog.name,
          static_cast<int>(error));
      return error;
    }
    if (!fog.name.empty() && !names.insert(fog.name).second) {
      log(ARX_LOG_DEBUG, "Scene validation: fog {} duplicates name '{}'", index, fog.name);
      return Error::kDuplicateFogName;
    }
  }
  return Error::kNone;
}

Error validateZone(const Zone& zone) {
  constexpr double kZeroEdgeSquared = kZoneZeroEdgeTolerance * kZoneZeroEdgeTolerance;
  if (!isIdentifier(zone.name, {.letter_case = IdentifierCase::kLower})) return Error::kBadZoneName;
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

Error validateZoneCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidZoneIndex) ? Error::kTooManyZones : Error::kNone;
}

Error validateZones(std::span<const Zone> zones) {
  if (zones.size() > static_cast<std::size_t>(kInvalidZoneIndex)) {
    log(ARX_LOG_DEBUG,
        "Scene validation: zone count {} exceeds limit {}",
        zones.size(),
        static_cast<std::size_t>(kInvalidZoneIndex));
    return Error::kTooManyZones;
  }
  std::unordered_set<std::string_view> names;
  names.reserve(zones.size());
  for (std::size_t index = 0; index < zones.size(); ++index) {
    const Zone& zone = zones[index];
    Error error = validateZone(zone);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Scene validation: zone {} '{}' is invalid: error {}",
          index,
          zone.name,
          static_cast<int>(error));
      return error;
    }
    if (!names.insert(zone.name).second) {
      log(ARX_LOG_DEBUG, "Scene validation: zone {} duplicates name '{}'", index, zone.name);
      return Error::kDuplicateZoneName;
    }
  }
  return Error::kNone;
}

Error validatePath(const Path& path) {
  if (!isIdentifier(path.name, {.letter_case = IdentifierCase::kLower})) return Error::kBadPathName;
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

Error validatePathCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidPathIndex) ? Error::kTooManyPaths : Error::kNone;
}

Error validatePaths(std::span<const Path> paths) {
  if (paths.size() > static_cast<std::size_t>(kInvalidPathIndex)) {
    log(ARX_LOG_DEBUG,
        "Scene validation: path count {} exceeds limit {}",
        paths.size(),
        static_cast<std::size_t>(kInvalidPathIndex));
    return Error::kTooManyPaths;
  }
  std::unordered_set<std::string_view> names;
  names.reserve(paths.size());
  for (std::size_t index = 0; index < paths.size(); ++index) {
    const Path& path = paths[index];
    Error error = validatePath(path);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Scene validation: path {} '{}' is invalid: {} nodes, error {}",
          index,
          path.name,
          path.nodes.size(),
          static_cast<int>(error));
      return error;
    }
    if (!names.insert(path.name).second) {
      log(ARX_LOG_DEBUG, "Scene validation: path {} duplicates name '{}'", index, path.name);
      return Error::kDuplicatePathName;
    }
  }
  return Error::kNone;
}

Error validate(const SceneData& scene) {
  Error error = validatePlayerSpawn(scene);
  if (error != Error::kNone) {
    log(ARX_LOG_DEBUG, "Scene validation: player spawn is invalid");
    return error;
  }
  error = validateEntities(scene.entities);
  if (error != Error::kNone) return error;
  error = validateFogs(scene.fogs);
  if (error != Error::kNone) return error;
  error = validateZones(scene.zones);
  if (error != Error::kNone) return error;
  return validatePaths(scene.paths);
}

}  // namespace pistoris::scene
