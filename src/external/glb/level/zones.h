// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"

#include "external/glb/container.h"
#include "external/glb/writer.h"
#include "modules/scene.h"
#include "utils/math/mat4.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris {
struct LevelModules;
}

namespace pistoris::glb_level {

struct ImportUnits;
class Palette;

struct PendingZone {
  std::optional<std::uint32_t> ordinal;
  std::size_t node_index = 0;
  Zone zone;
  float top_y = 0.0f;
  float bottom_y = 0.0f;
};

bool isReservedZoneName(std::string_view name);

ArxReturnCode exportZones(const LevelModules& level, const ArxAabb& referenced_bounds,
                          const Level::GlbExportOptions& options, glb::Builder& builder, Palette& palette);

ArxReturnCode importZones(const glb::Asset& asset, const std::vector<math::Mat4>& world,
                          std::span<const std::size_t> roots, const ImportUnits& units,
                          std::vector<PendingZone>& zones);

std::vector<Zone> finalizeImportedZones(std::vector<PendingZone> pending, const ArxAabb& referenced_bounds);

}  // namespace pistoris::glb_level
