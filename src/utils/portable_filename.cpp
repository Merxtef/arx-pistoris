// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/portable_filename.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>

namespace pistoris {
namespace {

char lowerAscii(char value) noexcept {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool equalsAsciiInsensitive(std::string_view value, std::string_view lowercase) noexcept {
  if (value.size() != lowercase.size()) return false;
  return std::equal(value.begin(), value.end(), lowercase.begin(), [](char left, char right) noexcept {
    return lowerAscii(left) == right;
  });
}

bool portableCharacter(char value) noexcept {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
         value == '.' || value == '-' || value == '_' || value == '[' || value == ']';
}

std::size_t extensionOffset(std::string_view name) noexcept {
  const std::size_t dot = name.find_last_of('.');
  return dot != std::string_view::npos && dot != 0 && dot + 1U < name.size() ? dot : name.size();
}

std::string suffixedName(std::string_view requested, std::size_t suffix) {
  const std::size_t extension = extensionOffset(requested);
  std::string_view stem = requested.substr(0, extension);
  std::string_view ending = requested.substr(extension);
  const std::string number = std::to_string(suffix);
  const bool needs_separator = !stem.ends_with('_');
  const std::size_t suffix_size = number.size() + static_cast<std::size_t>(needs_separator);

  std::size_t stem_size = stem.size();
  if (ending.size() + suffix_size >= kPortableNameMax) {
    ending = {};
    stem_size = std::min(stem_size, kPortableNameMax - suffix_size);
  } else {
    stem_size = std::min(stem_size, kPortableNameMax - ending.size() - suffix_size);
  }

  std::string candidate(stem.substr(0, stem_size));
  if (needs_separator) candidate.push_back('_');
  candidate += number;
  candidate += ending;
  return candidate;
}

std::string sanitizePortableNameImpl(std::string_view requested) {
  std::string result;
  result.reserve(std::min(requested.size(), kPortableNameMax));
  bool previous_underscore = false;
  for (char value : requested) {
    char output = portableCharacter(value) ? value : '_';
    if (output == '_') {
      if (previous_underscore) continue;
      previous_underscore = true;
    } else {
      previous_underscore = false;
    }
    result.push_back(output);
    if (result.size() == kPortableNameMax) break;
  }
  while (!result.empty() && (result.back() == '.' || result.back() == ' ')) result.pop_back();
  if (result.empty()) result = "_";
  return result;
}

}  // namespace

bool isPortableReservedName(std::string_view name) noexcept {
  name = name.substr(0, name.find('.'));
  if (equalsAsciiInsensitive(name, "con") || equalsAsciiInsensitive(name, "prn") ||
      equalsAsciiInsensitive(name, "aux") || equalsAsciiInsensitive(name, "nul") ||
      equalsAsciiInsensitive(name, "clock$") || equalsAsciiInsensitive(name, "conin$") ||
      equalsAsciiInsensitive(name, "conout$")) {
    return true;
  }
  return name.size() == 4 &&
         (equalsAsciiInsensitive(name.substr(0, 3), "com") || equalsAsciiInsensitive(name.substr(0, 3), "lpt")) &&
         name[3] >= '1' && name[3] <= '9';
}

bool isPortableName(std::string_view name) noexcept {
  if (name.empty() || name == "." || name == ".." || name.size() > kPortableNameMax || name.back() == '.' ||
      name.back() == ' ' || isPortableReservedName(name)) {
    return false;
  }
  bool previous_underscore = false;
  for (char value : name) {
    if (!portableCharacter(value)) return false;
    if (value == '_' && previous_underscore) return false;
    previous_underscore = value == '_';
  }
  return true;
}

std::string makeUniquePortableName(std::string_view requested, const std::unordered_set<std::string>& unavailable) {
  std::string candidate = isPortableName(requested) ? std::string(requested) : sanitizePortableNameImpl(requested);
  if (!isPortableReservedName(candidate) && !unavailable.contains(candidate)) return candidate;

  for (std::size_t suffix = 1;; ++suffix) {
    std::string suffixed = suffixedName(candidate, suffix);
    if (!isPortableReservedName(suffixed) && !unavailable.contains(suffixed)) return suffixed;
  }
}

}  // namespace pistoris
