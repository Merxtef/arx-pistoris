// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation/collision/internal.h"
#include "modules/navigation/traversal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

namespace pistoris::navigation::collision {
namespace {

std::optional<float> verticalPlacementOffset(const StaticCollisionIndex& index, const Cylinder& cylinder,
                                             std::vector<std::uint32_t>& candidates) {
  constexpr float kContactEpsilon = 0.01f;
  float cylinder_top = cylinder.origin.y + cylinder.height;
  float cylinder_bottom = cylinder.origin.y;
  float target_y = cylinder.origin.y;
  bool blocked = false;

  index.findCandidates(candidates, cylinder);
  for (std::uint32_t face_index : candidates) {
    std::optional<FootprintHit> hit = footprintHit(index.face(face_index), cylinder);
    if (!hit) continue;
    if (hit->min_y >= cylinder_bottom - kContactEpsilon) continue;
    if (hit->max_y <= cylinder_top + kContactEpsilon) continue;
    blocked = true;
    target_y = std::min(target_y, hit->min_y);
  }

  if (!blocked) return std::nullopt;
  return target_y - cylinder.origin.y;
}

}  // namespace

PlacementResult placeCylinderAt(const StaticCollisionIndex& index, const ArxVector3& position, float radius,
                                float height, float probe_depth, float tolerance,
                                std::vector<std::uint32_t>& candidate_scratch) {
  Cylinder cylinder{position, radius, height};
  cylinder.origin.y += probe_depth;
  bool had_contact = false;
  int guard = 32;
  while (guard-- > 0) {
    std::optional<float> offset = verticalPlacementOffset(index, cylinder, candidate_scratch);
    if (!offset.has_value()) {
      if (!had_contact) return {CylinderPlacementStatus::kNoSupport, cylinder};
      return std::abs(cylinder.origin.y - position.y) <= tolerance
                 ? PlacementResult{CylinderPlacementStatus::kPlaced, cylinder}
                 : PlacementResult{CylinderPlacementStatus::kTooFar, cylinder};
    }

    had_contact = true;
    if (*offset >= -0.01f) {
      return std::abs(cylinder.origin.y - position.y) <= tolerance
                 ? PlacementResult{CylinderPlacementStatus::kPlaced, cylinder}
                 : PlacementResult{CylinderPlacementStatus::kTooFar, cylinder};
    }

    cylinder.origin.y += *offset;
    if (cylinder.origin.y < position.y - tolerance) return {CylinderPlacementStatus::kTooFar, cylinder};
  }

  return {CylinderPlacementStatus::kUnresolved, cylinder};
}

}  // namespace pistoris::navigation::collision
