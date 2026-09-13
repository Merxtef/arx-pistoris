// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "paths/entity_class.h"
#include "paths/internal.h"

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::paths {

using namespace detail;

namespace {

constexpr ArxEntityClassKind publicEntityClassKind(InteractiveKind kind) noexcept {
  switch (kind) {
    case InteractiveKind::kUnknown:
      return ARX_ENTITY_CLASS_KIND_UNKNOWN;
    case InteractiveKind::kItem:
      return ARX_ENTITY_CLASS_KIND_ITEM;
    case InteractiveKind::kNpc:
      return ARX_ENTITY_CLASS_KIND_NPC;
    case InteractiveKind::kFix:
      return ARX_ENTITY_CLASS_KIND_FIX;
    case InteractiveKind::kCamera:
      return ARX_ENTITY_CLASS_KIND_CAMERA;
    case InteractiveKind::kMarker:
      return ARX_ENTITY_CLASS_KIND_MARKER;
  }
  return ARX_ENTITY_CLASS_KIND_UNKNOWN;
}

bool modelFtlImpl(ModelPathView model, bool use_tweak, std::string& out) {
  std::string_view type;
  std::string_view item_type;
  std::string_view name;
  if (!modelClassification(model.type, type, item_type) || hasLegacyTeoExtension(model.name) ||
      !normalizeName(model.name, {".ftl"}, name))
    return false;

  std::string tweak;
  if (!model.tweak.empty() &&
      (hasLegacyTeoExtension(model.tweak) || !normalizeRelativeName(model.tweak, {".ftl"}, tweak)))
    return false;

  const std::string_view flat_base = flatModelBase(type);
  if (!flat_base.empty()) {
    if (!tweak.empty()) return false;
    std::string result(flat_base);
    result.push_back('/');
    result.append(name);
    result += ".ftl";
    out = std::move(result);
    return true;
  }

  std::string result = "game/graph/obj3d/interactive/";
  result.append(type);
  result.push_back('/');
  if (!item_type.empty()) {
    result.append(item_type);
    result.push_back('/');
  }
  result.append(name);
  result.push_back('/');
  if (!use_tweak || tweak.empty()) {
    result.append(name);
  } else {
    result += "tweaks/";
    result += tweak;
  }
  result += ".ftl";
  out = std::move(result);
  return true;
}

}  // namespace

std::span<const std::string_view> modelSelectorTypes() noexcept { return kModelTypes; }

bool modelFtl(ModelPathView model, std::string& out) { return modelFtlImpl(model, true, out); }

bool modelFromFtl(std::string_view path, ModelPathView& out) noexcept { return parseModelPath(path, false, out); }

bool entityClassFromFtl(std::string_view path, std::string& out) {
  ResourcePathCursor cursor(path);
  if (!takeExpected(cursor, "game")) return false;
  std::string_view relative = path.substr(cursor.lastComponentEnd());
  while (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) relative.remove_prefix(1);
  if (relative.empty()) return false;

  std::string normalized;
  std::string_view removed_extension;
  bool discarded_prefix = false;
  if (!normalizeEntityClassPath(relative, normalized, removed_extension, &discarded_prefix) || discarded_prefix ||
      (!removed_extension.empty() && !equalAsciiInsensitive(removed_extension, ".ftl")) ||
      classifyEntityClassPath(normalized) == InteractiveKind::kUnknown)
    return false;
  const std::size_t slash = normalized.find_last_of('/');
  const std::size_t dot = normalized.find_last_of('.');
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) return false;
  out = std::move(normalized);
  return true;
}

bool ftlFromEntityClass(std::string_view path, std::string& out) {
  if (path.empty() || path.front() == '/' || path.front() == '\\' || path.back() == '/' || path.back() == '\\')
    return false;
  std::string normalized;
  std::string_view removed_extension;
  bool discarded_prefix = false;
  if (!normalizeEntityClassPath(path, normalized, removed_extension, &discarded_prefix) || discarded_prefix ||
      !removed_extension.empty() || classifyEntityClassPath(normalized) == InteractiveKind::kUnknown)
    return false;
  std::string result = "game/";
  result += normalized;
  result += ".ftl";
  out = std::move(result);
  return true;
}

bool entityClassKind(std::string_view path, ArxEntityClassKind& out) {
  std::string normalized;
  std::string_view removed_extension;
  bool discarded_prefix = false;
  if (!normalizeEntityClassPath(path, normalized, removed_extension, &discarded_prefix) || discarded_prefix ||
      !removed_extension.empty())
    return false;
  out = publicEntityClassKind(classifyEntityClassPath(normalized));
  return true;
}

