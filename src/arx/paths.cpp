// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/paths.hpp"

#include "arx_pistoris/paths/types.h"

#include "utils/name_tokens.h"
#include "utils/unique_name.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace pistoris::paths {
namespace {

struct TextureAlias {
  std::string_view game;
  std::string_view library;
};

constexpr std::array<TextureAlias, 3> kTextureAliases = {{
    {"graph/obj3d/textures/l4_dwarf_[stone]__wall01", "graph/obj3d/textures/l4_dwarf_[stone]_wall01"},
    {"graph/obj3d/textures/l4_dwarf_[stone]__wall24", "graph/obj3d/textures/l4_dwarf_[stone]_wall24"},
    {"graph/obj3d/textures/npc_human__base_hero_head", "graph/obj3d/textures/npc_human_base_hero_head_1"},
}};

constexpr std::array<std::string_view, 8> kItemTypes = {
    "armor",
    "jewelry",
    "magic",
    "movable",
    "provisions",
    "quest_item",
    "special",
    "weapons",
};

constexpr std::array<std::string_view, 11> kModelTypes = {
    "npc",
    "fix_inter",
    "system",
    "armor",
    "jewelry",
    "magic",
    "movable",
    "provisions",
    "quest_item",
    "special",
    "weapons",
};

constexpr std::array<std::string_view, 2> kAnimationTypes = {"npc", "fix_inter"};

char lowerAscii(char value) noexcept {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool equalAsciiInsensitive(std::string_view lhs, std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i)
    if (lowerAscii(lhs[i]) != lowerAscii(rhs[i])) return false;
  return true;
}

bool hasLegacyTeoExtension(std::string_view value) noexcept {
  constexpr std::string_view kExtension = ".teo";
  return value.size() >= kExtension.size() &&
         equalAsciiInsensitive(value.substr(value.size() - kExtension.size()), kExtension);
}

bool validResourceComponent(std::string_view component) noexcept {
  if (component.empty() || component == "." || component == "..") return false;
  for (char value : component) {
    switch (value) {
      case '/':
      case '\\':
      case ':':
      case '\0':
        return false;
      default:
        break;
    }
  }
  return true;
}

bool normalizeName(std::string_view name, std::initializer_list<std::string_view> extensions,
                   std::string_view& out) noexcept {
  for (std::string_view extension : extensions) {
    if (name.size() < extension.size() ||
        !equalAsciiInsensitive(name.substr(name.size() - extension.size()), extension)) {
      continue;
    }
    name.remove_suffix(extension.size());
    break;
  }
  if (!validResourceComponent(name)) return false;
  out = name;
  return true;
}

std::string_view modelType(std::string_view type) noexcept {
  for (std::string_view candidate : kModelTypes)
    if (equalAsciiInsensitive(type, candidate)) return candidate;
  return {};
}

std::string_view itemType(std::string_view type) noexcept {
  for (std::string_view candidate : kItemTypes)
    if (equalAsciiInsensitive(type, candidate)) return candidate;
  return {};
}

bool modelClassification(std::string_view requested, std::string_view& type, std::string_view& item_type) noexcept {
  const std::string_view normalized = modelType(requested);
  if (normalized.empty()) return false;
  item_type = itemType(normalized);
  type = item_type.empty() ? normalized : std::string_view{"items"};
  return true;
}

std::string_view animationType(std::string_view interactive_type) noexcept {
  const std::string_view type = modelType(interactive_type);
  if (type == "npc") return "npc";
  return type.empty() ? std::string_view{} : std::string_view{"fix_inter"};
}

std::string_view canonicalAnimationType(std::string_view type) noexcept {
  for (std::string_view candidate : kAnimationTypes)
    if (equalAsciiInsensitive(type, candidate)) return candidate;
  return {};
}

std::size_t extensionOffset(std::string_view path) {
  const std::size_t separator = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  if (dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)) return path.size();
  return dot;
}

std::string comparisonKey(std::string_view path) {
  std::string out;
  out.reserve(path.size());
  bool previous_separator = false;
  for (char value : path) {
    if (value == '/' || value == '\\') {
      if (!previous_separator) out.push_back('/');
      previous_separator = true;
    } else {
      out.push_back(lowerAscii(value));
      previous_separator = false;
    }
  }
  return out;
}

