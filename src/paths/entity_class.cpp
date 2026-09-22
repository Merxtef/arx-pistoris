// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "paths/entity_class.h"

#include "utils/name_tokens.h"
#include "utils/resource_path.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

struct InteractiveClassification {
  std::string_view marker;
  InteractiveKind kind;
};

constexpr std::array kInteractiveClassifications = {
    InteractiveClassification{"items", InteractiveKind::kItem},
    InteractiveClassification{"npc", InteractiveKind::kNpc},
    InteractiveClassification{"fix", InteractiveKind::kFix},
    InteractiveClassification{"camera", InteractiveKind::kCamera},
    InteractiveClassification{"marker", InteractiveKind::kMarker},
};

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool containsAsciiInsensitive(std::string_view value, std::string_view needle) noexcept {
  if (needle.size() > value.size()) return false;
  for (std::size_t begin = 0; begin <= value.size() - needle.size(); ++begin) {
    bool equal = true;
    for (std::size_t i = 0; i < needle.size(); ++i) {
      if (lowerAscii(value[begin + i]) != needle[i]) {
        equal = false;
        break;
      }
    }
    if (equal) return true;
  }
  return false;
}

}  // namespace

bool isLegacyTeoExtension(std::string_view extension) noexcept {
  constexpr std::string_view kLegacyExtension = ".teo";
  if (extension.size() != kLegacyExtension.size()) return false;
  for (std::size_t i = 0; i < extension.size(); ++i)
    if (lowerAscii(extension[i]) != kLegacyExtension[i]) return false;
  return true;
}

bool normalizeEntityClassPathImpl(std::string_view source, std::string& out, std::string_view& removed_extension,
                                  bool* discarded_prefix, bool require_utf8_resource_path) {
  removed_extension = {};
  if (discarded_prefix != nullptr) *discarded_prefix = false;
  if (hasEmbeddedNull(source)) return false;
  std::string normalized;
  normalized.reserve(source.size());
  bool previous_separator = false;
  for (char value : source) {
    value = lowerAscii(value);
    if (value == '\\' || value == '/') {
      if (!previous_separator) normalized.push_back('/');
      previous_separator = true;
    } else {
      normalized.push_back(value);
      previous_separator = false;
    }
  }

  const std::size_t graph = normalized.find("graph");
  if (graph != std::string::npos) {
    if (discarded_prefix != nullptr) *discarded_prefix = graph != 0;
    normalized.erase(0, graph);
  }

  std::vector<std::string_view> components;
  for (std::size_t begin = 0; begin < normalized.size();) {
    while (begin < normalized.size() && normalized[begin] == '/') ++begin;
    if (begin == normalized.size()) break;
    std::size_t end = normalized.find('/', begin);
    if (end == std::string::npos) end = normalized.size();
    components.emplace_back(normalized.data() + begin, end - begin);
    begin = end;
  }

  if (components.empty()) return false;
  for (std::string_view component : components)
    if (component == "." || component == ".." || component.empty()) return false;

  std::string result;
  for (std::string_view component : components) {
    if (!result.empty()) result.push_back('/');
    result.append(component);
  }

  std::size_t slash = result.find_last_of('/');
  std::size_t dot = result.find_last_of('.');
  std::string_view extension;
  if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
    std::size_t source_end = source.size();
    while (source_end != 0 && (source[source_end - 1] == '/' || source[source_end - 1] == '\\')) --source_end;
    const std::size_t source_dot = source.rfind('.', source_end - 1);
    if (source_dot != std::string_view::npos) extension = source.substr(source_dot, source_end - source_dot);
    result.erase(dot);
  }
  if (result.empty() || result.back() == '/' || (require_utf8_resource_path && !isResourcePath(result))) return false;

  out = std::move(result);
  removed_extension = extension;
  return true;
}

bool normalizeNativeEntityClassPath(std::string_view source, std::string& out, std::string_view& removed_extension,
                                    bool* discarded_prefix) {
  return normalizeEntityClassPathImpl(source, out, removed_extension, discarded_prefix, false);
}

bool normalizeEntityClassPath(std::string_view source, std::string& out, std::string_view& removed_extension,
                              bool* discarded_prefix) {
  return normalizeEntityClassPathImpl(source, out, removed_extension, discarded_prefix, true);
}

InteractiveKind classifyEntityClassPath(std::string_view normalized_path) noexcept {
  for (const InteractiveClassification& classification : kInteractiveClassifications)
    if (containsAsciiInsensitive(normalized_path, classification.marker)) return classification.kind;
  return InteractiveKind::kUnknown;
}

}  // namespace pistoris
