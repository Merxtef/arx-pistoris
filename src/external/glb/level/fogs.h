// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "../writer.h"
#include "cgltf/cgltf.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb_level {

struct ImportUnits;

void exportFogs(const LevelModules& level, const ArxAabb& referenced_bounds, const Level::GlbExportOptions& options,
                glb::Builder& builder);
ArxReturnCode importFogs(const cgltf_data& data, const std::vector<math::Mat4>& world,
                         std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level,
                         std::vector<std::string>& warnings);

}  // namespace pistoris::glb_level
