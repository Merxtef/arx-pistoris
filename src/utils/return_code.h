// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "utils/cursor.h"

namespace pistoris {

inline ArxReturnCode resolveRc(ArxReturnCode section_rc, const ReadCursor& cursor) noexcept {
  if (!cursor) return ARX_UNEXPECTED_EOF;
  return section_rc;
}

inline ArxReturnCode resolveRc(ArxReturnCode section_rc, const WriteCursor& cursor) noexcept {
  if (!cursor) return ARX_BAD_ALLOC;
  return section_rc;
}

inline ArxReturnCode resolveRc(const ReadCursor& cursor) noexcept { return resolveRc(ARX_OK, cursor); }
inline ArxReturnCode resolveRc(const WriteCursor& cursor) noexcept { return resolveRc(ARX_OK, cursor); }
inline ArxReturnCode resolveRc(ArxReturnCode rc) noexcept { return rc; }

}  // namespace pistoris

#define ARX_RETURN_IF_ERR(...)                                                                \
  do {                                                                                        \
    if (ArxReturnCode _rc_ = ::pistoris::resolveRc(__VA_ARGS__); _rc_ != ARX_OK) return _rc_; \
  } while (0)
