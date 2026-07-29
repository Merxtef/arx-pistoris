// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

namespace pistoris {

struct LevelModules;
struct LevelValidationState;

ArxReturnCode validateLevelModules(const LevelModules& modules, ArxAabb* out_bounds = nullptr,
                                   ArxAabb* out_referenced_bounds = nullptr);
ArxReturnCode validateLevelModules(const LevelModules& modules, LevelValidationState& out_state);

}  // namespace pistoris
