// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#ifndef ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API
#error "Define ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API to use the volatile Level debug API"
#endif

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/indices.h"

#include <array>
#include <cstdint>
#include <vector>

// Volatile Level tooling data, not a stable interchange contract

namespace pistoris::level_debug {

struct SurfaceDebugTriangle {
  std::array<ArxVector3, 3> vertices = {};
};

using NavigationDebugTriangle = SurfaceDebugTriangle;

struct NavSurfaceGenDiagnostics {
  std::vector<SurfaceDebugTriangle> support;
  std::vector<SurfaceDebugTriangle> base;
  std::vector<SurfaceDebugTriangle> repaired;
};

struct NavSurfacePruneDiagnostics {
  std::vector<SurfaceDebugTriangle> pruned;
};

enum class AnchorGenDebugStatus : std::uint8_t {
  kRepaired,
  kRejected,
};

struct AnchorGenDebugPoint {
  ArxVector3 requested = {};
  ArxVector3 resolved = {};
  AnchorGenDebugStatus status = AnchorGenDebugStatus::kRejected;
};

struct AnchorGenDiagnostics {
  std::vector<AnchorGenDebugPoint> points;
};

struct AnchorComponentPruneDiagnostics {
  std::vector<ArxVector3> pruned;
};

enum class AnchorConnectionEndpointDebugStatus : std::uint8_t {
  kInvalid,
  kNoSupport,
  kTooFar,
  kUnresolved,
};

struct AnchorConnectionEndpointDebugPoint {
  ArxVector3 requested = {};
  ArxVector3 resolved = {};
  AnchorConnectionEndpointDebugStatus status = AnchorConnectionEndpointDebugStatus::kInvalid;
};

enum class AnchorConnectionRejectedDebugReason : std::uint8_t {
  kInvalid,
  kNoSupport,
  kTooFar,
  kUnresolved,
  kMaxSteps,
  kEndMismatch,
};

enum class AnchorConnectionTraversalAttemptDebugKind : std::uint8_t {
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

struct AnchorConnectionRejectedDebugSegment {
  ArxVector3 start = {};
  ArxVector3 end = {};
  ArxVector3 failure_requested = {};
  ArxVector3 failure_resolved = {};
  AnchorConnectionRejectedDebugReason reason = AnchorConnectionRejectedDebugReason::kInvalid;
  AnchorConnectionTraversalAttemptDebugKind attempt = AnchorConnectionTraversalAttemptDebugKind::kStart;
  int step_index = 0;
};

struct AnchorConnectionGenDiagnostics {
  std::vector<AnchorConnectionEndpointDebugPoint> skipped_endpoints;
  std::vector<AnchorConnectionRejectedDebugSegment> rejected_connections;
};

struct NavigationDiagnostics {
  NavSurfaceGenDiagnostics surface;
  NavSurfacePruneDiagnostics surface_pruning;
  AnchorGenDiagnostics anchors;
  AnchorComponentPruneDiagnostics anchor_pruning;
  AnchorConnectionGenDiagnostics connections;
};

struct RoomDistanceDebugPoint {
  ArxVector3 position = {};
  RoomIndex room = 0;
};

struct RoomDistanceDebugSegment {
  ArxVector3 start = {};
  ArxVector3 end = {};
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
};

struct RoomDistanceDebugPath {
  std::vector<ArxVector3> points;
  RoomIndex room_1 = 0;
  RoomIndex room_2 = 0;
  PortalIndex portal_1 = kInvalidPortalIndex;
  PortalIndex portal_2 = kInvalidPortalIndex;
};

struct RoomDistanceSupportTriangle {
  std::array<ArxVector3, 3> vertices = {};
};

struct RoomDistanceGenDiagnostics {
  std::vector<std::vector<RoomDistanceSupportTriangle>> support_by_room;
  std::vector<std::vector<RoomDistanceDebugPoint>> portal_access_points_by_room;
  std::vector<RoomDistanceDebugSegment> portal_access_segments;
  std::vector<std::vector<RoomDistanceDebugPoint>> sampled_points_by_room;
  std::vector<RoomDistanceDebugSegment> in_room_visibility_edges;
  std::vector<std::vector<RoomDistanceDebugPath>> in_room_portal_paths_by_room;
  std::vector<RoomDistanceDebugPath> room_pair_paths;
};

struct StaticLightingDiagnostics {
  std::uint64_t generated_corners = 0;
  std::uint64_t contributing_light_corners = 0;
  std::uint64_t skipped_lights = 0;
  std::uint64_t shadow_rays = 0;
  std::uint64_t occluded_shadow_rays = 0;
};

}  // namespace pistoris::level_debug
