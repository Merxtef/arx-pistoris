// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "base/ascii.h"

#include <algorithm>
#include <string>
#include <string_view>

namespace cli {

inline std::string normalizeResourceSeparators(std::string_view path) {
  std::string result(path);
  for (char& value : result)
    if (value == '\\') value = '/';
  return result;
}

inline std::string resourcePathKey(std::string_view path) {
  std::string result = normalizeResourceSeparators(path);
  for (char& value : result) value = lowerAscii(value);
  return result;
}

constexpr bool resourcePathLess(std::string_view left, std::string_view right) noexcept {
  const std::size_t count = std::min(left.size(), right.size());
  for (std::size_t index = 0; index < count; ++index) {
    const char left_value = lowerAscii(left[index] == '\\' ? '/' : left[index]);
    const char right_value = lowerAscii(right[index] == '\\' ? '/' : right[index]);
    if (left_value != right_value) return left_value < right_value;
  }
  return left.size() < right.size();
}

std::string resourceParentPath(std::string_view path);
std::string_view resourceFilename(std::string_view path) noexcept;
std::string resourceStem(std::string_view path);

}  // namespace cli
