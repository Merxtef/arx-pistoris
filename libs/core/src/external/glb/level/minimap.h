// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"

#include "external/glb/level/coordinates.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming)
struct cgltf_node;

namespace pistoris {
struct MinimapData;
}

namespace pistoris::glb {
class Asset;
class Builder;
}  // namespace pistoris::glb

namespace pistoris::glb_level {

enum class MinimapImportError : std::uint8_t {
  kNone,
  kBadData,
  kOutOfMemory,
};

ArxReturnCode exportMinimap(const MinimapData& minimap, const ArxAabb& referenced_bounds, glb::Builder& builder);
MinimapImportError importMinimap(const glb::Asset& asset, const cgltf_node& node, const math::Mat4& world,
                                 const ImportUnits& units, MinimapData& out);

}  // namespace pistoris::glb_level
