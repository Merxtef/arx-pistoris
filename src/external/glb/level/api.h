// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/pistoris_types.h"

#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {

struct LevelModules;
struct LevelValidationState;

ArxReturnCode exportLevelToGlb(const LevelModules& level, const ArxAabb& referenced_bounds,
                               const Level::GlbExportOptions& options, std::vector<std::uint8_t>& out);
ArxReturnCode importLevelFromGlb(std::span<const std::uint8_t> glb, LevelModules& out,
                                 const Level::GlbImportOptions& options, LevelValidationState* out_validation = nullptr,
                                 Level::GlbImportInfo* info = nullptr);

ArxReturnCode buildFtsCellsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out,
                                    const Level::GlbExportOptions& options = {});
ArxReturnCode buildFtsRoomsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out);

}  // namespace pistoris
