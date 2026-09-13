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

// Missing terminator: warn and clamp final byte; ignore bytes after first NUL
template <std::size_t N>
inline void clampStr(char (&arr)[N], std::string_view field, int idx = -1) {
  if (isNullTerminated(arr)) return;
  if (idx < 0)
    log(ARX_LOG_WARN, "{} not null-terminated, clamping", field);
  else
    log(ARX_LOG_WARN, "{}[{}] not null-terminated, clamping", field, idx);
  arr[N - 1] = '\0';
}

}  // namespace pistoris
