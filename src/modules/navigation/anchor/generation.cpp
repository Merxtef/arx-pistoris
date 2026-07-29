// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/spatial/grid.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

using math::finite;

enum class AnchorCandidateFailure : std::uint8_t {
  kNoGeometry,
};

struct AnchorGenerationLogDiagnostics {
  std::size_t candidates = 0;
  std::size_t placed = 0;
  std::size_t invalid = 0;
  std::size_t outside_bounds = 0;
  std::size_t repaired = 0;
  std::size_t no_geometry = 0;
  std::size_t ignored_flags = 0;
  std::size_t slope = 0;
  std::size_t too_far = 0;
  std::size_t blocked = 0;
  std::size_t other = 0;
  std::vector<ArxVector3> examples;
};

struct PlacedAnchorCandidate {
  Anchor anchor;
  ArxVector3 requested = {};
  bool repaired = false;
};

bool farEnough(const std::vector<Anchor>& anchors, const ArxVector3& position, double min_distance_squared) {
  for (const Anchor& anchor : anchors)
    if (math::lengthSquared(anchor.position - position) < min_distance_squared) return false;
  return true;
}

CylinderPlacementStatus placeGeneratedAnchor(const StaticAnchorTraversal& traversal, const ArxVector3& candidate,
                                             const AnchorGenOptions& options, ArxVector3& out) {
  return traversal.placeEndpointAt(candidate, options.radius, options.height, out);
}

void recordAnchorCandidateFailure(AnchorGenerationLogDiagnostics& diagnostics, AnchorCandidateFailure failure,
                                  const ArxVector3& candidate) {
  switch (failure) {
    case AnchorCandidateFailure::kNoGeometry:
      ++diagnostics.no_geometry;
      break;
  }
  if (diagnostics.examples.size() < 5) diagnostics.examples.push_back(candidate);
}

std::string anchorCandidateExamples(const std::vector<ArxVector3>& examples) {
  std::string out;
  for (std::size_t i = 0; i < examples.size(); ++i) {
    if (i != 0) out += "; ";
    out += std::format("({:.1f},{:.1f},{:.1f})", examples[i].x, examples[i].y, examples[i].z);
  }
  return out;
}

void logAnchorGenerationDiagnostics(const AnchorGenerationLogDiagnostics& diagnostics) {
  if (diagnostics.candidates == 0) return;
  log(ARX_LOG_DEBUG,
      std::format("Level anchor generation candidates: candidates={}, placed={}, repaired={}, "
                  "invalid={}, outside_bounds={}, no_geometry={}, ignored_flags={}, slope={}, too_far={}, blocked={}, "
                  "other={}",
                  diagnostics.candidates,
                  diagnostics.placed,
                  diagnostics.repaired,
                  diagnostics.invalid,
                  diagnostics.outside_bounds,
                  diagnostics.no_geometry,
                  diagnostics.ignored_flags,
                  diagnostics.slope,
                  diagnostics.too_far,
                  diagnostics.blocked,
                  diagnostics.other));
  if (!diagnostics.examples.empty())
    log(ARX_LOG_DEBUG,
        std::format("Level anchor generation candidate examples: {}", anchorCandidateExamples(diagnostics.examples)));
}

std::optional<PlacedAnchorCandidate> placeAnchorCandidate(
    const StaticAnchorTraversal& traversal, const geometry::SurfaceSupportIndex& geometry_support,
    const GeometryData& geometry, const SurfaceSupportFilter& final_support_filter, const ArxVector3& candidate,
    bool repaired, const AnchorGenOptions& options, const ArxAabb& bounds,
    AnchorGenerationLogDiagnostics& log_diagnostics, AnchorGenDiagnostics* diagnostics) {
  ++log_diagnostics.candidates;
  if (!finite(candidate)) {
    ++log_diagnostics.invalid;
    return std::nullopt;
  }
  if (!math::containsInclusive(bounds, candidate)) {
    ++log_diagnostics.outside_bounds;
    if (log_diagnostics.examples.size() < 5) log_diagnostics.examples.push_back(candidate);
    return std::nullopt;
  }

  ArxVector3 placed{};
  CylinderPlacementStatus status = placeGeneratedAnchor(traversal, candidate, options, placed);
  if (status != CylinderPlacementStatus::kPlaced) {
    ++log_diagnostics.blocked;
    if (log_diagnostics.examples.size() < 5) log_diagnostics.examples.push_back(candidate);
    if (diagnostics) {
      diagnostics->points.push_back({candidate, candidate, AnchorGenDebugStatus::kRejected});
    }
    return std::nullopt;
  }
  if (!hasAllowedFinalGeometrySupport(geometry_support, geometry, placed, options.radius, final_support_filter)) {
    ++log_diagnostics.ignored_flags;
    if (log_diagnostics.examples.size() < 5) log_diagnostics.examples.push_back(candidate);
    if (diagnostics) {
      diagnostics->points.push_back({candidate, placed, AnchorGenDebugStatus::kRejected});
    }
    return std::nullopt;
  }

  return PlacedAnchorCandidate{{placed, options.radius, options.height, 0, {}}, candidate, repaired};
}

