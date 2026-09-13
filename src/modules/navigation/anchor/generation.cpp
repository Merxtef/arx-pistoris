// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/spatial/grid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

using math::finite;

struct AnchorGenerationLogDiagnostics {
  std::size_t candidates = 0;
  std::size_t placed = 0;
  std::size_t invalid = 0;
  std::size_t outside_bounds = 0;
  std::size_t repaired = 0;
  std::size_t empty_samples = 0;
  std::size_t no_support = 0;
  std::size_t too_far = 0;
  std::size_t unresolved = 0;
  std::size_t rejected_support = 0;
  std::size_t too_close = 0;
  std::vector<ArxVector3> examples;
};

struct PlacedAnchorCandidate {
  Anchor anchor;
  ArxVector3 requested = {};
  bool repaired = false;
};

bool farEnough(const std::vector<Anchor>& anchors, const geometry::PositionIndex& spacing_index,
               const ArxVector3& position, double min_distance_squared, std::vector<std::uint32_t>& candidates) {
  spacing_index.findCandidates(candidates, position);
  for (std::uint32_t candidate : candidates) {
    if (candidate < anchors.size() &&
        math::lengthSquared(anchors[candidate].position - position) < min_distance_squared)
      return false;
  }
  return true;
}

CylinderPlacementStatus placeGeneratedAnchor(StaticAnchorTraversal& traversal, const ArxVector3& candidate,
                                             const AnchorGenerationOptions& options, ArxVector3& out) {
  return traversal.placeEndpointAt(candidate, options.radius, options.height, out);
}

void recordAnchorCandidateExample(AnchorGenerationLogDiagnostics& diagnostics, const ArxVector3& candidate) {
  if (diagnostics.examples.size() < 5) diagnostics.examples.push_back(candidate);
}

void recordAnchorPlacementFailure(AnchorGenerationLogDiagnostics& diagnostics, CylinderPlacementStatus status,
                                  const ArxVector3& candidate) {
  switch (status) {
    case CylinderPlacementStatus::kPlaced:
      return;
    case CylinderPlacementStatus::kInvalid:
      ++diagnostics.invalid;
      break;
    case CylinderPlacementStatus::kNoSupport:
      ++diagnostics.no_support;
      break;
    case CylinderPlacementStatus::kTooFar:
      ++diagnostics.too_far;
      break;
    case CylinderPlacementStatus::kUnresolved:
      ++diagnostics.unresolved;
      break;
  }
  recordAnchorCandidateExample(diagnostics, candidate);
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
  if (diagnostics.candidates == 0 && diagnostics.empty_samples == 0) return;
  log(ARX_LOG_DEBUG,
      "Anchor generation candidates: candidates={}, placed={}, repaired={}, "
      "invalid={}, outside_bounds={}, empty_samples={}, no_support={}, too_far={}, unresolved={}, "
      "rejected_support={}, too_close={}",
      diagnostics.candidates,
      diagnostics.placed,
      diagnostics.repaired,
      diagnostics.invalid,
      diagnostics.outside_bounds,
      diagnostics.empty_samples,
      diagnostics.no_support,
      diagnostics.too_far,
      diagnostics.unresolved,
      diagnostics.rejected_support,
      diagnostics.too_close);
  if (!diagnostics.examples.empty())
    logLazy(ARX_LOG_DEBUG, [&] {
      return std::format("Anchor generation candidate examples: {}", anchorCandidateExamples(diagnostics.examples));
    });
}

