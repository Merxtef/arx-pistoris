// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "material.h"

#include <tuple>

namespace pistoris::glb_level {

bool LevelRenderKey::operator<(const LevelRenderKey& other) const {
  return std::tie(texture_group, flags, transval) < std::tie(other.texture_group, other.flags, other.transval);
}

}  // namespace pistoris::glb_level
