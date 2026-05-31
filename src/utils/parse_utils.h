// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include "utils/cursor.h"
#include "utils/log.h"

#include <cstddef>
#include <cstring>
#include <format>
#include <string_view>

namespace pistoris {

// WARN and clamp arr[N-1] if no null found; dirty trailing bytes after the null are silently accepted
template <std::size_t N>
inline void clampStr(char (&arr)[N], std::string_view field, int idx = -1) {
  if (std::memchr(arr, '\0', N)) return;
  if (idx < 0)
    log(ARX_LOG_WARN, std::format("{} not null-terminated, clamping", field));
  else
    log(ARX_LOG_WARN, std::format("{}[{}] not null-terminated, clamping", field, idx));
  arr[N - 1] = '\0';
}

inline ArxReturnCode resolveRc(ArxReturnCode section_rc, const ReadCursor& c) noexcept {
  if (!c) return ARX_UNEXPECTED_EOF;
  return section_rc;
}

inline ArxReturnCode resolveRc(ArxReturnCode section_rc, const WriteCursor& c) noexcept {
  if (!c) return ARX_BAD_ALLOC;
  return section_rc;
}

inline ArxReturnCode resolveRc(const ReadCursor& c) noexcept { return resolveRc(ARX_OK, c); }
inline ArxReturnCode resolveRc(const WriteCursor& c) noexcept { return resolveRc(ARX_OK, c); }
inline ArxReturnCode resolveRc(ArxReturnCode rc) noexcept { return rc; }

}  // namespace pistoris

#define ARX_RETURN_IF_ERR(...)                                                                \
  do {                                                                                        \
    if (ArxReturnCode _rc_ = ::pistoris::resolveRc(__VA_ARGS__); _rc_ != ARX_OK) return _rc_; \
  } while (0)
