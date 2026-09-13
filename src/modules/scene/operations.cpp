// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/scene.h"
#include "utils/identifier.h"
#include "utils/path.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::scene {
namespace {

template <class Value>
std::size_t repairNames(std::span<Value> values, IdentifierPolicy policy, bool preserve_empty = false) {
  IdentifierUniquifier names(policy);
  names.reserve(values.size());
  for (Value& value : values)
    if (!preserve_empty || !value.name.empty()) names.add(value.name);
  const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
  return summary.changed;
}

template <class Value>
void repairName(const std::vector<Value>& existing, Value& candidate, std::size_t ignored, IdentifierPolicy policy,
                bool preserve_empty = false) {
  if (preserve_empty && candidate.name.empty()) return;
  IdentifierUniquifier names(policy);
  names.reserve(1, existing.size());
  for (std::size_t index = 0; index < existing.size(); ++index) {
    if (index == ignored || (preserve_empty && existing[index].name.empty())) continue;
    names.occupy(existing[index].name);
  }
  names.add(candidate.name);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
}

std::string entityNameCandidate(const Entity& entity) {
  if (!entity.name.empty()) return entity.name;

  std::string_view name = pathFilename(entity.class_path);
  constexpr std::string_view kBaseSuffix = "_base";
  if (name.size() > kBaseSuffix.size() && name.ends_with(kBaseSuffix)) name.remove_suffix(kBaseSuffix.size());
  return std::string(name);
}

void repairEntityNameImpl(Entity& entity, std::span<const Entity> entities, std::size_t ignored_index) {
  IdentifierUniquifier names;
  names.reserve(1, entities.size());
  for (std::size_t i = 0; i < entities.size(); ++i) {
    if (i != ignored_index) names.occupy(entityNameCandidate(entities[i]));
  }
  std::string candidate = entityNameCandidate(entity);
  names.add(candidate);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
  entity.name = std::move(candidate);
}

}  // namespace

void setPlayerSpawn(SceneData& scene, PlayerSpawn player_spawn) noexcept { scene.player_spawn = player_spawn; }

void clearPlayerSpawn(SceneData& scene) noexcept { scene.player_spawn.reset(); }

void repairEntityName(const SceneData& scene, Entity& entity, EntityIndex ignored) {
  repairEntityNameImpl(entity, scene.entities, ignored);
}

void setEntity(SceneData& scene, EntityIndex index, Entity entity) noexcept {
  assert(static_cast<std::size_t>(index) < scene.entities.size());
  scene.entities[index] = std::move(entity);
}

EntityIndex addEntity(SceneData& scene, Entity entity) {
  assert(scene.entities.size() < static_cast<std::size_t>(kInvalidEntityIndex));
  const EntityIndex index = static_cast<EntityIndex>(scene.entities.size());
  scene.entities.push_back(std::move(entity));
  return index;
}

void removeEntity(SceneData& scene, EntityIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < scene.entities.size());
  scene.entities.erase(scene.entities.begin() + static_cast<std::ptrdiff_t>(index));
}

std::size_t repairEntityNames(std::span<Entity> entities) {
  std::vector<std::string> candidates;
  candidates.reserve(entities.size());
  for (const Entity& entity : entities) candidates.push_back(entityNameCandidate(entity));

  IdentifierUniquifier names;
  names.reserve(entities.size());
  for (std::string& candidate : candidates) names.add(candidate);
  [[maybe_unused]] const IdentifierRepairSummary summary = names.apply();
  assert(!summary.exhausted);
  std::size_t changed = 0;
  for (std::size_t index = 0; index < entities.size(); ++index) {
    changed += static_cast<std::size_t>(entities[index].name != candidates[index]);
    entities[index].name = std::move(candidates[index]);
  }
  return changed;
}

void repairFogName(const SceneData& scene, Fog& fog, FogIndex ignored) {
  repairName(scene.fogs, fog, ignored, {.allow_empty = true}, true);
}

std::size_t repairFogNames(std::span<Fog> fogs) { return repairNames(fogs, {.allow_empty = true}, true); }

void setFog(SceneData& scene, FogIndex index, Fog fog) noexcept {
  assert(static_cast<std::size_t>(index) < scene.fogs.size());
  scene.fogs[index] = std::move(fog);
}

FogIndex addFog(SceneData& scene, Fog fog) {
  assert(scene.fogs.size() < static_cast<std::size_t>(kInvalidFogIndex));
  const FogIndex index = static_cast<FogIndex>(scene.fogs.size());
  scene.fogs.push_back(std::move(fog));
  return index;
}

void removeFog(SceneData& scene, FogIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < scene.fogs.size());
  scene.fogs.erase(scene.fogs.begin() + static_cast<std::ptrdiff_t>(index));
}

void repairZoneName(const SceneData& scene, Zone& zone, ZoneIndex ignored) {
  repairName(scene.zones, zone, ignored, {.letter_case = IdentifierCase::kLower});
}

std::size_t repairZoneNames(std::span<Zone> zones) {
  return repairNames(zones, {.letter_case = IdentifierCase::kLower});
}

void setZone(SceneData& scene, ZoneIndex index, Zone zone) noexcept {
  assert(static_cast<std::size_t>(index) < scene.zones.size());
  scene.zones[index] = std::move(zone);
}

ZoneIndex addZone(SceneData& scene, Zone zone) {
  assert(scene.zones.size() < static_cast<std::size_t>(kInvalidZoneIndex));
  const ZoneIndex index = static_cast<ZoneIndex>(scene.zones.size());
  scene.zones.push_back(std::move(zone));
  return index;
}

void removeZone(SceneData& scene, ZoneIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < scene.zones.size());
  scene.zones.erase(scene.zones.begin() + static_cast<std::ptrdiff_t>(index));
}

void repairPathName(const SceneData& scene, Path& path, PathIndex ignored) {
  repairName(scene.paths, path, ignored, {.letter_case = IdentifierCase::kLower});
}

std::size_t repairPathNames(std::span<Path> paths) {
  return repairNames(paths, {.letter_case = IdentifierCase::kLower});
}

void setPath(SceneData& scene, PathIndex index, Path path) noexcept {
  assert(static_cast<std::size_t>(index) < scene.paths.size());
  scene.paths[index] = std::move(path);
}

PathIndex addPath(SceneData& scene, Path path) {
  assert(scene.paths.size() < static_cast<std::size_t>(kInvalidPathIndex));
  const PathIndex index = static_cast<PathIndex>(scene.paths.size());
  scene.paths.push_back(std::move(path));
  return index;
}

void removePath(SceneData& scene, PathIndex index) noexcept {
  assert(static_cast<std::size_t>(index) < scene.paths.size());
  scene.paths.erase(scene.paths.begin() + static_cast<std::ptrdiff_t>(index));
}

}  // namespace pistoris::scene
