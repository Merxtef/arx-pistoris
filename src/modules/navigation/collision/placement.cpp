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

std::vector<FootprintHit> footprintHits(const StaticCollisionIndex& index, const Cylinder& cylinder) {
  std::vector<FootprintHit> hits;
  for (std::uint32_t face_index : index.candidates(cylinder)) {
    std::optional<FootprintHit> hit = footprintHit(index.face(face_index), cylinder);
    if (hit.has_value()) hits.push_back(*hit);
  }
  return hits;
}

std::optional<float> verticalPlacementOffset(const StaticCollisionIndex& index, const Cylinder& cylinder) {
  constexpr float kContactEpsilon = 0.01f;
  float cylinder_top = cylinder.origin.y + cylinder.height;
  float cylinder_bottom = cylinder.origin.y;
  float target_y = cylinder.origin.y;
  bool blocked = false;

  for (const FootprintHit& hit : footprintHits(index, cylinder)) {
    if (hit.min_y >= cylinder_bottom - kContactEpsilon) continue;
    if (hit.max_y <= cylinder_top + kContactEpsilon) continue;
    blocked = true;
    target_y = std::min(target_y, hit.min_y);
  }

  if (!blocked) return std::nullopt;
  return target_y - cylinder.origin.y;
}

}  // namespace

PlacementResult placeCylinderAt(const StaticCollisionIndex& index, const ArxVector3& position, float radius,
                                float height, float probe_depth, float tolerance) {
  Cylinder cylinder{position, radius, height};
  cylinder.origin.y += probe_depth;
  bool had_contact = false;
  int guard = 32;
  while (guard-- > 0) {
    std::optional<float> offset = verticalPlacementOffset(index, cylinder);
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
