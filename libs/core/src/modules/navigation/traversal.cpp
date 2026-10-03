// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/navigation/traversal.h"

#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "utils/math/finite.h"

#include <algorithm>

namespace pistoris::navigation {
namespace {

using math::finite;

CylinderTraversalOptions cylinderTraversalOptions(const AnchorConnectionGenerationOptions& options) {
  return {
      .max_step_distance = options.max_step_distance,
      .max_step_up = options.max_step_up,
      .max_steps = options.max_steps,
  };
}

bool validEndpointCylinder(float radius, float height) {
  return finite(radius) && finite(height) && radius > 0.0f && height < 0.0f;
}

}  // namespace

StaticAnchorTraversal::StaticAnchorTraversal(const GeometryData& geometry) : scene_(geometry) {}

CylinderPlacementStatus StaticAnchorTraversal::endpointStatus(const Anchor& anchor,
                                                              const AnchorConnectionGenerationOptions& options,
                                                              ArxVector3* resolved) {
  if (!finite(anchor.position) || !validEndpointCylinder(anchor.radius, anchor.height) ||
      (anchor.flags & kAnchorFlagBlocked) != 0) {
    if (resolved) *resolved = anchor.position;
    return CylinderPlacementStatus::kInvalid;
  }
  float radius = anchor.radius * options.radius_scale;
  if (resolved) {
    return scene_.placeEndpointAt(anchor.position,
                                  radius,
                                  anchor.height,
                                  cylinderEndpointPlacementProbeDepth(anchor.height),
                                  cylinderEndpointPlacementTolerance(anchor.height),
                                  *resolved,
                                  candidate_scratch_);
  }
  return scene_.endpointStatus(anchor.position,
                               radius,
                               anchor.height,
                               cylinderEndpointPlacementProbeDepth(anchor.height),
                               cylinderEndpointPlacementTolerance(anchor.height),
                               candidate_scratch_);
}

CylinderPlacementStatus StaticAnchorTraversal::placeEndpointAt(const ArxVector3& position, float radius, float height,
                                                               ArxVector3& out) {
  if (!finite(position) || !validEndpointCylinder(radius, height)) return CylinderPlacementStatus::kInvalid;
  return scene_.placeEndpointAt(position,
                                radius,
                                height,
                                cylinderEndpointPlacementProbeDepth(height),
                                cylinderEndpointPlacementTolerance(height),
                                out,
                                candidate_scratch_);
}

bool StaticAnchorTraversal::traversable(const Anchor& first, const Anchor& second,
                                        const AnchorConnectionGenerationOptions& options) {
  return traversalStatus(first, second, options) == CylinderTraversalStatus::kTraversable;
}

CylinderTraversalStatus StaticAnchorTraversal::traversalStatus(const Anchor& first, const Anchor& second,
                                                               const AnchorConnectionGenerationOptions& options,
                                                               CylinderTraversalFailure* failure) {
  float radius = std::min(first.radius, second.radius);
  float height = std::max(first.height, second.height);
  if (radius <= 0.0f || height >= 0.0f) return CylinderTraversalStatus::kStartInvalid;
  radius *= options.radius_scale;
  return scene_.traversalStatus(
      first.position, second.position, radius, height, cylinderTraversalOptions(options), candidate_scratch_, failure);
}

}  // namespace pistoris::navigation
