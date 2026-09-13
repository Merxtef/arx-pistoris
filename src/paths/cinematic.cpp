// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"

#include "paths/internal.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::paths {

using namespace detail;

bool cinematicCin(CinematicPathView cinematic, std::string& out) {
  std::string_view name;
  if (!normalizeName(cinematic.name, {".cin"}, name)) return false;
  std::string result = "graph/interface/illustrations/";
  result.append(name);
  result += ".cin";
  out = std::move(result);
  return true;
}

bool cinematicFromCin(std::string_view path, CinematicPathView& out) noexcept {
  std::array<std::string_view, 4> components;
  std::size_t count = 0;
  std::string_view name;
  if (!resourcePathComponents(path, components, count) || count != components.size() ||
      !fixedComponents(components, count, {"graph", "interface", "illustrations"}) ||
      !stripOptionalExtension(components[3], {".cin"}, name))
    return false;
  out = {name};
  return true;
}

bool cinematicSelector(CinematicPathView cinematic, std::string& out) {
  std::string_view name;
  if (!normalizeName(cinematic.name, {".cin"}, name)) return false;
  std::string result = "cinematic:";
  result.append(name);
  out = std::move(result);
  return true;
}

bool cinematicFromSelector(std::string_view selector, CinematicPathView& out) noexcept {
  std::array<std::string_view, 2> fields;
  std::size_t count = 0;
  std::string_view name;
  if (!splitSelector(selector, fields, count) || count != fields.size() || !selectorKind(fields[0], "cinematic") ||
      !normalizeName(fields[1], {".cin"}, name))
    return false;
  out = {name};
  return true;
}

ResourceSearchLocation cinematicSearchLocation() noexcept { return {"graph/interface/illustrations", 1}; }

}  // namespace pistoris::paths
