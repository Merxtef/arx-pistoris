// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include "cgltf/cgltf.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <span>
#include <vector>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb {
class Builder;
}

namespace pistoris::glb_level {

struct ImportUnits;

void exportPlayerSpawn(const LevelModules& level, glb::Builder& builder);
ArxReturnCode importPlayerSpawn(const cgltf_data& data, const std::vector<math::Mat4>& world,
                                std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level);

}  // namespace pistoris::glb_level
