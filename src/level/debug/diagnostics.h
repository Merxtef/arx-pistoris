// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/debug/level_diagnostics.hpp"

#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"

#include <cstddef>

namespace pistoris::level_debug::detail {

inline AnchorGenDebugStatus convert(navigation::AnchorGenDebugStatus value) {
  switch (value) {
    case navigation::AnchorGenDebugStatus::kRepaired:
      return AnchorGenDebugStatus::kRepaired;
    case navigation::AnchorGenDebugStatus::kRejected:
      return AnchorGenDebugStatus::kRejected;
  }
  return AnchorGenDebugStatus::kRejected;
}

inline AnchorConnectionEndpointDebugStatus convert(navigation::AnchorConnectionEndpointDebugStatus value) {
  switch (value) {
    case navigation::AnchorConnectionEndpointDebugStatus::kInvalid:
      return AnchorConnectionEndpointDebugStatus::kInvalid;
    case navigation::AnchorConnectionEndpointDebugStatus::kNoSupport:
      return AnchorConnectionEndpointDebugStatus::kNoSupport;
    case navigation::AnchorConnectionEndpointDebugStatus::kTooFar:
      return AnchorConnectionEndpointDebugStatus::kTooFar;
    case navigation::AnchorConnectionEndpointDebugStatus::kUnresolved:
      return AnchorConnectionEndpointDebugStatus::kUnresolved;
  }
  return AnchorConnectionEndpointDebugStatus::kInvalid;
}

inline AnchorConnectionRejectedDebugReason convert(navigation::AnchorConnectionRejectedDebugReason value) {
  switch (value) {
    case navigation::AnchorConnectionRejectedDebugReason::kInvalid:
      return AnchorConnectionRejectedDebugReason::kInvalid;
    case navigation::AnchorConnectionRejectedDebugReason::kNoSupport:
      return AnchorConnectionRejectedDebugReason::kNoSupport;
    case navigation::AnchorConnectionRejectedDebugReason::kTooFar:
      return AnchorConnectionRejectedDebugReason::kTooFar;
    case navigation::AnchorConnectionRejectedDebugReason::kUnresolved:
      return AnchorConnectionRejectedDebugReason::kUnresolved;
    case navigation::AnchorConnectionRejectedDebugReason::kMaxSteps:
      return AnchorConnectionRejectedDebugReason::kMaxSteps;
    case navigation::AnchorConnectionRejectedDebugReason::kEndMismatch:
      return AnchorConnectionRejectedDebugReason::kEndMismatch;
  }
  return AnchorConnectionRejectedDebugReason::kInvalid;
}

inline AnchorConnectionTraversalAttemptDebugKind convert(navigation::AnchorConnectionTraversalAttemptDebugKind value) {
  switch (value) {
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kStart:
      return AnchorConnectionTraversalAttemptDebugKind::kStart;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kDirect:
      return AnchorConnectionTraversalAttemptDebugKind::kDirect;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kLeft30:
      return AnchorConnectionTraversalAttemptDebugKind::kLeft30;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kRight30:
      return AnchorConnectionTraversalAttemptDebugKind::kRight30;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kLeft60:
      return AnchorConnectionTraversalAttemptDebugKind::kLeft60;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kRight60:
      return AnchorConnectionTraversalAttemptDebugKind::kRight60;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kLeft90:
      return AnchorConnectionTraversalAttemptDebugKind::kLeft90;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kRight90:
      return AnchorConnectionTraversalAttemptDebugKind::kRight90;
    case navigation::AnchorConnectionTraversalAttemptDebugKind::kFinal:
      return AnchorConnectionTraversalAttemptDebugKind::kFinal;
  }
  return AnchorConnectionTraversalAttemptDebugKind::kStart;
}

inline void copyDiagnostics(const navigation::NavSurfaceGenDiagnostics& source, NavSurfaceGenDiagnostics& target) {
  const auto copy_triangles = [](const std::vector<navigation::SurfaceDebugTriangle>& from,
                                 std::vector<SurfaceDebugTriangle>& to) {
    to.resize(from.size());
    for (std::size_t i = 0; i < from.size(); ++i) to[i].vertices = from[i].vertices;
  };
  copy_triangles(source.support, target.support);
  copy_triangles(source.base, target.base);
  copy_triangles(source.repaired, target.repaired);
}

inline void copyDiagnostics(const navigation::NavSurfacePruneDiagnostics& source, NavSurfacePruneDiagnostics& target) {
  target.pruned.resize(source.pruned.size());
  for (std::size_t i = 0; i < source.pruned.size(); ++i) target.pruned[i].vertices = source.pruned[i].vertices;
}

inline void copyDiagnostics(const navigation::AnchorGenDiagnostics& source, AnchorGenDiagnostics& target) {
  target.points.resize(source.points.size());
  for (std::size_t i = 0; i < source.points.size(); ++i) {
    target.points[i] = {
        .requested = source.points[i].requested,
        .resolved = source.points[i].resolved,
        .status = convert(source.points[i].status),
    };
  }
}

inline void copyDiagnostics(const navigation::AnchorComponentPruneDiagnostics& source,
                            AnchorComponentPruneDiagnostics& target) {
  target.pruned = source.pruned;
}

inline void copyDiagnostics(const navigation::AnchorConnectionGenDiagnostics& source,
                            AnchorConnectionGenDiagnostics& target) {
  target.skipped_endpoints.resize(source.skipped_endpoints.size());
  for (std::size_t i = 0; i < source.skipped_endpoints.size(); ++i) {
    target.skipped_endpoints[i] = {
        .requested = source.skipped_endpoints[i].requested,
        .resolved = source.skipped_endpoints[i].resolved,
        .status = convert(source.skipped_endpoints[i].status),
    };
  }

  target.rejected_connections.resize(source.rejected_connections.size());
  for (std::size_t i = 0; i < source.rejected_connections.size(); ++i) {
    const navigation::AnchorConnectionRejectedDebugSegment& from = source.rejected_connections[i];
    target.rejected_connections[i] = {
        .start = from.start,
        .end = from.end,
        .failure_requested = from.failure_requested,
        .failure_resolved = from.failure_resolved,
        .reason = convert(from.reason),
        .attempt = convert(from.attempt),
        .step_index = from.step_index,
    };
  }
}

inline RoomDistanceDebugPoint convert(const rooms::RoomDistanceDebugPoint& source) {
  return {.position = source.position, .room = source.room};
}

inline RoomDistanceDebugSegment convert(const rooms::RoomDistanceDebugSegment& source) {
  return {.start = source.start, .end = source.end, .room_1 = source.room_1, .room_2 = source.room_2};
}

inline RoomDistanceDebugPath convert(const rooms::RoomDistanceDebugPath& source) {
  return {
      .points = source.points,
      .room_1 = source.room_1,
      .room_2 = source.room_2,
      .portal_1 = source.portal_1,
      .portal_2 = source.portal_2,
  };
}

inline void copyDiagnostics(const rooms::RoomDistanceGenDiagnostics& source, RoomDistanceGenDiagnostics& target) {
  target.support_by_room.resize(source.support_by_room.size());
  for (std::size_t room = 0; room < source.support_by_room.size(); ++room) {
    target.support_by_room[room].resize(source.support_by_room[room].size());
    for (std::size_t i = 0; i < source.support_by_room[room].size(); ++i) {
      target.support_by_room[room][i].vertices = source.support_by_room[room][i].vertices;
    }
  }

  const auto copy_points = [](const std::vector<std::vector<rooms::RoomDistanceDebugPoint>>& from,
                              std::vector<std::vector<RoomDistanceDebugPoint>>& to) {
    to.resize(from.size());
    for (std::size_t room = 0; room < from.size(); ++room) {
      to[room].resize(from[room].size());
      for (std::size_t i = 0; i < from[room].size(); ++i) to[room][i] = convert(from[room][i]);
    }
  };
  copy_points(source.portal_access_points_by_room, target.portal_access_points_by_room);
  copy_points(source.sampled_points_by_room, target.sampled_points_by_room);

  target.portal_access_segments.resize(source.portal_access_segments.size());
  for (std::size_t i = 0; i < source.portal_access_segments.size(); ++i)
    target.portal_access_segments[i] = convert(source.portal_access_segments[i]);

  target.in_room_visibility_edges.resize(source.in_room_visibility_edges.size());
  for (std::size_t i = 0; i < source.in_room_visibility_edges.size(); ++i)
    target.in_room_visibility_edges[i] = convert(source.in_room_visibility_edges[i]);

  target.in_room_portal_paths_by_room.resize(source.in_room_portal_paths_by_room.size());
  for (std::size_t room = 0; room < source.in_room_portal_paths_by_room.size(); ++room) {
    target.in_room_portal_paths_by_room[room].resize(source.in_room_portal_paths_by_room[room].size());
    for (std::size_t i = 0; i < source.in_room_portal_paths_by_room[room].size(); ++i)
      target.in_room_portal_paths_by_room[room][i] = convert(source.in_room_portal_paths_by_room[room][i]);
  }

  target.room_pair_paths.resize(source.room_pair_paths.size());
  for (std::size_t i = 0; i < source.room_pair_paths.size(); ++i)
    target.room_pair_paths[i] = convert(source.room_pair_paths[i]);
}

inline void copyDiagnostics(const lights::StaticLightingDiagnostics& source, StaticLightingDiagnostics& target) {
  target = {
      .generated_corners = source.generated_corners,
      .contributing_light_corners = source.contributing_light_corners,
      .skipped_lights = source.skipped_lights,
      .shadow_rays = source.shadow_rays,
      .occluded_shadow_rays = source.occluded_shadow_rays,
  };
}

}  // namespace pistoris::level_debug::detail
