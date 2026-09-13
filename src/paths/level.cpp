// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"

#include "paths/internal.h"
#include "utils/resource_path.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace pistoris::paths {

using namespace detail;

std::string levelDlf(std::uint32_t level) {
  const std::string name = "level" + std::to_string(level);
  return "graph/levels/" + name + "/" + name + ".dlf";
}

std::string levelLlf(std::uint32_t level) {
  const std::string name = "level" + std::to_string(level);
  return "graph/levels/" + name + "/" + name + ".llf";
}

std::string levelFts(std::uint32_t level) { return "game/graph/levels/level" + std::to_string(level) + "/fast.fts"; }

std::uint32_t minimapResourceLevel(std::uint32_t level) noexcept {
  switch (level) {
    case 0:
    case 8:
    case 11:
    case 12:
      return 0;
    case 1:
    case 13:
    case 14:
      return 1;
    case 2:
    case 15:
      return 2;
    case 3:
    case 16:
    case 17:
      return 3;
    case 4:
    case 18:
    case 19:
      return 4;
    case 5:
    case 21:
      return 5;
    case 6:
    case 22:
      return 6;
    case 7:
    case 23:
      return 7;
    default:
      return level;
  }
}

std::string levelMinimap(std::uint32_t level) {
  return "graph/levels/level" + std::to_string(minimapResourceLevel(level)) + "/map";
}

std::string levelLoadingScreen(std::uint32_t level) {
  return "graph/levels/level" + std::to_string(level) + "/loading";
}

std::string_view minimapOffsetsFile() noexcept { return "graph/levels/mini_offsets.ini"; }

bool levelFromDlf(std::string_view path, std::uint32_t& level) noexcept {
  return levelFromSceneFile(path, ".dlf", level);
}

bool levelFromLlf(std::string_view path, std::uint32_t& level) noexcept {
  return levelFromSceneFile(path, ".llf", level);
}

bool levelFromFts(std::string_view path, std::uint32_t& level) noexcept {
  std::array<std::string_view, 5> components;
  std::size_t count = 0;
  if (!resourcePathComponents(path, components, count) || count != components.size() ||
      !fixedComponents(components, count, {"game", "graph", "levels"}))
    return false;

  std::uint32_t parsed = 0;
  std::string_view stem;
  if (!parseLevelComponent(components[3], parsed) || !stripOptionalExtension(components[4], {".fts"}, stem) ||
      !equalAsciiInsensitive(stem, "fast"))
    return false;
  level = parsed;
  return true;
}

bool dlfSceneFromLevelName(std::string_view name, std::string& out) {
  if (!::pistoris::isPortableResourcePathComponent(name)) return false;
  std::string result = "graph/levels/";
  result.append(name);
  out = std::move(result);
  return true;
}

std::string levelSelector(std::uint32_t level) { return "level:" + std::to_string(level); }

bool levelFromSelector(std::string_view selector, std::uint32_t& level) noexcept {
  std::array<std::string_view, 2> fields;
  std::size_t count = 0;
  if (!splitSelector(selector, fields, count) || count != fields.size() || !selectorKind(fields[0], "level"))
    return false;

  std::uint32_t parsed = 0;
  const auto result = std::from_chars(fields[1].data(), fields[1].data() + fields[1].size(), parsed);
  if (result.ec != std::errc{} || result.ptr != fields[1].data() + fields[1].size()) return false;
  level = parsed;
  return true;
}

ResourceSearchLocation levelSearchLocation() noexcept { return {"graph/levels", 2}; }

bool ftsFromDlfScene(std::string_view scene_path, std::string& out) {
  std::vector<std::string_view> components;
  if (!splitResourcePath(scene_path, components)) return false;

  std::string result = "game/";
  for (std::string_view component : components) {
    if (result.back() != '/') result.push_back('/');
    result.append(component);
  }
  result += "/fast.fts";
  out = std::move(result);
  return true;
}

}  // namespace pistoris::paths
