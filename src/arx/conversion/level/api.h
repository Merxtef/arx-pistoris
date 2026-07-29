// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/pistoris_types.h"

namespace pistoris {

struct LevelModules;
struct LevelValidationState;
struct NativeLevelBundle;
struct SceneData;

namespace arx_level_conversion {

struct NativeLevelSource {
  const fts::Data& fts;
  const llf::Data* llf = nullptr;
  const dlf::Data* dlf = nullptr;
};

ArxReturnCode buildLevel(const NativeLevelSource& source, LevelModules& out,
                         LevelValidationState* out_validation = nullptr);

ArxReturnCode bakeNativeLevelBundle(const LevelModules& level, const Level::NativeBakeOptions& options,
                                    NativeLevelBundle& out);

// Caller owns complete Level validation
ArxReturnCode bakeValidatedNativeLevelBundle(const LevelModules& level, const Level::NativeBakeOptions& options,
                                             NativeLevelBundle& out);

// Caller owns validation of every Scene module
ArxReturnCode bakeValidatedNativeDlf(const SceneData& scene, const Level::NativeDlfBakeOptions& options,
                                     dlf::Data& out);

}  // namespace arx_level_conversion
}  // namespace pistoris