std::string remapTexture(std::string_view path, bool from_game) {
  const std::size_t extension = extensionOffset(path);
  const std::string key = comparisonKey(path.substr(0, extension));
  for (const TextureAlias& alias : kTextureAliases) {
    const std::string_view source = from_game ? alias.game : alias.library;
    if (key != source) continue;
    const std::string_view target = from_game ? alias.library : alias.game;
    std::string out(target);
    out.append(path.substr(extension));
    return out;
  }
  return std::string(path);
}

bool splitResourcePath(std::string_view path, std::vector<std::string_view>& out) {
  if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
  for (std::size_t begin = 0; begin < path.size();) {
    while (begin < path.size() && (path[begin] == '/' || path[begin] == '\\')) ++begin;
    if (begin == path.size()) break;
    std::size_t end = path.find_first_of("/\\", begin);
    if (end == std::string_view::npos) end = path.size();
    const std::string_view component = path.substr(begin, end - begin);
    if (!validResourceComponent(component)) return false;
    out.push_back(component);
    begin = end;
  }
  return !out.empty();
}

bool stripOptionalExtension(std::string_view filename, std::initializer_list<std::string_view> extensions,
                            std::string_view& stem) noexcept;

std::string_view trimRecognizedExtension(std::string_view value,
                                         std::initializer_list<std::string_view> extensions) noexcept {
  for (std::string_view extension : extensions) {
    if (value.size() >= extension.size() &&
        equalAsciiInsensitive(value.substr(value.size() - extension.size()), extension)) {
      value.remove_suffix(extension.size());
      break;
    }
  }
  return value;
}

bool normalizeRelativeName(std::string_view name, std::initializer_list<std::string_view> extensions,
                           std::string& out) {
  if (name.empty() || name.back() == '/' || name.back() == '\\') return false;
  std::vector<std::string_view> components;
  if (!splitResourcePath(name, components)) return false;
  const std::string_view stem = trimRecognizedExtension(components.back(), extensions);
  if (stem.empty()) return false;

  std::string result;
  result.reserve(name.size());
  for (std::size_t i = 0; i + 1 < components.size(); ++i) {
    if (!result.empty()) result.push_back('/');
    result.append(components[i]);
  }
  if (!result.empty()) result.push_back('/');
  result.append(stem);
  out = std::move(result);
  return true;
}

bool relativeNameView(std::string_view name, std::initializer_list<std::string_view> extensions,
                      std::string_view& out) noexcept {
  if (name.empty() || name.front() == '/' || name.front() == '\\' || name.back() == '/' || name.back() == '\\')
    return false;

  std::string_view last;
  for (std::size_t begin = 0; begin < name.size();) {
    while (begin < name.size() && (name[begin] == '/' || name[begin] == '\\')) ++begin;
    if (begin == name.size()) return false;
    std::size_t end = name.find_first_of("/\\", begin);
    if (end == std::string_view::npos) end = name.size();
    last = name.substr(begin, end - begin);
    if (!validResourceComponent(last)) return false;
    begin = end;
  }

  const std::string_view stem = trimRecognizedExtension(last, extensions);
  if (stem.empty()) return false;
  out = name.substr(0, name.size() - (last.size() - stem.size()));
  return true;
}

template <std::size_t Capacity>
bool splitShorthand(std::string_view shorthand, std::array<std::string_view, Capacity>& out,
                    std::size_t& count) noexcept {
  count = 0;
  for (std::size_t begin = 0;;) {
    if (count == Capacity) return false;
    const std::size_t separator = shorthand.find(':', begin);
    out[count++] = shorthand.substr(begin, separator == std::string_view::npos ? separator : separator - begin);
    if (out[count - 1].empty()) return false;
    if (separator == std::string_view::npos) return true;
    begin = separator + 1;
  }
}

bool shorthandKind(std::string_view field, std::string_view expected) noexcept {
  return equalAsciiInsensitive(field, expected);
}

