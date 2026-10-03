// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/navigation/traversal.h"
#include "utils/spatial/arx_level_grid_index.h"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace pistoris::navigation::collision {

inline constexpr float kCollisionBroadphaseEpsilon = 1.0f;

struct Cylinder {
  ArxVector3 origin{};
  float radius = 0.0f;
  float height = 0.0f;
};

struct TraversalFace {
  std::array<ArxVector3, 3> vertices{};
  ArxVector3 center{};
  ArxVector3 normal{};
  ArxVector3 min{};
  ArxVector3 max{};
  float area = 0.0f;
  FaceType flags = 0;
};

struct FootprintHit {
  ArxVector3 point{};
  ArxVector3 normal{};
  float min_y = 0.0f;
  float max_y = 0.0f;
};

struct PlacementResult {
  CylinderPlacementStatus status = CylinderPlacementStatus::kInvalid;
  Cylinder cylinder{};
};

class StaticCollisionIndex {
 public:
  explicit StaticCollisionIndex(const GeometryData& geometry);

  [[nodiscard]] const TraversalFace& face(std::uint32_t index) const;
  void findCandidates(std::vector<std::uint32_t>& out, const Cylinder& cylinder) const;

 private:
  std::vector<TraversalFace> faces_;
  spatial::ArxLevelGridIndex grid_index_;
};

std::optional<FootprintHit> footprintHit(const TraversalFace& face, const Cylinder& cylinder);
float broadphaseRadius(const Cylinder& cylinder);

PlacementResult placeCylinderAt(const StaticCollisionIndex& index, const ArxVector3& position, float radius,
                                float height, float probe_depth, float tolerance,
                                std::vector<std::uint32_t>& candidate_scratch);

}  // namespace pistoris::navigation::collision
