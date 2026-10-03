// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "../writer.h"
#include "modules/geometry.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pistoris::glb_level {

struct LevelVertexNormalAnalysis {
  ArxVector3 representative = {};
  std::size_t cluster_count = 0;
};

struct LevelNormalAnalysis {
  std::vector<LevelVertexNormalAnalysis> vertices;
};

ArxReturnCode analyzeLevelNormals(std::span<const Vertex> vertices, std::span<const Face> faces,
                                  float normal_weld_degrees, LevelNormalAnalysis& out);

}  // namespace pistoris::glb_level