template <std::size_t Capacity>
bool resourcePathComponents(std::string_view path, std::array<std::string_view, Capacity>& out,
                            std::size_t& count) noexcept {
  if (path.empty() || path.front() == '/' || path.front() == '\\' || path.back() == '/' || path.back() == '\\')
    return false;

  std::size_t parsed = 0;
  for (std::size_t begin = 0; begin < path.size();) {
    while (begin < path.size() && (path[begin] == '/' || path[begin] == '\\')) ++begin;
    if (begin == path.size()) break;
    std::size_t end = path.find_first_of("/\\", begin);
    if (end == std::string_view::npos) end = path.size();
    const std::string_view component = path.substr(begin, end - begin);
    if (parsed == Capacity || !validResourceComponent(component)) return false;
    out[parsed++] = component;
    begin = end;
  }
  if (parsed == 0) return false;
  count = parsed;
  return true;
}

template <std::size_t Capacity>
bool fixedComponents(const std::array<std::string_view, Capacity>& components, std::size_t count,
                     std::initializer_list<std::string_view> expected) noexcept {
  if (count < expected.size()) return false;
  std::size_t index = 0;
  for (std::string_view value : expected) {
    if (!equalAsciiInsensitive(components[index++], value)) return false;
  }
  return true;
}

bool stripOptionalExtension(std::string_view filename, std::initializer_list<std::string_view> extensions,
                            std::string_view& stem) noexcept {
  const std::size_t dot = filename.find_last_of('.');
  if (dot == std::string_view::npos) {
    stem = filename;
    return !stem.empty();
  }
  const std::string_view extension = filename.substr(dot);
  for (std::string_view candidate : extensions) {
    if (!equalAsciiInsensitive(extension, candidate)) continue;
    stem = filename.substr(0, dot);
    return !stem.empty();
  }
  return false;
}

bool parseLevelComponent(std::string_view component, std::uint32_t& level) noexcept {
  constexpr std::string_view kPrefix = "level";
  if (component.size() <= kPrefix.size() || !equalAsciiInsensitive(component.substr(0, kPrefix.size()), kPrefix))
    return false;
  const std::string_view digits = component.substr(kPrefix.size());
  if (digits.size() > 1 && digits.front() == '0') return false;
  std::uint32_t parsed = 0;
  const auto result = std::from_chars(digits.data(), digits.data() + digits.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size()) return false;
  level = parsed;
  return true;
}

bool levelFromSceneFile(std::string_view path, std::string_view extension, std::uint32_t& level) noexcept {
  std::array<std::string_view, 4> components;
  std::size_t count = 0;
  if (!resourcePathComponents(path, components, count) || count != components.size() ||
      !fixedComponents(components, count, {"graph", "levels"}))
    return false;

  std::uint32_t parsed = 0;
  if (!parseLevelComponent(components[2], parsed)) return false;
  std::string_view stem;
  std::uint32_t file_level = 0;
  if (!stripOptionalExtension(components[3], {extension}, stem) || !parseLevelComponent(stem, file_level) ||
      parsed != file_level)
    return false;
  level = parsed;
  return true;
}

void appendLowerAscii(std::string& out, std::string_view value) {
  for (char character : value) out.push_back(lowerAscii(character));
}

class ResourcePathCursor {
 public:
  explicit ResourcePathCursor(std::string_view path) : path_(path) {
    valid_ = !path.empty() && path.front() != '/' && path.front() != '\\' && path.back() != '/' && path.back() != '\\';
  }

  bool take(std::string_view& out) noexcept {
    if (!valid_ || position_ == path_.size()) return false;
    const std::size_t begin = position_;
    std::size_t end = path_.find_first_of("/\\", begin);
    if (end == std::string_view::npos) end = path_.size();
    out = path_.substr(begin, end - begin);
    if (!validResourceComponent(out)) {
      valid_ = false;
      return false;
    }
    last_component_begin_ = begin;
    last_component_end_ = end;
    position_ = end;
    while (position_ < path_.size() && (path_[position_] == '/' || path_[position_] == '\\')) ++position_;
    return true;
  }

  [[nodiscard]] bool done() const noexcept { return valid_ && position_ == path_.size(); }
  [[nodiscard]] std::size_t lastComponentBegin() const noexcept { return last_component_begin_; }
  [[nodiscard]] std::size_t lastComponentEnd() const noexcept { return last_component_end_; }

 private:
  std::string_view path_;
  std::size_t position_ = 0;
  std::size_t last_component_begin_ = 0;
  std::size_t last_component_end_ = 0;
  bool valid_ = false;
};

bool takeExpected(ResourcePathCursor& cursor, std::string_view expected) noexcept {
  std::string_view actual;
  return cursor.take(actual) && equalAsciiInsensitive(actual, expected);
}

