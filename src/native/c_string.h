// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include "utils/cursor.h"

#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pistoris::native_io {

inline ArxReturnCode readCString(std::string& out, ReadCursor& cursor) noexcept {
  out.clear();
  try {
    for (;;) {
      char value = '\0';
      cursor.read(value);
      if (!cursor) return ARX_UNEXPECTED_EOF;
      if (value == '\0') return ARX_OK;
      out.push_back(value);
    }
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (const std::length_error&) {
    return ARX_BAD_ALLOC;
  } catch (...) {
    return ARX_INTERNAL_ERROR;
  }
}

inline WriteCursor& writeCString(std::string_view value, WriteCursor& cursor) noexcept {
  cursor.writeN(value.data(), value.size());
  return cursor.pad(1);
}

inline bool containsNull(std::string_view value) noexcept { return value.find('\0') != std::string_view::npos; }

}  // namespace pistoris::native_io
