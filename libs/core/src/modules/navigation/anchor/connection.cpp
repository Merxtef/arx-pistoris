// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/navigation.h"
#include "modules/navigation/internal.h"
#include "modules/navigation/traversal.h"
#include "utils/log.h"
#include "utils/spatial/xz_point_index.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::navigation {
namespace {

struct AnchorConnectionEndpointDiagnostics {
  std::size_t anchors = 0;
  std::size_t usable = 0;
  std::size_t invalid = 0;
  std::size_t no_support = 0;
  std::size_t too_far = 0;
  std::size_t unresolved = 0;
  std::vector<ArxVector3> examples;
};

std::string anchorSupportExamples(const std::vector<ArxVector3>& examples) {
  std::string out;
  for (std::size_t i = 0; i < examples.size(); ++i) {
    if (i != 0) out += "; ";
    out += std::format("({:.1f},{:.1f},{:.1f})", examples[i].x, examples[i].y, examples[i].z);
  }
  return out;
}

void recordAnchorEndpointStatus(AnchorConnectionEndpointDiagnostics& diagnostics, CylinderPlacementStatus status,
                                const Anchor& anchor) {
  ++diagnostics.anchors;
  switch (status) {
    case CylinderPlacementStatus::kPlaced:
      ++diagnostics.usable;
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
  if (diagnostics.examples.size() < 5) diagnostics.examples.push_back(anchor.position);
}

bool anchorConnectionDebugStatus(CylinderPlacementStatus status, AnchorConnectionEndpointDebugStatus& out) {
  switch (status) {
    case CylinderPlacementStatus::kPlaced:
      return false;
    case CylinderPlacementStatus::kInvalid:
      out = AnchorConnectionEndpointDebugStatus::kInvalid;
      return true;
    case CylinderPlacementStatus::kNoSupport:
      out = AnchorConnectionEndpointDebugStatus::kNoSupport;
      return true;
    case CylinderPlacementStatus::kTooFar:
      out = AnchorConnectionEndpointDebugStatus::kTooFar;
      return true;
    case CylinderPlacementStatus::kUnresolved:
      out = AnchorConnectionEndpointDebugStatus::kUnresolved;
      return true;
  }
  return false;
}

AnchorConnectionRejectedDebugReason rejectedConnectionReason(CylinderTraversalStatus status) {
  switch (status) {
    case CylinderTraversalStatus::kTraversable:
    case CylinderTraversalStatus::kStartInvalid:
    case CylinderTraversalStatus::kStepInvalid:
      return AnchorConnectionRejectedDebugReason::kInvalid;
    case CylinderTraversalStatus::kStartNoSupport:
    case CylinderTraversalStatus::kStepNoSupport:
      return AnchorConnectionRejectedDebugReason::kNoSupport;
    case CylinderTraversalStatus::kStartTooFar:
    case CylinderTraversalStatus::kStepTooFar:
      return AnchorConnectionRejectedDebugReason::kTooFar;
    case CylinderTraversalStatus::kStartUnresolved:
    case CylinderTraversalStatus::kStepUnresolved:
      return AnchorConnectionRejectedDebugReason::kUnresolved;
    case CylinderTraversalStatus::kMaxSteps:
      return AnchorConnectionRejectedDebugReason::kMaxSteps;
    case CylinderTraversalStatus::kEndMismatch:
      return AnchorConnectionRejectedDebugReason::kEndMismatch;
  }
  return AnchorConnectionRejectedDebugReason::kInvalid;
}

AnchorConnectionTraversalAttemptDebugKind rejectedConnectionAttempt(CylinderTraversalAttempt attempt) {
  switch (attempt) {
    case CylinderTraversalAttempt::kStart:
      return AnchorConnectionTraversalAttemptDebugKind::kStart;
    case CylinderTraversalAttempt::kDirect:
      return AnchorConnectionTraversalAttemptDebugKind::kDirect;
    case CylinderTraversalAttempt::kLeft30:
      return AnchorConnectionTraversalAttemptDebugKind::kLeft30;
    case CylinderTraversalAttempt::kRight30:
      return AnchorConnectionTraversalAttemptDebugKind::kRight30;
    case CylinderTraversalAttempt::kLeft60:
      return AnchorConnectionTraversalAttemptDebugKind::kLeft60;
    case CylinderTraversalAttempt::kRight60:
      return AnchorConnectionTraversalAttemptDebugKind::kRight60;
    case CylinderTraversalAttempt::kLeft90:
      return AnchorConnectionTraversalAttemptDebugKind::kLeft90;
    case CylinderTraversalAttempt::kRight90:
      return AnchorConnectionTraversalAttemptDebugKind::kRight90;
    case CylinderTraversalAttempt::kFinal:
      return AnchorConnectionTraversalAttemptDebugKind::kFinal;
  }
  return AnchorConnectionTraversalAttemptDebugKind::kStart;
}

void logAnchorEndpointDiagnostics(const AnchorConnectionEndpointDiagnostics& diagnostics) {
  if (diagnostics.anchors == 0) return;
  log(ARX_LOG_DEBUG,
      "Anchor connection endpoint placement: anchors={}, usable={}, invalid={}, no_support={}, "
      "too_far={}, unresolved={}",
      diagnostics.anchors,
      diagnostics.usable,
      diagnostics.invalid,
      diagnostics.no_support,
      diagnostics.too_far,
      diagnostics.unresolved);
  if (!diagnostics.examples.empty())
    logLazy(ARX_LOG_DEBUG, [&] {
      return std::format("Anchor connection endpoint placement examples: {}",
                         anchorSupportExamples(diagnostics.examples));
    });
}

}  // namespace

Error buildAnchorConnections(std::vector<AnchorConnection>& out, std::span<const Anchor> anchors,
                             StaticAnchorTraversal& traversal, const AnchorConnectionGenerationOptions& options,
                             AnchorConnectionGenerationDiagnostics* diagnostics) {
  double max_distance_squared = static_cast<double>(options.max_distance) * options.max_distance;
  std::vector<bool> usable(anchors.size(), false);
  AnchorConnectionEndpointDiagnostics endpoint_diagnostics;
  std::size_t skipped = 0;
  for (std::size_t i = 0; i < anchors.size(); ++i) {
    ArxVector3 resolved_endpoint{};
    CylinderPlacementStatus status =
        traversal.endpointStatus(anchors[i], options, diagnostics ? &resolved_endpoint : nullptr);
    recordAnchorEndpointStatus(endpoint_diagnostics, status, anchors[i]);
    usable[i] = status == CylinderPlacementStatus::kPlaced;
    if (!usable[i]) {
      ++skipped;
      AnchorConnectionEndpointDebugStatus debug_status{};
      if (diagnostics && anchorConnectionDebugStatus(status, debug_status)) {
        diagnostics->skipped_endpoints.push_back({anchors[i].position, resolved_endpoint, debug_status});
      }
    }
  }
  logAnchorEndpointDiagnostics(endpoint_diagnostics);
  if (skipped > 0) {
    log(ARX_LOG_WARN, "Anchor connection generation skipped {} anchor(s)", skipped);
  }

  spatial::XzPointIndex anchor_index;
  anchor_index.rebuild(
      anchors.size(), options.max_distance, [&](std::size_t index) { return anchors[index].position; });
  std::vector<std::uint32_t> candidates;
  std::vector<AnchorConnection> connections;
  for (std::uint32_t first = 0; first < anchors.size(); ++first) {
    if (!usable[first]) continue;
    anchor_index.findNeighborCellCandidates(candidates, anchors[first].position.x, anchors[first].position.z);
    for (std::uint32_t second : candidates) {
      if (second <= first) continue;
      if (!usable[second]) continue;
      const double dx = static_cast<double>(anchors[second].position.x) - anchors[first].position.x;
      const double dz = static_cast<double>(anchors[second].position.z) - anchors[first].position.z;
      if (dx * dx + dz * dz > max_distance_squared) continue;
      CylinderTraversalFailure failure;
      CylinderTraversalStatus status =
          traversal.traversalStatus(anchors[first], anchors[second], options, diagnostics ? &failure : nullptr);
      if (status != CylinderTraversalStatus::kTraversable) {
        if (diagnostics) {
          diagnostics->rejected_connections.push_back({
              .start = anchors[first].position,
              .end = anchors[second].position,
              .failure_requested = failure.requested,
              .failure_resolved = failure.resolved,
              .reason = rejectedConnectionReason(status),
              .attempt = rejectedConnectionAttempt(failure.attempt),
              .step_index = failure.step_index,
          });
        }
        continue;
      }
      connections.push_back({first, second});
    }
  }

  out = std::move(connections);
  return Error::kNone;
}

}  // namespace pistoris::navigation
