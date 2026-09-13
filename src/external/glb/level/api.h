// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/texture.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct LevelModules;
struct LevelValidationState;
struct ModelModules;

ArxReturnCode exportLevelToGlb(const LevelModules& level, const ArxAabb& referenced_bounds,
                               const Level::GlbExportOptions& options,
                               std::span<const ModelModules* const> model_previews, ArxLevelModelPreviewReport* report,
                               std::vector<std::uint8_t>& out);

inline ArxReturnCode exportLevelToGlb(const LevelModules& level, const ArxAabb& referenced_bounds,
                                      const Level::GlbExportOptions& options, std::vector<std::uint8_t>& out) {
  return exportLevelToGlb(level, referenced_bounds, options, {}, nullptr, out);
}
ArxReturnCode importLevelFromGlb(std::span<const std::uint8_t> glb, LevelModules& out,
                                 const Level::GlbImportOptions& options, LevelValidationState* out_validation = nullptr,
                                 ArxLevelGlbImportInfo* info = nullptr,
                                 std::vector<std::string>* texture_source_paths = nullptr);

ArxReturnCode buildFtsCellsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out,
                                    const Level::GlbExportOptions& options = {});
ArxReturnCode buildFtsRoomsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out);

}  // namespace pistoris
