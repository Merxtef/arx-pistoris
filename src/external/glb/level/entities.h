// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "cgltf/cgltf.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <cstdint>
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

void exportEntities(const LevelModules& level, const ArxAabb& referenced_bounds, glb::Builder& builder);
ArxReturnCode importEntities(const cgltf_data& data, const std::vector<math::Mat4>& world,
                             std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level,
                             std::uint64_t& normalized_legacy_teo);

}  // namespace pistoris::glb_level
