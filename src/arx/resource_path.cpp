// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx/resource_path.h"

#include "arx_pistoris/paths.hpp"

#include "utils/name_tokens.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

}  // namespace

bool isLegacyTeoExtension(std::string_view extension) noexcept {
  constexpr std::string_view kLegacyExtension = ".teo";
  if (extension.size() != kLegacyExtension.size()) return false;
  for (std::size_t i = 0; i < extension.size(); ++i)
    if (lowerAscii(extension[i]) != kLegacyExtension[i]) return false;
  return true;
}

bool normalizeEntityClassPath(std::string_view source, std::string& out, std::string_view& removed_extension) {
  removed_extension = {};
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

  std::vector<std::string_view> components;
  for (std::size_t begin = 0; begin < normalized.size();) {
    while (begin < normalized.size() && normalized[begin] == '/') ++begin;
    if (begin == normalized.size()) break;
    std::size_t end = normalized.find('/', begin);
    if (end == std::string::npos) end = normalized.size();
    components.emplace_back(normalized.data() + begin, end - begin);
    begin = end;
  }

  auto graph = std::find(components.begin(), components.end(), "graph");
  if (graph == components.end() || graph + 1 == components.end()) return false;
  for (auto it = graph; it != components.end(); ++it)
    if (*it == "." || *it == ".." || it->empty()) return false;

  std::string result;
  for (auto it = graph; it != components.end(); ++it) {
    if (!result.empty()) result.push_back('/');
    result.append(*it);
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
  if (result.empty() || result.back() == '/') return false;

  out = std::move(result);
  removed_extension = extension;
  return true;
}

}  // namespace pistoris

namespace pistoris::paths {

bool normalizeZoneAmbiance(std::string_view source, std::string& out) {
  if (hasEmbeddedNull(source)) return false;
  const bool absolute_separator = !source.empty() && (source.front() == '/' || source.front() == '\\');
  const bool drive_path = source.size() >= 2 &&
                          ((source[0] >= 'A' && source[0] <= 'Z') || (source[0] >= 'a' && source[0] <= 'z')) &&
                          source[1] == ':';
  if (absolute_separator || drive_path) return false;

  std::string normalized;
  normalized.reserve(source.size());
  bool previous_separator = false;
  for (char value : source) {
    value = lowerAscii(value);
    if (value == '\\' || value == '/') {
      if (!normalized.empty() && !previous_separator) normalized.push_back('/');
      previous_separator = true;
    } else {
      normalized.push_back(value);
      previous_separator = false;
    }
  }
  while (!normalized.empty() && normalized.back() == '/') normalized.pop_back();
  if (normalized.empty() || normalized.front() == '/' || !validSemanticString(normalized)) return false;

  std::size_t component_begin = 0;
  while (component_begin < normalized.size()) {
    const std::size_t end = normalized.find('/', component_begin);
    const std::size_t component_end = end == std::string::npos ? normalized.size() : end;
    const std::string_view component(normalized.data() + component_begin, component_end - component_begin);
    if (component.empty() || component == "." || component == "..") return false;
    if (end == std::string::npos) break;
    component_begin = end + 1U;
  }

  const std::size_t extension = normalized.find_last_of('.');
  if (extension != std::string::npos && extension >= component_begin) normalized.erase(extension);
  if (normalized.size() == component_begin || normalized.find('.', component_begin) != std::string::npos) return false;
  out = std::move(normalized);
  return true;
}

bool zoneAmbianceFile(std::string_view ambiance, std::string& out) {
  std::string normalized;
  if (!normalizeZoneAmbiance(ambiance, normalized) || normalized == "none") {
    out.clear();
    return false;
  }
  out = "sfx/ambiance/";
  out += normalized;
  out += ".amb";
  return true;
}

}  // namespace pistoris::paths
