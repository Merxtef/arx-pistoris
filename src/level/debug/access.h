// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "level/data.h"

namespace pistoris::level_debug {

struct LevelDebugAccess {
  static LevelModules& modules(Level& level) noexcept { return static_cast<LevelModules&>(*level.data_); }
  static const LevelModules& modules(const Level& level) noexcept {
    return static_cast<const LevelModules&>(*level.data_);
  }
  static LevelValidationState& validation(Level& level) noexcept { return level.data_->validation; }
  static const LevelValidationState& validation(const Level& level) noexcept { return level.data_->validation; }
};

}  // namespace pistoris::level_debug
