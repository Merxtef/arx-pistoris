// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level/types.h"

#include "cgltf/cgltf.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {
struct LevelModules;
struct ModelModules;
}  // namespace pistoris

namespace pistoris::glb {
class Builder;
}

namespace pistoris::glb_level {

struct ImportUnits;

std::string entityNodeName(std::string_view label, std::size_t ordinal, std::int32_t ident = -1);
std::string entityClassHelperName(std::string_view class_path, std::string_view label);
ArxReturnCode exportEntities(const LevelModules& level, const ArxAabb& referenced_bounds,
                             std::span<const ModelModules* const> model_previews, ArxLevelModelPreviewReport& report,
                             glb::Builder& builder);
ArxReturnCode importEntities(const cgltf_data& data, const std::vector<math::Mat4>& world,
                             std::span<const std::size_t> nodes, const ImportUnits& units, LevelModules& level,
                             std::uint64_t& normalized_legacy_teo);

}  // namespace pistoris::glb_level
