// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct PlayerSpawn {
  ArxVector3 position = {};
  ArxQuat rotation = {};
};

struct Entity {
  std::string class_path;
  std::int32_t ident = -1;
  ArxVector3 position = {};
  ArxQuat rotation = {};
  std::string name;
};

struct Fog {
  ArxVector3 position = {};
  ArxColor3 color = {};
  float size = 0.0f;
  bool directional = false;
  float scale = 0.0f;
  ArxQuat rotation = {};
  float speed = 0.0f;
  float rotate_speed = 0.0f;
  std::int32_t lifetime_ms = 0;
  float frequency = 0.0f;
  std::string name;
};

enum class ZoneHeightMode : std::uint8_t {
  kFinite,
  kInfinite,
};

struct ZoneAmbiance {
  std::string name;
  float volume = 100.0f;
};

struct Zone {
  std::string name;
  std::vector<ArxVector2> perimeter_xz;
  float reference_y = 0.0f;
  ZoneHeightMode height_mode = ZoneHeightMode::kFinite;
  float height = 0.0f;
  std::optional<ArxColor3> color;
  std::optional<float> farclip;
  std::optional<ZoneAmbiance> ambiance;
};

enum class PathNodeType : std::uint8_t {
  kStandard,
  kBezier,
  kControlPoint,
};

struct PathNode {
  ArxVector3 relative_position = {};
  PathNodeType type = PathNodeType::kStandard;
  std::uint32_t time_ms = 0;
};

struct Path {
  std::string name;
  ArxVector3 position = {};
  std::vector<PathNode> nodes;
};

struct SceneData {
  PlayerSpawn player_spawn;
  bool player_spawn_is_fallback = true;
  std::vector<Entity> entities;
  std::vector<Fog> fogs;
  std::vector<Zone> zones;
  std::vector<Path> paths;
};

namespace scene {

enum class Error : std::uint8_t {
  kNone,
  kBadPlayerSpawn,
  kTooManyEntities,
  kBadEntityName,
  kDuplicateEntityName,
  kBadEntityClassPath,
  kBadEntityPosition,
  kBadEntityRotation,
  kTooManyFogs,
  kBadFogName,
  kDuplicateFogName,
  kBadFogPosition,
  kBadFogRotation,
  kBadFogColor,
  kBadFogEffect,
  kTooManyZones,
  kBadZoneName,
  kDuplicateZoneName,
  kBadZonePerimeter,
  kBadZoneHeightMode,
  kBadZoneHeight,
  kBadZoneColor,
  kBadZoneFarclip,
  kBadZoneAmbiance,
  kTooManyPaths,
  kBadPathName,
  kDuplicatePathName,
  kBadPathPosition,
  kBadPathNodeCount,
  kBadPathNodePosition,
  kBadPathNodeType,
  kBadPathFirstNode,
};

bool normalizeRotation(ArxQuat& rotation) noexcept;
Error validatePlayerSpawn(const PlayerSpawn& player_spawn);
Error validatePlayerSpawn(const SceneData& scene);
Error setPlayerSpawn(SceneData& scene, PlayerSpawn player_spawn) noexcept;
void clearPlayerSpawn(SceneData& scene) noexcept;
Error validateEntity(const Entity& entity);
Error validateEntities(std::span<const Entity> entities);
void makeEntityNameUnique(Entity& entity, std::span<const Entity> entities);
void makeEntityNameUnique(Entity& entity, std::span<const Entity> entities, std::size_t ignored_index);
void makeEntityNamesUnique(std::span<Entity> entities);
Error validateFog(const Fog& fog);
Error validateFogs(std::span<const Fog> fogs);
std::size_t makeFogNamesUnique(std::span<Fog> fogs);
Error validateZone(const Zone& zone);
Error validateZones(std::span<const Zone> zones);
std::size_t makeZoneNamesUnique(std::span<Zone> zones);
Error validatePath(const Path& path);
Error validatePaths(std::span<const Path> paths);
std::size_t makePathNamesUnique(std::span<Path> paths);
Error validate(const SceneData& scene);

}  // namespace scene
}  // namespace pistoris
