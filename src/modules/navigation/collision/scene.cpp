// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"

#include "modules/geometry.h"
#include "modules/navigation/collision/internal.h"
#include "modules/navigation/traversal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

namespace pistoris::navigation {
namespace {

using collision::Cylinder;
using collision::PlacementResult;
using collision::StaticCollisionIndex;

struct StepPlacementResult {
  PlacementResult placement;
  ArxVector3 requested = {};
};

float rotateSin(float degrees) {
  constexpr float kPi = 3.14159265358979323846f;
  return std::sin(degrees * kPi / 180.0f);
}

float rotateCos(float degrees) {
  constexpr float kPi = 3.14159265358979323846f;
  return std::cos(degrees * kPi / 180.0f);
}

ArxVector3 rotateAroundY(const ArxVector3& value, float degrees) {
  float s = rotateSin(degrees);
  float c = rotateCos(degrees);
  return {value.x * c + value.z * s, value.y, -value.x * s + value.z * c};
}

}  // namespace

struct NavigationCollisionScene::Impl {
  StaticCollisionIndex index;

  explicit Impl(const GeometryData& geometry) : index(geometry) {}

  StepPlacementResult tryStep(Cylinder& cylinder, const ArxVector3& step,
                              const CylinderTraversalOptions& options) const {
    Cylinder test = cylinder;
    test.origin = test.origin + step;
    ArxVector3 requested = test.origin;
    PlacementResult placed = collision::placeCylinderAt(
        index, test.origin, test.radius, test.height, options.max_step_up, options.max_step_up);
    if (placed.status == CylinderPlacementStatus::kPlaced) {
      test = placed.cylinder;
      cylinder = test;
    }
    return {placed, requested};
  }
};

NavigationCollisionScene::NavigationCollisionScene(const GeometryData& geometry)
    : impl_(std::make_unique<Impl>(geometry)) {}

NavigationCollisionScene::~NavigationCollisionScene() = default;

CylinderPlacementStatus NavigationCollisionScene::endpointStatus(const ArxVector3& position, float radius, float height,
                                                                 float probe_depth, float tolerance) const {
  return collision::placeCylinderAt(impl_->index, position, radius, height, probe_depth, tolerance).status;
}

CylinderPlacementStatus NavigationCollisionScene::placeEndpointAt(const ArxVector3& position, float radius,
                                                                  float height, float probe_depth, float tolerance,
                                                                  ArxVector3& out) const {
  PlacementResult placed = collision::placeCylinderAt(impl_->index, position, radius, height, probe_depth, tolerance);
  out = placed.cylinder.origin;
  return placed.status;
}

namespace {

CylinderTraversalStatus startTraversalStatus(CylinderPlacementStatus status) {
  switch (status) {
    case CylinderPlacementStatus::kPlaced:
      return CylinderTraversalStatus::kTraversable;
    case CylinderPlacementStatus::kInvalid:
      return CylinderTraversalStatus::kStartInvalid;
    case CylinderPlacementStatus::kNoSupport:
      return CylinderTraversalStatus::kStartNoSupport;
    case CylinderPlacementStatus::kTooFar:
      return CylinderTraversalStatus::kStartTooFar;
    case CylinderPlacementStatus::kUnresolved:
      return CylinderTraversalStatus::kStartUnresolved;
  }
  return CylinderTraversalStatus::kStartUnresolved;
}

CylinderTraversalStatus stepTraversalStatus(CylinderPlacementStatus status) {
  switch (status) {
    case CylinderPlacementStatus::kPlaced:
      return CylinderTraversalStatus::kTraversable;
    case CylinderPlacementStatus::kInvalid:
      return CylinderTraversalStatus::kStepInvalid;
    case CylinderPlacementStatus::kNoSupport:
      return CylinderTraversalStatus::kStepNoSupport;
    case CylinderPlacementStatus::kTooFar:
      return CylinderTraversalStatus::kStepTooFar;
    case CylinderPlacementStatus::kUnresolved:
      return CylinderTraversalStatus::kStepUnresolved;
  }
  return CylinderTraversalStatus::kStepUnresolved;
}

void recordTraversalFailure(CylinderTraversalFailure* failure, const ArxVector3& requested, const ArxVector3& resolved,
                            CylinderTraversalAttempt attempt, int step_index) {
  if (!failure) return;
  *failure = {.requested = requested, .resolved = resolved, .attempt = attempt, .step_index = step_index};
}

}  // namespace

CylinderTraversalStatus NavigationCollisionScene::traversalStatus(const ArxVector3& first, const ArxVector3& second,
                                                                  float radius, float height,
                                                                  const CylinderTraversalOptions& options,
                                                                  CylinderTraversalFailure* failure) const {
  float start_tolerance = cylinderEndpointPlacementTolerance(height);
  PlacementResult start = collision::placeCylinderAt(
      impl_->index, first, radius, height, cylinderEndpointPlacementProbeDepth(height), start_tolerance);
  if (start.status != CylinderPlacementStatus::kPlaced) {
    recordTraversalFailure(failure, first, start.cylinder.origin, CylinderTraversalAttempt::kStart, 0);
    return startTraversalStatus(start.status);
  }
  Cylinder cylinder = start.cylinder;

  ArxVector3 delta = second - cylinder.origin;
  float distance = static_cast<float>(math::length(delta));
  if (distance < 0.1f) return CylinderTraversalStatus::kTraversable;
  ArxVector3 direction = delta * (1.0f / distance);

  int steps = 0;
  while (distance > 0.0f && steps++ < options.max_steps) {
    float step_distance = std::min(distance, options.max_step_distance);
    ArxVector3 step = direction * step_distance;
    distance -= step_distance;

    StepPlacementResult direct = impl_->tryStep(cylinder, step, options);
    if (direct.placement.status == CylinderPlacementStatus::kPlaced) continue;

    bool found = false;
    CylinderPlacementStatus failure_status = direct.placement.status;
    ArxVector3 failure_requested = direct.requested;
    ArxVector3 failure_resolved = direct.placement.cylinder.origin;
    CylinderTraversalAttempt failure_attempt = CylinderTraversalAttempt::kDirect;
    if (std::abs(direction.x) > std::numeric_limits<float>::epsilon() ||
        std::abs(direction.z) > std::numeric_limits<float>::epsilon()) {
      for (std::pair<float, CylinderTraversalAttempt> angle : {
               std::pair{30.0f, CylinderTraversalAttempt::kLeft30},
               std::pair{60.0f, CylinderTraversalAttempt::kLeft60},
               std::pair{90.0f, CylinderTraversalAttempt::kLeft90},
           }) {
        StepPlacementResult left =
            impl_->tryStep(cylinder, rotateAroundY(direction, angle.first) * step_distance, options);
        if (left.placement.status == CylinderPlacementStatus::kPlaced) {
          found = true;
          break;
        }
        failure_status = left.placement.status;
        failure_requested = left.requested;
        failure_resolved = left.placement.cylinder.origin;
        failure_attempt = angle.second;
        CylinderTraversalAttempt right_attempt =
            angle.second == CylinderTraversalAttempt::kLeft30   ? CylinderTraversalAttempt::kRight30
            : angle.second == CylinderTraversalAttempt::kLeft60 ? CylinderTraversalAttempt::kRight60
                                                                : CylinderTraversalAttempt::kRight90;
        StepPlacementResult right =
            impl_->tryStep(cylinder, rotateAroundY(direction, -angle.first) * step_distance, options);
        if (right.placement.status == CylinderPlacementStatus::kPlaced) {
          found = true;
          break;
        }
        failure_status = right.placement.status;
        failure_requested = right.requested;
        failure_resolved = right.placement.cylinder.origin;
        failure_attempt = right_attempt;
      }
    }
    if (!found) {
      recordTraversalFailure(failure, failure_requested, failure_resolved, failure_attempt, steps);
      return stepTraversalStatus(failure_status);
    }
  }

  if (steps > options.max_steps) {
    recordTraversalFailure(failure, second, cylinder.origin, CylinderTraversalAttempt::kFinal, steps);
    return CylinderTraversalStatus::kMaxSteps;
  }
  if (math::length(cylinder.origin - second) > options.max_step_up) {
    recordTraversalFailure(failure, second, cylinder.origin, CylinderTraversalAttempt::kFinal, steps);
    return CylinderTraversalStatus::kEndMismatch;
  }
  return CylinderTraversalStatus::kTraversable;
}

bool NavigationCollisionScene::traversable(const ArxVector3& first, const ArxVector3& second, float radius,
                                           float height, const CylinderTraversalOptions& options) const {
  return traversalStatus(first, second, radius, height, options) == CylinderTraversalStatus::kTraversable;
}

}  // namespace pistoris::navigation
