// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "base/ascii.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace cli {

enum class PrefixCase : std::uint8_t {
  kSensitive,
  kAsciiInsensitive,
};

enum class UniquePrefixStatus : std::uint8_t {
  kNone,
  kUnique,
  kAmbiguous,
};

template <typename T>
struct UniquePrefixResult {
  UniquePrefixStatus status = UniquePrefixStatus::kNone;
  const T* value = nullptr;
};

template <typename T, typename Projection>
UniquePrefixResult<T> resolveUniquePrefix(std::string_view requested, std::span<const T> values, Projection projection,
                                          PrefixCase letter_case = PrefixCase::kSensitive) {
  if (requested.empty()) return {};

  const auto equal = [letter_case](std::string_view left, std::string_view right) {
    return letter_case == PrefixCase::kSensitive ? left == right : equalAsciiInsensitive(left, right);
  };
  const auto starts_with = [letter_case](std::string_view value, std::string_view prefix) {
    return letter_case == PrefixCase::kSensitive ? value.starts_with(prefix)
                                                 : startsWithAsciiInsensitive(value, prefix);
  };

  const T* exact = nullptr;
  const T* prefix = nullptr;
  bool duplicate_exact = false;
  bool ambiguous_prefix = false;
  for (const T& value : values) {
    const std::string_view name = std::invoke(projection, value);
    if (equal(name, requested)) {
      duplicate_exact = exact != nullptr;
      if (!exact) exact = &value;
      continue;
    }
    if (!starts_with(name, requested)) continue;
    ambiguous_prefix = prefix != nullptr;
    if (!prefix) prefix = &value;
  }

  if (exact)
    return {.status = duplicate_exact ? UniquePrefixStatus::kAmbiguous : UniquePrefixStatus::kUnique,
            .value = duplicate_exact ? nullptr : exact};
  if (ambiguous_prefix) return {.status = UniquePrefixStatus::kAmbiguous};
  if (prefix) return {.status = UniquePrefixStatus::kUnique, .value = prefix};
  return {};
}

}  // namespace cli
