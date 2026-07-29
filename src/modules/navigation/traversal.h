// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include "modules/geometry.h"
#include "modules/navigation.h"

#include <cmath>
#include <cstdint>
#include <memory>

namespace pistoris::navigation {

struct AnchorConnectionGenOptions;

enum class CylinderPlacementStatus : std::uint8_t {
  kPlaced,
  kInvalid,
  kNoSupport,
  kTooFar,
  kUnresolved,
};

enum class CylinderTraversalStatus : std::uint8_t {
  kTraversable,
  kStartInvalid,
  kStartNoSupport,
  kStartTooFar,
  kStartUnresolved,
  kStepInvalid,
  kStepNoSupport,
  kStepTooFar,
  kStepUnresolved,
  kMaxSteps,
  kEndMismatch,
};

enum class CylinderTraversalAttempt : std::uint8_t {
  kStart,
  kDirect,
  kLeft30,
  kRight30,
  kLeft60,
  kRight60,
  kLeft90,
  kRight90,
  kFinal,
};

struct CylinderTraversalFailure {
  ArxVector3 requested = {};
  ArxVector3 resolved = {};
  CylinderTraversalAttempt attempt = CylinderTraversalAttempt::kStart;
  int step_index = 0;
};

struct CylinderTraversalOptions {
  float max_step_distance = 40.0f;
  float max_step_up = 55.0f;
  int max_steps = 100;
};

inline constexpr float kCylinderEndpointProbePadding = 5.0f;

[[nodiscard]] inline float cylinderEndpointPlacementTolerance(float height) { return std::abs(height) * 0.5f; }

[[nodiscard]] inline float cylinderEndpointPlacementProbeDepth(float height) {
  return cylinderEndpointPlacementTolerance(height) + kCylinderEndpointProbePadding;
}

struct SurfaceSupportFilter {
  FaceType ignore_flags = 0;
  float min_up_dot = -1.0f;
};

class NavigationCollisionScene {
 public:
  explicit NavigationCollisionScene(const GeometryData& geometry);
  ~NavigationCollisionScene();

  NavigationCollisionScene(const NavigationCollisionScene&) = delete;
  NavigationCollisionScene& operator=(const NavigationCollisionScene&) = delete;
  NavigationCollisionScene(NavigationCollisionScene&&) = delete;
  NavigationCollisionScene& operator=(NavigationCollisionScene&&) = delete;

  [[nodiscard]] CylinderPlacementStatus endpointStatus(const ArxVector3& position, float radius, float height,
                                                       float probe_depth, float tolerance) const;
  [[nodiscard]] CylinderPlacementStatus placeEndpointAt(const ArxVector3& position, float radius, float height,
                                                        float probe_depth, float tolerance, ArxVector3& out) const;
  [[nodiscard]] CylinderTraversalStatus traversalStatus(const ArxVector3& first, const ArxVector3& second, float radius,
                                                        float height, const CylinderTraversalOptions& options,
                                                        CylinderTraversalFailure* failure = nullptr) const;
  [[nodiscard]] bool traversable(const ArxVector3& first, const ArxVector3& second, float radius, float height,
                                 const CylinderTraversalOptions& options) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

class StaticAnchorTraversal {
 public:
  explicit StaticAnchorTraversal(const GeometryData& geometry);
  StaticAnchorTraversal(const StaticAnchorTraversal&) = delete;
  StaticAnchorTraversal& operator=(const StaticAnchorTraversal&) = delete;
  StaticAnchorTraversal(StaticAnchorTraversal&&) = delete;
  StaticAnchorTraversal& operator=(StaticAnchorTraversal&&) = delete;

  [[nodiscard]] CylinderPlacementStatus endpointStatus(const Anchor& anchor, const AnchorConnectionGenOptions& options,
                                                       ArxVector3* resolved = nullptr) const;
  [[nodiscard]] CylinderPlacementStatus placeEndpointAt(const ArxVector3& position, float radius, float height,
                                                        ArxVector3& out) const;
  [[nodiscard]] CylinderTraversalStatus traversalStatus(const Anchor& first, const Anchor& second,
                                                        const AnchorConnectionGenOptions& options,
                                                        CylinderTraversalFailure* failure = nullptr) const;
  [[nodiscard]] bool traversable(const Anchor& first, const Anchor& second,
                                 const AnchorConnectionGenOptions& options) const;

 private:
  NavigationCollisionScene scene_;
};

}  // namespace pistoris::navigation
