// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace pistoris {

enum class ObjSource : std::uint8_t {
  kObj,
  kMaterialLibrary,
};

struct ObjLocation {
  ObjSource source = ObjSource::kObj;
  std::size_t source_index = kNoInputIndex;
  std::size_t line = 0;
  std::string source_path;
};

template <class T>
using ObjResult = Result<T, ObjLocation>;

}  // namespace pistoris