bool matchesExistingVerticalBand(const std::vector<PlacedAnchorCandidate>& candidates, const ArxVector3& position,
                                 float height) {
  float max_delta = std::abs(height) * 0.5f;
  for (const PlacedAnchorCandidate& candidate : candidates) {
    if (std::abs(candidate.anchor.position.y - position.y) <= max_delta) return true;
  }
  return false;
}

void addAnchorCandidatesAt(std::vector<Anchor>& anchors, const geometry::SurfaceSupportIndex& support_index,
                           const geometry::SurfaceSupportIndex& geometry_support, const GeometryData& geometry,
                           const SurfaceSupportFilter& final_support_filter, const StaticAnchorTraversal& traversal,
                           const AnchorGenOptions& options, double min_distance_squared, const ArxAabb& bounds, float x,
                           float z, const std::vector<ArxVector3>& offsets,
                           AnchorGenerationLogDiagnostics& log_diagnostics, AnchorGenDiagnostics* diagnostics) {
  bool found_support = false;
  std::vector<PlacedAnchorCandidate> local_candidates;
  for (std::size_t offset_index = 0; offset_index < offsets.size(); ++offset_index) {
    const ArxVector3& offset = offsets[offset_index];
    std::vector<geometry::SurfaceSupportHit> hits = support_index.hitsAt(x + offset.x, z + offset.z);
    geometry::mergeSurfaceSupportHits(hits);
    if (!hits.empty()) found_support = true;
    for (const geometry::SurfaceSupportHit& hit : hits) {
      std::optional<PlacedAnchorCandidate> candidate = placeAnchorCandidate(traversal,
                                                                            geometry_support,
                                                                            geometry,
                                                                            final_support_filter,
                                                                            hit.position,
                                                                            offset_index != 0,
                                                                            options,
                                                                            bounds,
                                                                            log_diagnostics,
                                                                            diagnostics);
      if (!candidate.has_value()) continue;
      if (matchesExistingVerticalBand(local_candidates, candidate->anchor.position, options.height)) continue;
      if (candidate->repaired) {
        ++log_diagnostics.repaired;
      } else {
        ++log_diagnostics.placed;
      }
      local_candidates.push_back(*candidate);
    }
  }
  if (!found_support) recordAnchorCandidateFailure(log_diagnostics, AnchorCandidateFailure::kNoGeometry, {x, 0.0f, z});
  for (const PlacedAnchorCandidate& candidate : local_candidates) {
    if (!farEnough(anchors, candidate.anchor.position, min_distance_squared)) continue;
    if (candidate.repaired && diagnostics) {
      diagnostics->points.push_back({candidate.requested, candidate.anchor.position, AnchorGenDebugStatus::kRepaired});
    }
    anchors.push_back(candidate.anchor);
  }
}

}  // namespace

Error buildNavigationAnchors(std::vector<Anchor>& out, const GeometryData& geometry,
                             const geometry::SurfaceSupportIndex& support_index,
                             const geometry::SurfaceSupportIndex& geometry_support,
                             const SurfaceSupportFilter& final_support_filter, const ArxAabb& bounds,
                             const StaticAnchorTraversal& traversal, const AnchorGenOptions& options,
                             AnchorGenDiagnostics* diagnostics) {
  std::vector<Anchor> anchors;
  if (support_index.empty()) return Error::kEmptyResult;
  AnchorGenerationLogDiagnostics log_diagnostics;
  double min_distance = std::max(1.0f, options.sample_spacing * 0.5f);
  double min_distance_squared = min_distance * min_distance;
  const ArxAabb& support_bounds = support_index.bounds();
  std::vector<ArxVector3> offsets = navigationProbeOffsets(options.radius);
  // Sampling intentionally follows the authored floating-point spacing exactly
  // NOLINTBEGIN(bugprone-float-loop-counter)
  for (float z = spatial::firstCellCenter(support_bounds.min.z, options.sample_spacing); z <= support_bounds.max.z;
       z += options.sample_spacing) {
    for (float x = spatial::firstCellCenter(support_bounds.min.x, options.sample_spacing); x <= support_bounds.max.x;
         x += options.sample_spacing) {
      addAnchorCandidatesAt(anchors,
                            support_index,
                            geometry_support,
                            geometry,
                            final_support_filter,
                            traversal,
                            options,
                            min_distance_squared,
                            bounds,
                            x,
                            z,
                            offsets,
                            log_diagnostics,
                            diagnostics);
    }
  }
  // NOLINTEND(bugprone-float-loop-counter)
  logAnchorGenerationDiagnostics(log_diagnostics);
  std::sort(anchors.begin(), anchors.end(), [](const Anchor& a, const Anchor& b) {
    if (a.position.z != b.position.z) return a.position.z < b.position.z;
    if (a.position.x != b.position.x) return a.position.x < b.position.x;
    return a.position.y < b.position.y;
  });
  if (anchors.empty()) return Error::kEmptyResult;

  out = std::move(anchors);
  return Error::kNone;
}

}  // namespace pistoris::navigation
