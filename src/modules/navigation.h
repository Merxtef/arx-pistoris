// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

namespace navigation {

inline constexpr float kDefaultAnchorRadius = 50.0f;
inline constexpr float kDefaultAnchorHeight = -165.0f;
inline constexpr std::int16_t kAnchorFlagBlocked = 1 << 3;
inline constexpr std::int16_t kAnchorFlagsAll = kAnchorFlagBlocked;
inline constexpr float kMinAnchorRadius = 5.0f;
inline constexpr float kMinAnchorSpacing = kMinAnchorRadius * 2.0f;
inline constexpr float kMinAnchorHeight = -20.0f;
inline constexpr float kDefaultNavSurfaceMaxStepUp = 55.0f;
inline constexpr float kDefaultNavSurfaceClearance = 5.0f;
inline constexpr float kDefaultNavSurfaceSupportMinCos = 0.5881716976750462f;
inline constexpr FaceType kDefaultNavSurfaceIgnoreFlags = kFaceBitWater | kFaceBitTrans | kFaceBitNocol | kFaceBitLava;

struct SurfaceDebugTriangle {
  std::array<ArxVector3, 3> vertices = {};
};

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

}  // namespace navigation

struct Anchor {
  ArxVector3 position = {};
  float radius = navigation::kDefaultAnchorRadius;
  float height = navigation::kDefaultAnchorHeight;
  std::int16_t flags = 0;
  std::string name;
};

struct AnchorConnection {
  AnchorIndex first = 0;
  AnchorIndex second = 0;
};

struct NavSurfaceTriangle {
  std::array<NavSurfaceVertexIndex, 3> vertices = {};
};

struct NavSurface {
  std::vector<Vertex> vertices;
  std::vector<NavSurfaceTriangle> triangles;
};

struct NavigationData {
  std::vector<Anchor> anchors;
  std::vector<AnchorConnection> connections;
  std::optional<NavSurface> surface;
};

namespace navigation {

enum class Error : std::uint8_t {
  kNone,
  kInvalidOptions,
  kBadFaceVertex,
  kBadSurface,
  kTooManySurfaceVertices,
  kBadSurfaceVertex,
  kBadSurfaceTriangle,
  kDegenerateSurfaceTriangle,
  kSurfaceRequired,
  kEmptyResult,
  kTooManyAnchors,
  kBadAnchorName,
  kDuplicateAnchorName,
  kBadAnchorPosition,
  kBadAnchorRadius,
  kBadAnchorHeight,
  kBadAnchorFlags,
  kTooManyConnections,
  kBadConnectionIndex,
  kBadConnectionOrder,
};

struct NavSurfaceSourceOptions {
  float clearance = kDefaultNavSurfaceClearance;
  float support_min_up_cos = kDefaultNavSurfaceSupportMinCos;
  FaceType support_ignore_flags = kDefaultNavSurfaceIgnoreFlags;
};

struct NavSurfaceGenOptions : NavSurfaceSourceOptions {
  float radius = kDefaultAnchorRadius;
  float height = kDefaultAnchorHeight;
  float max_step_up = kDefaultNavSurfaceMaxStepUp;
};

struct NavSurfacePruneOptions {
  float min_component_area_ratio = 0.05f;
  double min_component_area = 0.0;
};

struct AnchorGenOptions {
  float sample_spacing = 100.0f;
  float radius = kDefaultAnchorRadius;
  float height = kDefaultAnchorHeight;
};

struct AnchorComponentPruneOptions {
  float min_component_anchor_ratio = 0.05f;
  std::uint32_t min_component_anchor_count = 1;
};

struct AnchorConnectionGenOptions {
  float max_distance = 150.0f;
  float max_step_distance = 40.0f;
  float max_step_up = 55.0f;
  float radius_scale = 0.9f;
  int max_steps = 100;
};

Error validateAnchor(const Anchor& anchor) noexcept;
Error validateAnchorDefinitions(std::span<const Anchor> anchors);
std::size_t makeAnchorNamesUnique(std::span<Anchor> anchors);
Error validateConnection(const AnchorConnection& connection, std::size_t anchor_count);
Error validateConnections(std::span<const Anchor> anchors, std::span<const AnchorConnection> connections);
Error validateSurface(const NavSurface& surface);
Error validateSurface(const std::optional<NavSurface>& surface);
Error validate(const NavigationData& navigation);

Error generateSurface(NavSurface& out, const GeometryData& geometry, const NavSurfaceGenOptions& options,
                      NavSurfaceGenDiagnostics* diagnostics = nullptr);
Error generateSurfaceFromFloorPolygons(NavSurface& out, const GeometryData& geometry,
                                       const NavSurfaceSourceOptions& options,
                                       NavSurfaceGenDiagnostics* diagnostics = nullptr);
Error pruneSurfaceComponents(NavSurface& surface, const NavSurfacePruneOptions& options,
                             NavSurfacePruneDiagnostics* diagnostics = nullptr);
std::size_t surfaceComponentCount(const NavSurface& surface);
Error generateAnchors(std::vector<Anchor>& out, const GeometryData& geometry, const NavSurface& surface,
                      const ArxAabb& referenced_bounds, const AnchorGenOptions& options,
                      AnchorGenDiagnostics* diagnostics = nullptr);
Error pruneAnchorComponents(std::vector<Anchor>& anchors, std::vector<AnchorConnection>& connections,
                            const AnchorComponentPruneOptions& options,
                            AnchorComponentPruneDiagnostics* diagnostics = nullptr);
Error generateAnchorConnections(std::vector<AnchorConnection>& out, const GeometryData& geometry,
                                std::span<const Anchor> anchors, const AnchorConnectionGenOptions& options,
                                AnchorConnectionGenDiagnostics* diagnostics = nullptr);

}  // namespace navigation
}  // namespace pistoris