std::optional<PlacedAnchorCandidate> placeAnchorCandidate(
    StaticAnchorTraversal& traversal, const geometry::SurfaceSupportIndex& geometry_support,
    const GeometryData& geometry, const SurfaceSupportFilter& final_support_filter, const ArxVector3& candidate,
    bool repaired, const AnchorGenerationOptions& options, const ArxAabb& bounds,
    AnchorGenerationLogDiagnostics& log_diagnostics, AnchorGenerationDiagnostics* diagnostics) {
  ++log_diagnostics.candidates;
  if (!finite(candidate)) {
    ++log_diagnostics.invalid;
    recordAnchorCandidateExample(log_diagnostics, candidate);
    return std::nullopt;
  }
  if (!math::containsInclusive(bounds, candidate)) {
    ++log_diagnostics.outside_bounds;
    recordAnchorCandidateExample(log_diagnostics, candidate);
    return std::nullopt;
  }

  ArxVector3 placed{};
  CylinderPlacementStatus status = placeGeneratedAnchor(traversal, candidate, options, placed);
  if (status != CylinderPlacementStatus::kPlaced) {
    recordAnchorPlacementFailure(log_diagnostics, status, candidate);
    if (diagnostics) {
      diagnostics->points.push_back({candidate, candidate, AnchorGenerationDebugStatus::kRejected});
    }
    return std::nullopt;
  }
  if (!hasAllowedFinalGeometrySupport(geometry_support, geometry, placed, options.radius, final_support_filter)) {
    ++log_diagnostics.rejected_support;
    recordAnchorCandidateExample(log_diagnostics, candidate);
    if (diagnostics) {
      diagnostics->points.push_back({candidate, placed, AnchorGenerationDebugStatus::kRejected});
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
                           const SurfaceSupportFilter& final_support_filter, StaticAnchorTraversal& traversal,
                           const AnchorGenerationOptions& options, double min_distance_squared, const ArxAabb& bounds,
                           float x, float z, std::span<const ArxVector3> offsets,
                           AnchorGenerationLogDiagnostics& log_diagnostics, AnchorGenerationDiagnostics* diagnostics,
                           std::vector<geometry::SurfaceSupportHit>& hits,
                           std::vector<PlacedAnchorCandidate>& local_candidates, geometry::PositionIndex& spacing_index,
                           std::vector<std::uint32_t>& spacing_candidates) {
  bool found_support = false;
  local_candidates.clear();
  for (std::size_t offset_index = 0; offset_index < offsets.size(); ++offset_index) {
    const ArxVector3& offset = offsets[offset_index];
    support_index.findHitsAt(hits, x + offset.x, z + offset.z);
    geometry::mergeSortedSurfaceSupportHits(hits);
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
      if (matchesExistingVerticalBand(local_candidates, candidate->anchor.position, options.height)) {
        ++log_diagnostics.too_close;
        continue;
      }
      local_candidates.push_back(*candidate);
    }
  }
  if (!found_support) {
    ++log_diagnostics.empty_samples;
    recordAnchorCandidateExample(log_diagnostics, {x, 0.0f, z});
  }
  for (const PlacedAnchorCandidate& candidate : local_candidates) {
    if (!farEnough(anchors, spacing_index, candidate.anchor.position, min_distance_squared, spacing_candidates)) {
      ++log_diagnostics.too_close;
      continue;
    }
    anchors.push_back(candidate.anchor);
    if (!spacing_index.tryAdd(static_cast<std::uint32_t>(anchors.size() - 1U), candidate.anchor.position)) {
      anchors.pop_back();
      ++log_diagnostics.invalid;
      recordAnchorCandidateExample(log_diagnostics, candidate.anchor.position);
      continue;
    }
    if (candidate.repaired && diagnostics) {
      diagnostics->points.push_back(
          {candidate.requested, candidate.anchor.position, AnchorGenerationDebugStatus::kRepaired});
    }
    if (candidate.repaired) {
      ++log_diagnostics.repaired;
    } else {
      ++log_diagnostics.placed;
    }
  }
}

}  // namespace

Error buildNavigationAnchors(std::vector<Anchor>& out, const GeometryData& geometry,
                             const geometry::SurfaceSupportIndex& support_index,
                             const geometry::SurfaceSupportIndex& geometry_support,
                             const SurfaceSupportFilter& final_support_filter, const ArxAabb& bounds,
                             StaticAnchorTraversal& traversal, const AnchorGenerationOptions& options,
                             AnchorGenerationDiagnostics* diagnostics) {
  std::vector<Anchor> anchors;
  if (support_index.empty()) return Error::kEmptyResult;
  AnchorGenerationLogDiagnostics log_diagnostics;
  float min_distance = std::max(1.0f, options.sample_spacing * 0.5f);
  double min_distance_squared = static_cast<double>(min_distance) * min_distance;
  const ArxAabb& support_bounds = support_index.bounds();
  const std::array<ArxVector3, 17> offsets = navigationProbeOffsets(options.radius);
  std::vector<geometry::SurfaceSupportHit> support_hits;
  std::vector<PlacedAnchorCandidate> local_candidates;
  geometry::PositionIndex spacing_index(min_distance, geometry::PositionWeldMetric::kEuclidean);
  spacing_index.reservePositionCapacity(support_index.size());
  std::vector<std::uint32_t> spacing_candidates;
  // Float accumulation order affects generated topology
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
                            diagnostics,
                            support_hits,
                            local_candidates,
                            spacing_index,
                            spacing_candidates);
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
