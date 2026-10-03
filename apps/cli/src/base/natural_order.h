// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "base/ascii.h"

#include <cstddef>
#include <string_view>

namespace cli {

constexpr bool naturalStringLess(std::string_view left, std::string_view right) noexcept {
  std::size_t left_index = 0;
  std::size_t right_index = 0;
  while (left_index < left.size() && right_index < right.size()) {
    if (!isAsciiDigit(left[left_index]) || !isAsciiDigit(right[right_index])) {
      const auto left_value = static_cast<unsigned char>(left[left_index]);
      const auto right_value = static_cast<unsigned char>(right[right_index]);
      if (left_value != right_value) return left_value < right_value;
      ++left_index;
      ++right_index;
      continue;
    }

    std::size_t left_end = left_index;
    while (left_end < left.size() && isAsciiDigit(left[left_end])) ++left_end;
    std::size_t right_end = right_index;
    while (right_end < right.size() && isAsciiDigit(right[right_end])) ++right_end;

    std::size_t left_significant = left_index;
    while (left_significant < left_end && left[left_significant] == '0') ++left_significant;
    std::size_t right_significant = right_index;
    while (right_significant < right_end && right[right_significant] == '0') ++right_significant;

    const std::size_t left_digits = left_end - left_significant;
    const std::size_t right_digits = right_end - right_significant;
    if (left_digits != right_digits) return left_digits < right_digits;
    for (std::size_t offset = 0; offset < left_digits; ++offset) {
      const char left_digit = left[left_significant + offset];
      const char right_digit = right[right_significant + offset];
      if (left_digit != right_digit) return left_digit < right_digit;
    }

    const std::size_t left_run = left_end - left_index;
    const std::size_t right_run = right_end - right_index;
    if (left_run != right_run) return left_run < right_run;
    left_index = left_end;
    right_index = right_end;
  }
  return left_index == left.size() && right_index != right.size();
}

}  // namespace cli
