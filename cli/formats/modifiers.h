// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"

#include <optional>

namespace cli {

inline constexpr float kMinGlbArxUnitsPerUnit = 1.0f;
inline constexpr float kMaxGlbArxUnitsPerUnit = 1000.0f;

struct GlbModifierOptions {
  std::optional<float> arx_units_per_unit;
  std::optional<pistoris::ArxVector3> arx_offset;
};

struct FormatModifierOptions {
  GlbModifierOptions glb;
};

}  // namespace cli
