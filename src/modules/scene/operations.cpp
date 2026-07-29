// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/scene.h"
#include "utils/unique_name.h"

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pistoris::scene {
namespace {

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

std::string nameKey(std::string_view name, bool ascii_insensitive) {
  std::string key(name);
  if (ascii_insensitive) {
    for (char& value : key) value = lowerAscii(value);
  }
  return key;
}

std::string uniqueName(std::string_view requested, const std::unordered_set<std::string>& unavailable,
                       bool ascii_insensitive) {
  if (!unavailable.contains(nameKey(requested, ascii_insensitive))) return std::string(requested);

  std::string prefix(requested);
  if (!prefix.ends_with('_')) prefix.push_back('_');
  for (std::size_t suffix = 1;; ++suffix) {
    std::string candidate = prefix + std::to_string(suffix);
    if (!unavailable.contains(nameKey(candidate, ascii_insensitive))) return candidate;
  }
}

template <class Value>
std::size_t makeNamesUnique(std::span<Value> values, bool ascii_insensitive, bool ignore_empty = false) {
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(values.size());
  for (const Value& value : values) {
    if (!ignore_empty || !value.name.empty()) unavailable.insert(nameKey(value.name, ascii_insensitive));
  }

  std::unordered_set<std::string> assigned;
  assigned.reserve(values.size());
  std::size_t renamed = 0;
  for (Value& value : values) {
    if (ignore_empty && value.name.empty()) continue;
    const std::string key = nameKey(value.name, ascii_insensitive);
    if (assigned.insert(key).second) continue;
    value.name = uniqueName(value.name, unavailable, ascii_insensitive);
    const std::string unique_key = nameKey(value.name, ascii_insensitive);
    unavailable.insert(unique_key);
    assigned.insert(unique_key);
    ++renamed;
  }
  return renamed;
}

std::string entityNameCandidate(const Entity& entity) {
  if (!entity.name.empty()) return entity.name;

  std::string_view name = entity.class_path;
  const std::size_t separator = name.find_last_of("/\\");
  if (separator != std::string_view::npos) name.remove_prefix(separator + 1);
  constexpr std::string_view kBaseSuffix = "_base";
  if (name.size() > kBaseSuffix.size() && name.ends_with(kBaseSuffix)) name.remove_suffix(kBaseSuffix.size());
  return std::string(name);
}

void makeEntityNameUniqueImpl(Entity& entity, std::span<const Entity> entities, std::size_t ignored_index) {
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(entities.size());
  for (std::size_t i = 0; i < entities.size(); ++i) {
    if (i != ignored_index) unavailable.insert(entityNameCandidate(entities[i]));
  }
  entity.name = makeUniqueName(entityNameCandidate(entity), unavailable);
}

}  // namespace

void makeEntityNameUnique(Entity& entity, std::span<const Entity> entities) {
  makeEntityNameUniqueImpl(entity, entities, entities.size());
}

void makeEntityNameUnique(Entity& entity, std::span<const Entity> entities, std::size_t ignored_index) {
  makeEntityNameUniqueImpl(entity, entities, ignored_index);
}

void makeEntityNamesUnique(std::span<Entity> entities) {
  std::vector<std::string> candidates;
  candidates.reserve(entities.size());
  std::unordered_set<std::string> unavailable;
  unavailable.reserve(entities.size());
  for (const Entity& entity : entities) {
    candidates.push_back(entityNameCandidate(entity));
    unavailable.insert(candidates.back());
  }

  std::unordered_set<std::string> assigned;
  assigned.reserve(entities.size());
  for (std::size_t i = 0; i < entities.size(); ++i) {
    std::string& candidate = candidates[i];
    if (assigned.insert(candidate).second) {
      entities[i].name = std::move(candidate);
      continue;
    }
    entities[i].name = makeUniqueName(candidate, unavailable);
    unavailable.insert(entities[i].name);
    assigned.insert(entities[i].name);
  }
}

std::size_t makeFogNamesUnique(std::span<Fog> fogs) { return makeNamesUnique(fogs, false, true); }

std::size_t makeZoneNamesUnique(std::span<Zone> zones) { return makeNamesUnique(zones, true); }

std::size_t makePathNamesUnique(std::span<Path> paths) { return makeNamesUnique(paths, true); }

}  // namespace pistoris::scene
