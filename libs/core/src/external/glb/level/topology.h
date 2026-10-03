// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "../writer.h"

#include <cstddef>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb_level {

inline constexpr float kLevelGlbEpsilon = 1.0e-4f;

bool sameUv(const glb::Vec2& a, const glb::Vec2& b);

std::size_t compactLevelVertices(LevelModules& level);

}  // namespace pistoris::glb_level
