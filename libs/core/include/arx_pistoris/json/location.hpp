// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstddef>
#include <string>

namespace pistoris {

struct JsonLocation {
  std::size_t byte_offset = kNoElementIndex;
  std::string pointer;
};

template <class T>
using JsonResult = Result<T, JsonLocation>;

}  // namespace pistoris
