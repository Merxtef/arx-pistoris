// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/level.hpp"

#include "../writer.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::glb_level {

struct LevelNormalCluster {
  std::vector<ArxVector3> directions;
  ArxVector3 representative = {};
};

struct LevelNormalAnalysis {
  std::vector<std::vector<LevelNormalCluster>> clusters;
  std::vector<std::array<std::uint32_t, 3>> corner_clusters;
};

ArxReturnCode analyzeLevelNormals(std::span<const ArxLevelVertex> vertices, std::span<const ArxLevelFace> faces,
                                  float normal_weld_degrees, LevelNormalAnalysis& out);
ArxVector3 debugVertexNormal(std::span<const LevelNormalCluster> clusters);

}  // namespace pistoris::glb_level
