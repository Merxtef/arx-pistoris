// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace cli {

constexpr bool isAsciiDigit(char value) noexcept { return value >= '0' && value <= '9'; }

constexpr char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

inline std::string asciiLower(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (char c : value) result.push_back(lowerAscii(c));
  return result;
}

constexpr bool equalAsciiInsensitive(std::string_view left, std::string_view right) noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index)
    if (lowerAscii(left[index]) != lowerAscii(right[index])) return false;
  return true;
}

constexpr bool startsWithAsciiInsensitive(std::string_view value, std::string_view prefix) noexcept {
  return value.size() >= prefix.size() && equalAsciiInsensitive(value.substr(0, prefix.size()), prefix);
}

constexpr bool endsWithAsciiInsensitive(std::string_view value, std::string_view suffix) noexcept {
  return value.size() >= suffix.size() && equalAsciiInsensitive(value.substr(value.size() - suffix.size()), suffix);
}

}  // namespace cli
