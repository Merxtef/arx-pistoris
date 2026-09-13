// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/texture.hpp"

#include <string>
#include <vector>

namespace pistoris {

struct LevelModules;
struct LevelValidationState;
struct NativeLevelBundle;
struct SceneData;

namespace level_native {

struct NativeLevelSource {
  const fts::Data& fts;
  const llf::Data* llf = nullptr;
  const dlf::Data* dlf = nullptr;
};

ArxReturnCode buildLevel(const NativeLevelSource& source, LevelModules& out,
                         LevelValidationState* out_validation = nullptr,
                         std::vector<std::string>* texture_source_paths = nullptr);

ArxReturnCode bakeNativeLevelBundle(const LevelModules& level, const Level::NativeBakeOptions& options,
                                    NativeLevelBundle& out);

// Caller owns complete Level validation
ArxReturnCode bakeValidatedNativeLevelBundle(const LevelModules& level, const Level::NativeBakeOptions& options,
                                             NativeLevelBundle& out);

// Caller owns validation of every Scene module
ArxReturnCode bakeValidatedNativeDlf(const SceneData& scene, const Level::DlfBakeOptions& options, dlf::Data& out);

}  // namespace level_native
}  // namespace pistoris
