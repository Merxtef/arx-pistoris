// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "topology.h"

#include "external/glb/accessor.h"
#include "level/data.h"
#include "modules/geometry.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace pistoris::glb_level {
bool sameUv(const glb::Vec2& a, const glb::Vec2& b) {
  return std::abs(a.x - b.x) <= kLevelGlbEpsilon && std::abs(a.y - b.y) <= kLevelGlbEpsilon;
}

std::size_t compactLevelVertices(LevelModules& level) {
  const std::size_t original_size = level.geometry.vertices.size();
  std::vector<std::uint32_t> remap(original_size, std::numeric_limits<std::uint32_t>::max());
  std::vector<Vertex> compact;
  compact.reserve(original_size);
  for (Face& face : level.geometry.faces) {
    for (Corner& corner : face.corners) {
      std::uint32_t& mapped = remap[corner.vertex];
      if (mapped == std::numeric_limits<std::uint32_t>::max()) {
        mapped = static_cast<std::uint32_t>(compact.size());
        compact.push_back(level.geometry.vertices[corner.vertex]);
      }
      corner.vertex = mapped;
    }
  }
  level.geometry.vertices = std::move(compact);
  return original_size - level.geometry.vertices.size();
}

}  // namespace pistoris::glb_level
