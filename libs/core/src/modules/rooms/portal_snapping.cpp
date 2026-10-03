// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace pistoris::rooms {
namespace {

using Vec3d = Vec3<double>;

struct SnapCandidate {
  VertexIndex vertex = kInvalidVertexIndex;
  ArxVector3 target = {};
  double distance_squared = 0.0;
};

double comparisonTolerance(double scale) {
  return 8.0 * static_cast<double>(std::numeric_limits<float>::epsilon()) * scale;
}

bool nearEqual(double first, double second) {
  return std::abs(first - second) <= comparisonTolerance(std::max(std::abs(first), std::abs(second)));
}

ArxVector3 toArxVector3(const Vec3d& value) {
  return {static_cast<float>(value.x), static_cast<float>(value.y), static_cast<float>(value.z)};
}

bool portalAcceptsVertex(const Portal& portal, std::span<const FaceIndex> faces, const RoomsData& rooms) {
  for (FaceIndex face : faces) {
    const RoomIndex room = rooms.face_rooms[face];
    if (room != portal.room_1 && room != portal.room_2) return false;
  }
  return true;
}

Vec3d rawFaceNormal(const Face& face, std::span<const ArxVector3> positions) {
  const Vec3d first = math::toVec3d(positions[face.corners[0].vertex]);
  const Vec3d second = math::toVec3d(positions[face.corners[1].vertex]);
  const Vec3d third = math::toVec3d(positions[face.corners[2].vertex]);
  return math::cross(second - first, third - first);
}

bool safeCandidate(const GeometryData& geometry, const geometry::VertexFaceIndex& face_index,
                   std::span<const ArxVector3> original_positions, std::span<const ArxVector3> positions,
                   VertexIndex vertex) {
  for (FaceIndex face_index_value : face_index.incidentFaces(vertex)) {
    const Face& face = geometry.faces[face_index_value];
    const ArxVector3& first = positions[face.corners[0].vertex];
    const ArxVector3& second = positions[face.corners[1].vertex];
    const ArxVector3& third = positions[face.corners[2].vertex];
    if (geometry::degenerateTriangle(first, second, third)) return false;
    if (math::dot(rawFaceNormal(face, original_positions), rawFaceNormal(face, positions)) <= 0.0) return false;
  }
  return true;
}

Error geometryIndexError(geometry::Error error) {
  switch (error) {
    case geometry::Error::kNone:
      return Error::kNone;
    case geometry::Error::kTooManyVertices:
      return Error::kTooManyVertices;
    case geometry::Error::kTooManyFaces:
      return Error::kTooManyFaces;
    case geometry::Error::kBadFaceVertex:
      return Error::kBadFaceVertex;
    default:
      return Error::kInvalidOptions;
  }
}

}  // namespace

Error snapGeometryToPortals(GeometryData& geometry, const RoomsData& rooms, const PortalSnapOptions& options,
                            PortalSnapStatistics* statistics) {
  if (!(options.radius > 0.0f) || !std::isfinite(options.radius)) return Error::kInvalidOptions;
  Error error = validateFaceRooms(rooms, geometry.faces.size());
  if (error != Error::kNone) return error;
  error = validatePortals(rooms);
  if (error != Error::kNone) return error;

  geometry::VertexFaceIndex face_index;
  error = geometryIndexError(geometry::buildVertexFaceIndex(geometry, face_index));
  if (error != Error::kNone) return error;
  RoomPortalIndex portal_index;
  error = buildRoomPortalIndex(rooms, portal_index);
  if (error != Error::kNone) return error;

  std::vector<PortalSurface> surfaces;
  surfaces.reserve(rooms.portals.size());
  for (const Portal& portal : rooms.portals) {
    std::optional<PortalSurface> surface = projectPortalSurface(portal);
    if (!surface) return Error::kDegeneratePortal;
    surfaces.push_back(*surface);
  }

  PortalSnapStatistics result;
  std::vector<SnapCandidate> candidates;
  candidates.reserve(geometry.vertices.size());
  std::vector<PortalIndex> candidate_portals;
  const double radius_squared = static_cast<double>(options.radius) * options.radius;
  for (std::size_t vertex_value = 0; vertex_value < geometry.vertices.size(); ++vertex_value) {
    const VertexIndex vertex = static_cast<VertexIndex>(vertex_value);
    const std::span<const FaceIndex> incident_faces = face_index.incidentFaces(vertex);
    if (incident_faces.empty()) continue;

    candidate_portals.clear();
    for (FaceIndex face : incident_faces) {
      const std::span<const PortalIndex> portals = portal_index.roomPortals(rooms.face_rooms[face]);
      candidate_portals.insert(candidate_portals.end(), portals.begin(), portals.end());
    }
    std::ranges::sort(candidate_portals);
    candidate_portals.erase(std::unique(candidate_portals.begin(), candidate_portals.end()), candidate_portals.end());

    const Vec3d position = math::toVec3d(geometry.vertices[vertex].position);
    bool has_best = false;
    bool ambiguous = false;
    bool room_conflict = false;
    ArxVector3 best_target{};
    double best_distance = 0.0;
    for (PortalIndex portal : candidate_portals) {
      const Vec3d precise_target = closestPointOnPortalSurface(surfaces[portal], position);
      const ArxVector3 target = toArxVector3(precise_target);
      const double distance_squared = math::lengthSquared(position - math::toVec3d(target));
      if (distance_squared > radius_squared) continue;
      if (!portalAcceptsVertex(rooms.portals[portal], incident_faces, rooms)) {
        room_conflict = true;
        continue;
      }
      if (!has_best || (distance_squared < best_distance && !nearEqual(distance_squared, best_distance))) {
        has_best = true;
        ambiguous = false;
        best_target = target;
        best_distance = distance_squared;
      } else if (nearEqual(distance_squared, best_distance) && target != best_target) {
        ambiguous = true;
      }
    }

    if (!has_best) {
      if (room_conflict) ++result.skipped_room_conflict;
      continue;
    }
    if (ambiguous) {
      ++result.skipped_ambiguous;
      continue;
    }
    if (geometry.vertices[vertex].position == best_target) {
      ++result.already_aligned;
      continue;
    }
    candidates.push_back({vertex, best_target, best_distance});
  }

  std::ranges::sort(candidates, {}, [](const SnapCandidate& candidate) {
    return std::pair{candidate.distance_squared, candidate.vertex};
  });
  result.candidates = candidates.size();

  std::vector<ArxVector3> original_positions;
  original_positions.reserve(geometry.vertices.size());
  for (const Vertex& vertex : geometry.vertices) original_positions.push_back(vertex.position);
  std::vector<ArxVector3> next_positions = original_positions;
  for (const SnapCandidate& candidate : candidates) {
    const ArxVector3 previous = next_positions[candidate.vertex];
    next_positions[candidate.vertex] = candidate.target;
    if (!safeCandidate(geometry, face_index, original_positions, next_positions, candidate.vertex)) {
      next_positions[candidate.vertex] = previous;
      ++result.skipped_face_safety;
      continue;
    }
    ++result.snapped;
  }

  for (std::size_t vertex = 0; vertex < geometry.vertices.size(); ++vertex)
    geometry.vertices[vertex].position = next_positions[vertex];
  geometry::refreshFaceNormals(geometry);
  if (statistics) *statistics = result;
  return Error::kNone;
}

}  // namespace pistoris::rooms