bool itemIconFromEntityClass(std::string_view path, std::string& out) {
  std::string normalized;
  std::string_view removed_extension;
  bool discarded_prefix = false;
  if (!normalizeEntityClassPath(path, normalized, removed_extension, &discarded_prefix) || discarded_prefix ||
      !removed_extension.empty())
    return false;
  if (classifyEntityClassPath(normalized) != InteractiveKind::kItem) {
    out.clear();
    return true;
  }
  normalized += "[icon]";
  out = std::move(normalized);
  return true;
}

bool entityClassFromModel(ModelPathView model, std::string& path) {
  std::string ftl;
  return modelFtl(model, ftl) && entityClassFromFtl(ftl, path);
}

bool baseEntityClassFromModel(ModelPathView model, std::string& path) {
  std::string ftl;
  return modelFtlImpl(model, false, ftl) && entityClassFromFtl(ftl, path);
}

bool modelFromEntityClass(std::string_view path, ModelPathView& out) noexcept {
  ModelPathView parsed;
  if (!parseModelPath(path, true, parsed) || classifyEntityClassPath(path) == InteractiveKind::kUnknown) return false;
  out = parsed;
  return true;
}

bool modelSelector(ModelPathView model, std::string& out) {
  std::string_view type;
  std::string_view path_type;
  std::string_view item_type;
  std::string_view name;
  if (!modelClassification(model.type, path_type, item_type) || hasLegacyTeoExtension(model.name) ||
      !normalizeName(model.name, {".ftl"}, name))
    return false;
  type = item_type.empty() ? path_type : item_type;

  std::string tweak;
  if (!model.tweak.empty() &&
      (hasLegacyTeoExtension(model.tweak) || !normalizeRelativeName(model.tweak, {".ftl"}, tweak)))
    return false;
  if (!flatModelBase(path_type).empty() && !tweak.empty()) return false;

  std::string result = "model:";
  result.append(type);
  result.push_back(':');
  result.append(name);
  if (!tweak.empty()) {
    result.push_back(':');
    result += tweak;
  }
  out = std::move(result);
  return true;
}

bool modelFromSelector(std::string_view selector, ModelPathView& out) noexcept {
  std::array<std::string_view, 4> fields;
  std::size_t count = 0;
  if (!splitSelector(selector, fields, count) || (count != 3 && count != 4) || !selectorKind(fields[0], "model"))
    return false;

  const std::string_view type = modelType(fields[1]);
  std::string_view name;
  std::string_view tweak;
  if (type.empty() || hasLegacyTeoExtension(fields[2]) || !normalizeName(fields[2], {".ftl"}, name) ||
      (count == 4 && (!flatModelBase(type).empty() || hasLegacyTeoExtension(fields[3]) ||
                      !relativeNameView(fields[3], {".ftl"}, tweak))))
    return false;
  out = {type, name, tweak};
  return true;
}

bool modelSearchLocation(std::string_view requested, ResourceSearchLocation& out) noexcept {
  std::string_view type;
  std::string_view item_type;
  if (!modelClassification(requested, type, item_type)) return false;
  const std::string_view flat_base = flatModelBase(type);
  if (!flat_base.empty()) {
    out = {flat_base, 1};
    return true;
  }
  if (item_type.empty()) {
    if (type == "npc") {
      out = {"game/graph/obj3d/interactive/npc", 3};
    } else if (type == "fix_inter") {
      out = {"game/graph/obj3d/interactive/fix_inter", 3};
    } else {
      out = {"game/graph/obj3d/interactive/system", 3};
    }
    return true;
  }
  for (std::size_t i = 0; i < kItemTypes.size(); ++i) {
    if (item_type != kItemTypes[i]) continue;
    static constexpr std::array<std::string_view, 8> kItemSearchPaths = {
        "game/graph/obj3d/interactive/items/armor",
        "game/graph/obj3d/interactive/items/jewelry",
        "game/graph/obj3d/interactive/items/magic",
        "game/graph/obj3d/interactive/items/movable",
        "game/graph/obj3d/interactive/items/provisions",
        "game/graph/obj3d/interactive/items/quest_item",
        "game/graph/obj3d/interactive/items/special",
        "game/graph/obj3d/interactive/items/weapons",
    };
    out = {kItemSearchPaths[i], 3};
    return true;
  }
  return false;
}

}  // namespace pistoris::paths