bool parseModelPath(std::string_view path, bool entity_class, ModelPathView& out) noexcept {
  ResourcePathCursor cursor(path);
  if (entity_class) {
    if (!takeExpected(cursor, "graph") || !takeExpected(cursor, "obj3d") || !takeExpected(cursor, "interactive"))
      return false;
  } else if (!takeExpected(cursor, "game") || !takeExpected(cursor, "graph") || !takeExpected(cursor, "obj3d") ||
             !takeExpected(cursor, "interactive")) {
    return false;
  }

  std::string_view path_type;
  if (!cursor.take(path_type)) return false;
  std::string_view type;
  if (equalAsciiInsensitive(path_type, "items")) {
    std::string_view path_item_type;
    if (!cursor.take(path_item_type)) return false;
    type = itemType(path_item_type);
    if (type.empty()) return false;
  } else {
    type = modelType(path_type);
    if (type.empty() || !itemType(type).empty()) return false;
  }

  std::string_view name;
  std::string_view next;
  if (!cursor.take(name) || !cursor.take(next)) return false;
  if (entity_class && (name.find('.') != std::string_view::npos || !validSemanticString(name))) return false;

  if (cursor.done()) {
    std::string_view stem;
    if (hasLegacyTeoExtension(next) || !stripOptionalExtension(next, {".ftl"}, stem) ||
        !equalAsciiInsensitive(stem, name))
      return false;
    out = {type, name, {}};
    return true;
  }
  if (entity_class || !equalAsciiInsensitive(next, "tweaks")) return false;

  std::string_view first_tweak;
  std::string_view last_tweak;
  if (!cursor.take(first_tweak)) return false;
  const std::size_t tweak_begin = cursor.lastComponentBegin();
  last_tweak = first_tweak;
  while (!cursor.done()) {
    if (!cursor.take(last_tweak)) return false;
  }
  std::string_view stem;
  if (hasLegacyTeoExtension(last_tweak) || !stripOptionalExtension(last_tweak, {".ftl"}, stem)) return false;
  const std::size_t tweak_end = cursor.lastComponentEnd() - (last_tweak.size() - stem.size());
  out = {type, name, path.substr(tweak_begin, tweak_end - tweak_begin)};
  return true;
}

}  // namespace

bool isPortableFilename(std::string_view filename) noexcept { return isPortableName(filename); }

std::string sanitizePortableFilename(std::string_view filename) { return makeUniquePortableName(filename); }

std::string textureFromGame(std::string_view path) { return remapTexture(path, true); }

std::string textureToGame(std::string_view path) { return remapTexture(path, false); }

std::string levelDlf(std::uint32_t level) {
  const std::string name = "level" + std::to_string(level);
  return "graph/levels/" + name + "/" + name + ".dlf";
}

std::string levelLlf(std::uint32_t level) {
  const std::string name = "level" + std::to_string(level);
  return "graph/levels/" + name + "/" + name + ".llf";
}

std::string levelFts(std::uint32_t level) { return "game/graph/levels/level" + std::to_string(level) + "/fast.fts"; }

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
  if (!validResourceComponent(name)) return false;
  out = "graph/levels/";
  out.append(name);
  return true;
}

ArxResourceKind resourceShorthandKind(std::string_view shorthand) noexcept {
  const std::size_t delimiter = shorthand.find(':');
  if (delimiter == std::string_view::npos) return ARX_RESOURCE_KIND_NONE;
  const std::string_view kind = shorthand.substr(0, delimiter);
  if (shorthandKind(kind, "level")) return ARX_RESOURCE_KIND_LEVEL;
  if (shorthandKind(kind, "model")) return ARX_RESOURCE_KIND_MODEL;
  if (shorthandKind(kind, "anim")) return ARX_RESOURCE_KIND_ANIMATION;
  if (shorthandKind(kind, "cinematic")) return ARX_RESOURCE_KIND_CINEMATIC;
  if (shorthandKind(kind, "ambiance")) return ARX_RESOURCE_KIND_AMBIANCE;
  return ARX_RESOURCE_KIND_NONE;
}

std::string levelShorthand(std::uint32_t level) { return "level:" + std::to_string(level); }

