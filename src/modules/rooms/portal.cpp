// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "utils/math/finite.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace pistoris::rooms {
namespace {

constexpr double kPortalVertexDistanceTolerance = 1.0e-4;
constexpr double kPortalAreaSquaredTolerance = 1.0e-8;
constexpr double kPortalPlanarityRelativeTolerance = 0.02;
constexpr double kPortalPlanarityDistanceCap = 5.0;
constexpr double kPortalOrientationTolerance = 1.0e-8;

using Vec3d = Vec3<double>;

bool samePortalPosition(const ArxVector3& a, const ArxVector3& b) {
  return math::lengthSquared(a - b) <= kPortalVertexDistanceTolerance * kPortalVertexDistanceTolerance;
}

}  // namespace

std::size_t portalVertexCount(PortalShape shape) {
  switch (shape) {
    case PortalShape::kTriangle:
      return 3;
    case PortalShape::kQuad:
      return 4;
  }
  return 0;
}

ArxVector3 portalCentroid(const Portal& portal) {
  const std::size_t count = portalVertexCount(portal.shape);
  if (count == 0) return {};
  ArxVector3 centroid{};
  for (std::size_t i = 0; i < count; ++i) centroid = centroid + portal.vertices[i];
  return centroid / static_cast<float>(count);
}

ArxVector3 portalNormal(const Portal& portal) {
  const std::size_t count = portalVertexCount(portal.shape);
  if (count == 0) return {0.0f, 0.0f, 1.0f};
  const ArxVector3& a = portal.vertices[0];
  const ArxVector3& b = portal.vertices[1];
  const ArxVector3& c = portal.vertices[count == 3U ? 2U : 3U];
  return math::normalizeFiniteOr(math::cross(b - a, c - a), {0.0f, 0.0f, 1.0f});
}

RoomIndex portalSideRoom(const Portal& portal, PortalSide side) {
  return side == PortalSide::kFront ? portal.room_1 : portal.room_2;
}

bool connectsRooms(const Portal& portal, RoomIndex first, RoomIndex second) {
  return (portal.room_1 == first && portal.room_2 == second) || (portal.room_1 == second && portal.room_2 == first);
}

std::optional<PortalIndex> firstDirectPortal(const RoomsData& rooms, RoomIndex first_room, RoomIndex second_room) {
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal) {
    if (connectsRooms(rooms.portals[portal], first_room, second_room)) return portal;
  }
  return std::nullopt;
}

PortalValidation validatePortalGeometry(const Portal& portal) {
  std::size_t count = portalVertexCount(portal.shape);
  if (count == 0) return PortalValidation::kBadShape;
  for (std::size_t i = 0; i < count; ++i) {
    if (!math::finite(portal.vertices[i])) return PortalValidation::kBadVertex;
    for (std::size_t j = i + 1; j < count; ++j)
      if (samePortalPosition(portal.vertices[i], portal.vertices[j])) return PortalValidation::kBadVertex;
  }

  const ArxVector3& a = portal.vertices[0];
  const ArxVector3& b = portal.vertices[1];
  const ArxVector3& c = portal.vertices[count == 3U ? 2U : 3U];
  ArxVector3 normal = math::cross(b - a, c - a);
  double normal_length_squared = math::lengthSquared(normal);
  if (!std::isfinite(normal_length_squared) || normal_length_squared <= kPortalAreaSquaredTolerance)
    return PortalValidation::kDegenerate;
  if (count == 3U) return PortalValidation::kValid;

  const ArxVector3& d = portal.vertices[2];
  ArxVector3 second_normal = math::cross(c - d, b - d);
  if (math::lengthSquared(second_normal) <= kPortalAreaSquaredTolerance) return PortalValidation::kDegenerate;

  const Vec3d a_double = math::toVec3d(a);
  const Vec3d b_double = math::toVec3d(b);
  const Vec3d c_double = math::toVec3d(c);
  const Vec3d d_double = math::toVec3d(d);
  const Vec3d first_normal = math::cross(b_double - a_double, c_double - a_double);
  const Vec3d shared_diagonal = c_double - b_double;
  const Vec3d second_normal_double = math::cross(d_double - b_double, shared_diagonal);
  const double first_normal_length = math::length(first_normal);
  const double shared_diagonal_length = math::length(shared_diagonal);
  const double plane_distance = std::abs(math::dot(first_normal, d_double - a_double)) / first_normal_length;
  const double planarity_scale = math::length(second_normal_double) / shared_diagonal_length;
  const double planarity_tolerance =
      std::min(planarity_scale * kPortalPlanarityRelativeTolerance, kPortalPlanarityDistanceCap);
  if (plane_distance > planarity_tolerance) return PortalValidation::kNonPlanar;

  int axis = math::dominantAxis(normal);
  std::array<ArxVector2, 4> points{};
  for (std::size_t i = 0; i < points.size(); ++i) points[i] = math::projectExcludingAxis(portal.vertices[i], axis);
  if (math::segmentsIntersectInclusive(points[0], points[1], points[2], points[3], kPortalOrientationTolerance) ||
      math::segmentsIntersectInclusive(points[1], points[2], points[3], points[0], kPortalOrientationTolerance))
    return PortalValidation::kSelfIntersecting;
  double first_orientation = math::orient2d(points[0], points[1], points[3]);
  double second_orientation = math::orient2d(points[2], points[3], points[1]);
  bool same_positive =
      first_orientation > kPortalOrientationTolerance && second_orientation > kPortalOrientationTolerance;
  bool same_negative =
      first_orientation < -kPortalOrientationTolerance && second_orientation < -kPortalOrientationTolerance;
  if (!same_positive && !same_negative) return PortalValidation::kInconsistentOrientation;
  return PortalValidation::kValid;
}

}  // namespace pistoris::rooms
