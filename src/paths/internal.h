
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "paths/entity_class.h"
#include "utils/identifier.h"
#include "utils/resource_path.h"

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

namespace pistoris::paths::detail {

inline constexpr std::array<std::string_view, 8> kItemTypes = {
    "armor",
    "jewelry",
    "magic",
    "movable",
    "provisions",
    "quest_item",
    "special",
    "weapons",
};

inline constexpr std::array<std::string_view, 14> kModelTypes = {
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
    "ui-runes",
    "ui-menus",
    "editor",
};

struct FlatModelLayout {
  std::string_view type;
  std::string_view base_path;
};

inline constexpr std::array<FlatModelLayout, 3> kFlatModelLayouts = {{
    {"ui-runes", "game/graph/interface/book/runes"},
    {"ui-menus", "game/graph/interface/menus"},
    {"editor", "game/editor/obj3d"},
}};

inline constexpr std::array<std::string_view, 2> kAnimationTypes = {"npc", "fix_inter"};

inline char lowerAscii(char value) noexcept {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

inline bool equalAsciiInsensitive(std::string_view lhs, std::string_view rhs) noexcept {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i)
    if (lowerAscii(lhs[i]) != lowerAscii(rhs[i])) return false;
  return true;
}

inline bool hasLegacyTeoExtension(std::string_view value) noexcept {
  constexpr std::string_view kExtension = ".teo";
  return value.size() >= kExtension.size() &&
         equalAsciiInsensitive(value.substr(value.size() - kExtension.size()), kExtension);
}

inline bool normalizeName(std::string_view name, std::initializer_list<std::string_view> extensions,
                          std::string_view& out) noexcept {
  for (std::string_view extension : extensions) {
    if (name.size() < extension.size() ||
        !equalAsciiInsensitive(name.substr(name.size() - extension.size()), extension)) {
      continue;
    }
    name.remove_suffix(extension.size());
    break;
  }
  if (!::pistoris::isPortableResourcePathComponent(name)) return false;
  out = name;
  return true;
}

inline std::string_view modelType(std::string_view type) noexcept {
  for (std::string_view candidate : kModelTypes)
    if (equalAsciiInsensitive(type, candidate)) return candidate;
  return {};
}

inline std::string_view itemType(std::string_view type) noexcept {
  for (std::string_view candidate : kItemTypes)
    if (equalAsciiInsensitive(type, candidate)) return candidate;
  return {};
}

inline bool modelClassification(std::string_view requested, std::string_view& type,
                                std::string_view& item_type) noexcept {
  const std::string_view normalized = modelType(requested);
  if (normalized.empty()) return false;
  item_type = itemType(normalized);
  type = item_type.empty() ? normalized : std::string_view{"items"};
  return true;
}

inline bool interactiveModelClassification(std::string_view requested, std::string_view& type,
                                           std::string_view& item_type) noexcept {
  if (!modelClassification(requested, type, item_type)) return false;
  return !item_type.empty() || type == "npc" || type == "fix_inter" || type == "system";
}

inline std::string_view flatModelBase(std::string_view type) noexcept {
  for (const FlatModelLayout& layout : kFlatModelLayouts)
    if (type == layout.type) return layout.base_path;
  return {};
}

inline std::string_view animationType(std::string_view interactive_type) noexcept {
  std::string_view type;
  std::string_view item_type;
  if (!interactiveModelClassification(interactive_type, type, item_type)) return {};
  if (type == "npc") return "npc";
  return "fix_inter";
}

inline std::string_view canonicalAnimationType(std::string_view type) noexcept {
  for (std::string_view candidate : kAnimationTypes)
    if (equalAsciiInsensitive(type, candidate)) return candidate;
  return {};
}

inline bool splitResourcePath(std::string_view path, std::vector<std::string_view>& out) {
  if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
  for (std::size_t begin = 0; begin < path.size();) {
    while (begin < path.size() && (path[begin] == '/' || path[begin] == '\\')) ++begin;
    if (begin == path.size()) break;
    std::size_t end = path.find_first_of("/\\", begin);
    if (end == std::string_view::npos) end = path.size();
    const std::string_view component = path.substr(begin, end - begin);
    if (!::pistoris::isPortableResourcePathComponent(component)) return false;
    out.push_back(component);
    begin = end;
  }
  return !out.empty();
}

inline bool stripOptionalExtension(std::string_view filename, std::initializer_list<std::string_view> extensions,
                                   std::string_view& stem) noexcept;

inline std::string_view trimRecognizedExtension(std::string_view value,
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

inline bool normalizeRelativeName(std::string_view name, std::initializer_list<std::string_view> extensions,
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

inline bool relativeNameView(std::string_view name, std::initializer_list<std::string_view> extensions,
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
    if (!::pistoris::isPortableResourcePathComponent(last)) return false;
    begin = end;
  }

  const std::string_view stem = trimRecognizedExtension(last, extensions);
  if (stem.empty()) return false;
  out = name.substr(0, name.size() - (last.size() - stem.size()));
  return true;
}

template <std::size_t Capacity>
inline bool splitSelector(std::string_view selector, std::array<std::string_view, Capacity>& out,
                          std::size_t& count) noexcept {
  count = 0;
  for (std::size_t begin = 0;;) {
    if (count == Capacity) return false;
    const std::size_t separator = selector.find(':', begin);
    out[count++] = selector.substr(begin, separator == std::string_view::npos ? separator : separator - begin);
    if (out[count - 1].empty()) return false;
    if (separator == std::string_view::npos) return true;
    begin = separator + 1;
  }
}

inline bool selectorKind(std::string_view field, std::string_view expected) noexcept {
  return equalAsciiInsensitive(field, expected);
}

template <std::size_t Capacity>
inline bool resourcePathComponents(std::string_view path, std::array<std::string_view, Capacity>& out,
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
    if (parsed == Capacity || !::pistoris::isPortableResourcePathComponent(component)) return false;
    out[parsed++] = component;
    begin = end;
  }
  if (parsed == 0) return false;
  count = parsed;
  return true;
}

template <std::size_t Capacity>
inline bool fixedComponents(const std::array<std::string_view, Capacity>& components, std::size_t count,
                            std::initializer_list<std::string_view> expected) noexcept {
  if (count < expected.size()) return false;
  std::size_t index = 0;
  for (std::string_view value : expected) {
    if (!equalAsciiInsensitive(components[index++], value)) return false;
  }
  return true;
}

inline bool stripOptionalExtension(std::string_view filename, std::initializer_list<std::string_view> extensions,
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

inline bool parseLevelComponent(std::string_view component, std::uint32_t& level) noexcept {
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

inline bool levelFromSceneFile(std::string_view path, std::string_view extension, std::uint32_t& level) noexcept {
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
    if (!::pistoris::isPortableResourcePathComponent(out)) {
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

inline bool takeExpected(ResourcePathCursor& cursor, std::string_view expected) noexcept {
  std::string_view actual;
  return cursor.take(actual) && equalAsciiInsensitive(actual, expected);
}

inline bool takeExpectedBase(ResourcePathCursor& cursor, std::string_view base_path) noexcept {
  ResourcePathCursor expected(base_path);
  std::string_view expected_component;
  std::string_view actual_component;
  while (expected.take(expected_component)) {
    if (!cursor.take(actual_component) || !equalAsciiInsensitive(actual_component, expected_component)) return false;
  }
  return expected.done();
}

inline bool parseFlatModelPath(std::string_view path, bool entity_class, ModelPathView& out) noexcept {
  for (const FlatModelLayout& layout : kFlatModelLayouts) {
    ResourcePathCursor cursor(path);
    std::string_view base_path = layout.base_path;
    if (entity_class) base_path.remove_prefix(std::string_view{"game/"}.size());
    if (!takeExpectedBase(cursor, base_path)) continue;
    std::string_view filename;
    std::string_view name;
    if (!cursor.take(filename) || !cursor.done() || hasLegacyTeoExtension(filename) ||
        !stripOptionalExtension(filename, {".ftl"}, name) || (entity_class && name.size() != filename.size()))
      continue;
    out = {layout.type, name, {}};
    return true;
  }
  return false;
}

inline bool parseModelPath(std::string_view path, bool entity_class, ModelPathView& out) noexcept {
  if (parseFlatModelPath(path, entity_class, out)) return true;
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
    std::string_view item_type;
    if (!interactiveModelClassification(path_type, type, item_type) || !item_type.empty()) return false;
  }

  std::string_view name;
  std::string_view next;
  if (!cursor.take(name) || !cursor.take(next)) return false;
  if (entity_class && name.find('.') != std::string_view::npos) return false;

  if (cursor.done()) {
    std::string_view stem;
    if (hasLegacyTeoExtension(next) || !stripOptionalExtension(next, {".ftl"}, stem) ||
        (entity_class && stem.size() != next.size()) || !equalAsciiInsensitive(stem, name))
      return false;
    out = {type, name, {}};
    return true;
  }
  if (!equalAsciiInsensitive(next, "tweaks")) return false;

  std::string_view first_tweak;
  std::string_view last_tweak;
  if (!cursor.take(first_tweak)) return false;
  const std::size_t tweak_begin = cursor.lastComponentBegin();
  last_tweak = first_tweak;
  while (!cursor.done()) {
    if (!cursor.take(last_tweak)) return false;
  }
  std::string_view stem;
  if (hasLegacyTeoExtension(last_tweak) || !stripOptionalExtension(last_tweak, {".ftl"}, stem) ||
      (entity_class && stem.size() != last_tweak.size()))
    return false;
  const std::size_t tweak_end = cursor.lastComponentEnd() - (last_tweak.size() - stem.size());
  out = {type, name, path.substr(tweak_begin, tweak_end - tweak_begin)};
  return true;
}

}  // namespace pistoris::paths::detail