bool levelFromShorthand(std::string_view shorthand, std::uint32_t& level) noexcept {
  std::array<std::string_view, 2> fields;
  std::size_t count = 0;
  if (!splitShorthand(shorthand, fields, count) || count != fields.size() || !shorthandKind(fields[0], "level"))
    return false;

  std::uint32_t parsed = 0;
  const auto result = std::from_chars(fields[1].data(), fields[1].data() + fields[1].size(), parsed);
  if (result.ec != std::errc{} || result.ptr != fields[1].data() + fields[1].size()) return false;
  level = parsed;
  return true;
}

std::span<const std::string_view> modelTypes() noexcept { return kModelTypes; }

bool modelFtl(ModelPathView model, std::string& out) {
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

  std::string result = "game/graph/obj3d/interactive/";
  result.append(type);
  result.push_back('/');
  if (!item_type.empty()) {
    result.append(item_type);
    result.push_back('/');
  }
  result.append(name);
  result.push_back('/');
  if (tweak.empty()) {
    result.append(name);
  } else {
    result += "tweaks/";
    result += tweak;
  }
  result += ".ftl";
  out = std::move(result);
  return true;
}

bool modelFromFtl(std::string_view path, ModelPathView& out) noexcept { return parseModelPath(path, false, out); }

bool entityClassFromModel(ModelPathView model, std::string& path) {
  std::string_view type;
  std::string_view item_type;
  std::string_view name;
  if (!model.tweak.empty() || !modelClassification(model.type, type, item_type) || hasLegacyTeoExtension(model.name) ||
      !normalizeName(model.name, {".ftl"}, name) || name.find('.') != std::string_view::npos ||
      !validSemanticString(name))
    return false;

  std::string result = "graph/obj3d/interactive/";
  result.append(type);
  result.push_back('/');
  if (!item_type.empty()) {
    result.append(item_type);
    result.push_back('/');
  }
  appendLowerAscii(result, name);
  result.push_back('/');
  appendLowerAscii(result, name);
  path = std::move(result);
  return true;
}

bool modelFromEntityClass(std::string_view path, ModelPathView& out) noexcept {
  return parseModelPath(path, true, out);
}

