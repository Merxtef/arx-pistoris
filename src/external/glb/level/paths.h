// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"

#include "cgltf/cgltf.h"
#include "external/glb/writer.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <span>
#include <vector>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb_level {

struct ImportUnits;

void exportPaths(const LevelModules& level, const ArxAabb& referenced_bounds, glb::Builder& builder);

ArxReturnCode importPaths(const cgltf_data& data, const std::vector<math::Mat4>& world,
                          std::span<const std::size_t> roots, const ImportUnits& units, LevelModules& level);

}  // namespace pistoris::glb_level
