// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

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

struct NavSurfaceGenerationDiagnostics {
  std::vector<SurfaceDebugTriangle> support;
  std::vector<SurfaceDebugTriangle> base;
  std::vector<SurfaceDebugTriangle> repaired;
};

struct NavSurfacePruneDiagnostics {
  std::vector<SurfaceDebugTriangle> pruned;
};

enum class AnchorGenerationDebugStatus : std::uint8_t {
  kRepaired,
  kRejected,
};

struct AnchorGenerationDebugPoint {
  ArxVector3 requested = {};
  ArxVector3 resolved = {};
  AnchorGenerationDebugStatus status = AnchorGenerationDebugStatus::kRejected;
};

struct AnchorGenerationDiagnostics {
  std::vector<AnchorGenerationDebugPoint> points;
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

struct AnchorConnectionGenerationDiagnostics {
  std::vector<AnchorConnectionEndpointDebugPoint> skipped_endpoints;
  std::vector<AnchorConnectionRejectedDebugSegment> rejected_connections;
};

struct NavigationDiagnostics {
  NavSurfaceGenerationDiagnostics surface;
  NavSurfacePruneDiagnostics surface_pruning;
  AnchorGenerationDiagnostics anchors;
  AnchorComponentPruneDiagnostics anchor_pruning;
  AnchorConnectionGenerationDiagnostics connections;
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
  kDuplicateConnection,
  kBadConnectionOrder,
  kBadIndex,
};

struct NavSurfaceSourceOptions {
  float clearance = kDefaultNavSurfaceClearance;
  float support_min_up_cos = kDefaultNavSurfaceSupportMinCos;
  FaceType support_ignore_flags = kDefaultNavSurfaceIgnoreFlags;
};

struct NavSurfaceGenerationOptions : NavSurfaceSourceOptions {
  float radius = kDefaultAnchorRadius;
  float height = kDefaultAnchorHeight;
  float max_step_up = kDefaultNavSurfaceMaxStepUp;
};

struct NavSurfacePruneOptions {
  float min_component_area_ratio = 0.05f;
  double min_component_area = 0.0;
};

struct AnchorGenerationOptions {
  float sample_spacing = 100.0f;
  float radius = kDefaultAnchorRadius;
  float height = kDefaultAnchorHeight;
};

struct AnchorComponentPruneOptions {
  float min_component_anchor_ratio = 0.05f;
  std::uint32_t min_component_anchor_count = 1;
};

struct AnchorConnectionGenerationOptions {
  float max_distance = 150.0f;
  float max_step_distance = 40.0f;
  float max_step_up = 55.0f;
  float radius_scale = 0.9f;
  int max_steps = 100;
};

struct NavSurfaceComponentPrunePlan {
  std::optional<NavSurface> replacement;
  std::size_t removed_components = 0;
  std::size_t removed_triangles = 0;
  double removed_area = 0.0;
};

struct AnchorComponentPrunePlan {
  std::vector<AnchorIndex> remap;
  std::size_t kept_connection_count = 0;
  std::size_t removed_components = 0;
  std::size_t removed_anchors = 0;
};

// --- Validation ---

Error validateAnchorCount(std::size_t count) noexcept;
Error validateAnchor(const Anchor& anchor) noexcept;
Error validateAnchorDefinitions(std::span<const Anchor> anchors);
Error validateConnection(const AnchorConnection& connection, std::size_t anchor_count);
Error validateConnections(std::span<const Anchor> anchors, std::span<const AnchorConnection> connections);
Error validateConnectionPlacement(const NavigationData& navigation, AnchorConnectionIndex index,
                                  const AnchorConnection& connection) noexcept;
Error validateConnectionInsertion(const NavigationData& navigation, const AnchorConnection& connection,
                                  AnchorConnectionIndex& out_index) noexcept;
Error validateSurface(const NavSurface& surface);
Error validateSurface(const std::optional<NavSurface>& surface);
Error validate(const NavigationData& navigation);

// --- Queries ---

std::size_t surfaceComponentCount(const NavSurface& surface);

// --- Mutation ---

void setAnchor(NavigationData& navigation, AnchorIndex index, Anchor anchor) noexcept;
AnchorIndex addAnchor(NavigationData& navigation, Anchor anchor);
void removeAnchor(NavigationData& navigation, AnchorIndex index) noexcept;
void setConnection(NavigationData& navigation, AnchorConnectionIndex index, AnchorConnection connection) noexcept;
void insertConnection(NavigationData& navigation, AnchorConnectionIndex index, AnchorConnection connection);
void removeConnection(NavigationData& navigation, AnchorConnectionIndex index) noexcept;
void replaceAnchors(NavigationData& navigation, std::vector<Anchor>&& anchors,
                    std::vector<AnchorConnection>&& connections) noexcept;
void replaceConnections(NavigationData& navigation, std::vector<AnchorConnection>&& connections) noexcept;
void clearAnchors(NavigationData& navigation) noexcept;
void setSurface(NavigationData& navigation, NavSurface surface) noexcept;
void clearSurface(NavigationData& navigation) noexcept;

// --- Repair ---

void repairAnchorName(const NavigationData& navigation, Anchor& anchor, AnchorIndex ignored = kInvalidAnchorIndex);
std::size_t repairAnchorNames(std::span<Anchor> anchors);
Error pruneSurfaceComponents(NavSurface& surface, const NavSurfacePruneOptions& options,
                             NavSurfacePruneDiagnostics* diagnostics = nullptr);
Error pruneAnchorComponents(std::vector<Anchor>& anchors, std::vector<AnchorConnection>& connections,
                            const AnchorComponentPruneOptions& options,
                            AnchorComponentPruneDiagnostics* diagnostics = nullptr);
Error planSurfaceComponentPrune(NavSurfaceComponentPrunePlan& out, const NavSurface& surface,
                                const NavSurfacePruneOptions& options,
                                NavSurfacePruneDiagnostics* diagnostics = nullptr);
void applySurfaceComponentPrune(NavSurface& surface, NavSurfaceComponentPrunePlan&& plan) noexcept;
Error planAnchorComponentPrune(AnchorComponentPrunePlan& out, std::span<const Anchor> anchors,
                               std::span<const AnchorConnection> connections,
                               const AnchorComponentPruneOptions& options,
                               AnchorComponentPruneDiagnostics* diagnostics = nullptr);
void applyAnchorComponentPrune(std::vector<Anchor>& anchors, std::vector<AnchorConnection>& connections,
                               AnchorComponentPrunePlan&& plan) noexcept;

// --- Generation ---

Error generateSurface(NavSurface& out, const GeometryData& geometry, const NavSurfaceGenerationOptions& options,
                      NavSurfaceGenerationDiagnostics* diagnostics = nullptr);
Error generateSurfaceFromFloor(NavSurface& out, const GeometryData& geometry, const NavSurfaceSourceOptions& options,
                               NavSurfaceGenerationDiagnostics* diagnostics = nullptr);
Error generateAnchors(std::vector<Anchor>& out, const GeometryData& geometry, const NavSurface& surface,
                      const ArxAabb& referenced_bounds, const AnchorGenerationOptions& options,
                      AnchorGenerationDiagnostics* diagnostics = nullptr);
Error generateAnchorConnections(std::vector<AnchorConnection>& out, const GeometryData& geometry,
                                std::span<const Anchor> anchors, const AnchorConnectionGenerationOptions& options,
                                AnchorConnectionGenerationDiagnostics* diagnostics = nullptr);

}  // namespace navigation
}  // namespace pistoris