bool modelShorthand(ModelPathView model, std::string& out) {
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

bool modelFromShorthand(std::string_view shorthand, ModelPathView& out) noexcept {
  std::array<std::string_view, 4> fields;
  std::size_t count = 0;
  if (!splitShorthand(shorthand, fields, count) || (count != 3 && count != 4) || !shorthandKind(fields[0], "model"))
    return false;

  const std::string_view type = modelType(fields[1]);
  std::string_view name;
  std::string_view tweak;
  if (type.empty() || hasLegacyTeoExtension(fields[2]) || !normalizeName(fields[2], {".ftl"}, name) ||
      (count == 4 && (hasLegacyTeoExtension(fields[3]) || !relativeNameView(fields[3], {".ftl"}, tweak))))
    return false;
  out = {type, name, tweak};
  return true;
}

std::span<const std::string_view> animationTypes() noexcept { return kAnimationTypes; }

bool animationDirectory(std::string_view interactive_type, std::string& out) {
  const std::string_view normalized_type = animationType(interactive_type);
  if (normalized_type.empty()) return false;
  out = "graph/obj3d/anims/";
  out.append(normalized_type);
  return true;
}

bool animationTea(AnimationPathView animation, std::string& out) {
  const std::string_view type = canonicalAnimationType(animation.type);
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

  const std::string_view normalized_type = canonicalAnimationType(components[3]);
  std::string_view stem;
  if (normalized_type.empty() || !stripOptionalExtension(components[4], {".tea"}, stem)) return false;

  out = {normalized_type, stem};
  return true;
}

bool animationShorthand(AnimationPathView animation, std::string& out) {
  const std::string_view type = canonicalAnimationType(animation.type);
  std::string_view name;
  if (type.empty() || !normalizeName(animation.name, {".tea"}, name)) return false;
  std::string result = "anim:";
  result.append(type);
  result.push_back(':');
  result.append(name);
  out = std::move(result);
  return true;
}

bool animationFromShorthand(std::string_view shorthand, AnimationPathView& out) noexcept {
  std::array<std::string_view, 3> fields;
  std::size_t count = 0;
  if (!splitShorthand(shorthand, fields, count) || count != fields.size() || !shorthandKind(fields[0], "anim"))
    return false;
  const std::string_view type = canonicalAnimationType(fields[1]);
  std::string_view name;
  if (type.empty() || !normalizeName(fields[2], {".tea"}, name)) return false;
  out = {type, name};
  return true;
}

bool cinematicFile(CinematicPathView cinematic, std::string& out) {
  std::string_view name;
  if (!normalizeName(cinematic.name, {".cin"}, name)) return false;
  std::string result = "graph/interface/illustrations/";
  result.append(name);
  result += ".cin";
  out = std::move(result);
  return true;
}

bool cinematicFromFile(std::string_view path, CinematicPathView& out) noexcept {
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

bool cinematicShorthand(CinematicPathView cinematic, std::string& out) {
  std::string_view name;
  if (!normalizeName(cinematic.name, {".cin"}, name)) return false;
  std::string result = "cinematic:";
  result.append(name);
  out = std::move(result);
  return true;
}

bool cinematicFromShorthand(std::string_view shorthand, CinematicPathView& out) noexcept {
  std::array<std::string_view, 2> fields;
  std::size_t count = 0;
  std::string_view name;
  if (!splitShorthand(shorthand, fields, count) || count != fields.size() || !shorthandKind(fields[0], "cinematic") ||
      !normalizeName(fields[1], {".cin"}, name))
    return false;
  out = {name};
  return true;
}

bool ambianceFile(AmbiancePathView ambiance, std::string& out) {
  std::string name;
  if (!normalizeRelativeName(ambiance.name, {".amb"}, name) || equalAsciiInsensitive(name, "none")) return false;
  std::string result = "sfx/ambiance/";
  result += name;
  result += ".amb";
  out = std::move(result);
  return true;
}

bool ambianceFromFile(std::string_view path, AmbiancePathView& out) noexcept {
  ResourcePathCursor cursor(path);
  if (!takeExpected(cursor, "sfx") || !takeExpected(cursor, "ambiance")) return false;
  std::string_view first;
  std::string_view last;
  if (!cursor.take(first)) return false;
  const std::size_t name_begin = cursor.lastComponentBegin();
  last = first;
  while (!cursor.done()) {
    if (!cursor.take(last)) return false;
  }
  std::string_view stem;
  if (!stripOptionalExtension(last, {".amb"}, stem)) return false;
  const std::size_t name_end = cursor.lastComponentEnd() - (last.size() - stem.size());
  const std::string_view name = path.substr(name_begin, name_end - name_begin);
  if (equalAsciiInsensitive(name, "none")) return false;
  out = {name};
  return true;
}

bool ambianceShorthand(AmbiancePathView ambiance, std::string& out) {
  std::string name;
  if (!normalizeRelativeName(ambiance.name, {".amb"}, name) || equalAsciiInsensitive(name, "none")) return false;
  std::string result = "ambiance:";
  result += name;
  out = std::move(result);
  return true;
}

bool ambianceFromShorthand(std::string_view shorthand, AmbiancePathView& out) noexcept {
  std::array<std::string_view, 2> fields;
  std::size_t count = 0;
  std::string_view name;
  if (!splitShorthand(shorthand, fields, count) || count != fields.size() || !shorthandKind(fields[0], "ambiance") ||
      !relativeNameView(fields[1], {".amb"}, name) || equalAsciiInsensitive(name, "none"))
    return false;
  out = {name};
  return true;
}

bool modelSearchLocation(std::string_view requested, ResourceSearchLocation& out) noexcept {
  std::string_view type;
  std::string_view item_type;
  if (!modelClassification(requested, type, item_type)) return false;
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

bool animationSearchLocation(std::string_view requested, ResourceSearchLocation& out) noexcept {
  const std::string_view type = canonicalAnimationType(requested);
  if (type == "npc") {
    out = {"graph/obj3d/anims/npc", 1};
    return true;
  }
  if (type == "fix_inter") {
    out = {"graph/obj3d/anims/fix_inter", 1};
    return true;
  }
  return false;
}

ResourceSearchLocation levelSearchLocation() noexcept { return {"graph/levels", 2}; }

ResourceSearchLocation cinematicSearchLocation() noexcept { return {"graph/interface/illustrations", 1}; }

ResourceSearchLocation ambianceSearchLocation() noexcept { return {"sfx/ambiance", 8}; }

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
