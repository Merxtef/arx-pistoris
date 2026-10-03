// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

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
  std::optional<PlayerSpawn> player_spawn;
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
  kBadIndex,
};

// --- Validation ---

Error validatePlayerSpawn(const PlayerSpawn& player_spawn);
Error validatePlayerSpawn(const SceneData& scene);
Error validateEntityCount(std::size_t count) noexcept;
Error validateEntity(const Entity& entity);
Error validateEntities(std::span<const Entity> entities);
Error validateFogCount(std::size_t count) noexcept;
Error validateFog(const Fog& fog);
Error validateFogs(std::span<const Fog> fogs);
Error validateZoneCount(std::size_t count) noexcept;
Error validateZone(const Zone& zone);
Error validateZones(std::span<const Zone> zones);
Error validatePathCount(std::size_t count) noexcept;
Error validatePath(const Path& path);
Error validatePaths(std::span<const Path> paths);
Error validate(const SceneData& scene);

// --- Mutation ---

void setPlayerSpawn(SceneData& scene, PlayerSpawn player_spawn) noexcept;
void clearPlayerSpawn(SceneData& scene) noexcept;
void setEntity(SceneData& scene, EntityIndex index, Entity entity) noexcept;
EntityIndex addEntity(SceneData& scene, Entity entity);
void removeEntity(SceneData& scene, EntityIndex index) noexcept;
void setFog(SceneData& scene, FogIndex index, Fog fog) noexcept;
FogIndex addFog(SceneData& scene, Fog fog);
void removeFog(SceneData& scene, FogIndex index) noexcept;
void setZone(SceneData& scene, ZoneIndex index, Zone zone) noexcept;
ZoneIndex addZone(SceneData& scene, Zone zone);
void removeZone(SceneData& scene, ZoneIndex index) noexcept;
void setPath(SceneData& scene, PathIndex index, Path path) noexcept;
PathIndex addPath(SceneData& scene, Path path);
void removePath(SceneData& scene, PathIndex index) noexcept;

// --- Repair ---

void repairEntityName(const SceneData& scene, Entity& entity, EntityIndex ignored = kInvalidEntityIndex);
std::size_t repairEntityNames(std::span<Entity> entities);
void repairFogName(const SceneData& scene, Fog& fog, FogIndex ignored = kInvalidFogIndex);
std::size_t repairFogNames(std::span<Fog> fogs);
void repairZoneName(const SceneData& scene, Zone& zone, ZoneIndex ignored = kInvalidZoneIndex);
std::size_t repairZoneNames(std::span<Zone> zones);
void repairPathName(const SceneData& scene, Path& path, PathIndex ignored = kInvalidPathIndex);
std::size_t repairPathNames(std::span<Path> paths);

}  // namespace scene
}  // namespace pistoris
