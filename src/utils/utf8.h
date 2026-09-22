// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <string_view>

namespace pistoris::utf8 {

[[nodiscard]] inline bool valid(std::string_view text) noexcept {
  const auto continuation = [](unsigned char value) { return value >= 0x80U && value <= 0xbfU; };
  for (std::size_t index = 0; index < text.size();) {
    const auto first = static_cast<unsigned char>(text[index++]);
    if (first <= 0x7fU) continue;
    if (first >= 0xc2U && first <= 0xdfU) {
      if (index >= text.size()) return false;
      const auto second = static_cast<unsigned char>(text[index]);
      ++index;
      if (!continuation(second)) return false;
      continue;
    }
    if (first >= 0xe0U && first <= 0xefU) {
      if (index + 1U >= text.size()) return false;
      const auto second = static_cast<unsigned char>(text[index++]);
      const auto third = static_cast<unsigned char>(text[index++]);
      if (!continuation(third) || (first == 0xe0U && (second < 0xa0U || second > 0xbfU)) ||
          (first == 0xedU && (second < 0x80U || second > 0x9fU)) ||
          (first != 0xe0U && first != 0xedU && !continuation(second)))
        return false;
      continue;
    }
    if (first >= 0xf0U && first <= 0xf4U) {
      if (index + 2U >= text.size()) return false;
      const auto second = static_cast<unsigned char>(text[index++]);
      const auto third = static_cast<unsigned char>(text[index++]);
      const auto fourth = static_cast<unsigned char>(text[index++]);
      if (!continuation(third) || !continuation(fourth) || (first == 0xf0U && (second < 0x90U || second > 0xbfU)) ||
          (first == 0xf4U && (second < 0x80U || second > 0x8fU)) ||
          (first != 0xf0U && first != 0xf4U && !continuation(second)))
        return false;
      continue;
    }
    return false;
  }
  return true;
}

[[nodiscard]] inline std::size_t prefixSize(std::string_view text, std::size_t limit) noexcept {
  if (text.size() <= limit) return text.size();
  while (limit != 0 && (static_cast<unsigned char>(text[limit]) & 0xc0U) == 0x80U) --limit;
  return limit;
}

}  // namespace pistoris::utf8
