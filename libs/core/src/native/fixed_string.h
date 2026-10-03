// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "utils/log.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <string_view>

namespace pistoris {

template <std::size_t N>
inline bool isNullTerminated(const char (&value)[N]) noexcept {
  return std::memchr(value, '\0', N) != nullptr;
}

template <std::size_t N>
inline bool fitsNativeString(std::string_view value) noexcept {
  return value.size() < N && value.find('\0') == std::string_view::npos;
}

template <std::size_t N>
inline bool copyFixedString(std::string_view value, char (&out)[N], bool allow_empty = true) noexcept {
  if ((!allow_empty && value.empty()) || !fitsNativeString<N>(value)) return false;
  if (!value.empty()) std::memcpy(out, value.data(), value.size());
  std::memset(out + value.size(), 0, N - value.size());
  return true;
}

template <std::size_t N>
inline std::string_view fixedStringView(const char (&value)[N]) noexcept {
  const void* terminator = std::memchr(value, '\0', N);
  const std::size_t size = terminator ? static_cast<const char*>(terminator) - value : N;
  return {value, size};
}

template <std::size_t N>
inline void canonicalizeFixedString(char (&arr)[N], std::string_view field, int idx = -1) {
  static_assert(N != 0);
  void* terminator = std::memchr(arr, '\0', N);
  if (!terminator) {
    if (idx < 0)
      log(ARX_LOG_WARN, "{} not null-terminated, clamping", field);
    else
      log(ARX_LOG_WARN, "{}[{}] not null-terminated, clamping", field, idx);
    arr[N - 1] = '\0';
    terminator = arr + N - 1;
  }
  const auto tail = static_cast<char*>(terminator) + 1;
  std::memset(tail, 0, static_cast<std::size_t>(arr + N - tail));
}

}  // namespace pistoris
