// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"

#include "paths/internal.h"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::paths {

using namespace detail;

std::span<const AnimationPathType> animationPathTypes() noexcept { return kAnimationTypes; }

std::string_view animationPathTypeName(AnimationPathType type) noexcept {
  for (std::size_t index = 0; index < kAnimationTypes.size(); ++index)
    if (type == kAnimationTypes[index]) return kAnimationTypeNames[index];
  return {};
}

bool animationPathTypeFromName(std::string_view name, AnimationPathType& out) noexcept {
  const AnimationPathType parsed = canonicalAnimationType(name);
  if (parsed == AnimationPathType::kNone) return false;
  out = parsed;
  return true;
}

bool animationDirectory(ModelPathType type, std::string& out) { return animationDirectory(animationType(type), out); }

bool animationDirectory(AnimationPathType type, std::string& out) {
  const std::string_view normalized_type = animationPathTypeName(type);
  if (normalized_type.empty()) return false;
  out = "graph/obj3d/anims/";
  out.append(normalized_type);
  return true;
}

bool animationTea(AnimationPathView animation, std::string& out) {
  const std::string_view type = animationPathTypeName(animation.type);
  std::string_view name;
  if (type.empty() || !normalizeName(animation.name, {".tea"}, name)) return false;

  std::string result = "graph/obj3d/anims/";
  result.append(type);
  result.push_back('/');
  result.append(name);
  result += ".tea";
  out = std::move(result);
  return true;
}

bool animationFromTea(std::string_view path, AnimationPathView& out) noexcept {
  std::array<std::string_view, 5> components;
  std::size_t count = 0;
  if (!resourcePathComponents(path, components, count) || count != components.size() ||
      !fixedComponents(components, count, {"graph", "obj3d", "anims"}))
    return false;

  const AnimationPathType normalized_type = canonicalAnimationType(components[3]);
  std::string_view stem;
  if (normalized_type == AnimationPathType::kNone || !stripOptionalExtension(components[4], {".tea"}, stem))
    return false;

  out = {normalized_type, stem};
  return true;
}

bool animationSelector(AnimationPathView animation, std::string& out) {
  const std::string_view type = animationPathTypeName(animation.type);
  std::string_view name;
  if (type.empty() || !normalizeName(animation.name, {".tea"}, name)) return false;
  std::string result = "anim:";
  result.append(type);
  result.push_back(':');
  result.append(name);
  out = std::move(result);
  return true;
}

bool animationFromSelector(std::string_view selector, AnimationPathView& out) noexcept {
  std::array<std::string_view, 3> fields;
  std::size_t count = 0;
  if (!splitSelector(selector, fields, count) || count != fields.size() || !selectorKind(fields[0], "anim"))
    return false;
  const AnimationPathType type = canonicalAnimationType(fields[1]);
  std::string_view name;
  if (type == AnimationPathType::kNone || !normalizeName(fields[2], {".tea"}, name)) return false;
  out = {type, name};
  return true;
}

bool animationSearchLocation(AnimationPathType type, ResourceSearchLocation& out) noexcept {
  if (type == AnimationPathType::kNpc) {
    out = {"graph/obj3d/anims/npc", 1};
    return true;
  }
  if (type == AnimationPathType::kFixInter) {
    out = {"graph/obj3d/anims/fix_inter", 1};
    return true;
  }
  return false;
}

}  // namespace pistoris::paths
