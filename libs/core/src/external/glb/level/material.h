// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/level.hpp"

#include <cstddef>
#include <limits>

namespace pistoris::glb_level {

struct LevelRenderKey {
  std::size_t texture_group = std::numeric_limits<std::size_t>::max();
  FaceType flags = 0;
  float transval = 0.0f;

  bool operator<(const LevelRenderKey& other) const;
};

}  // namespace pistoris::glb_level
