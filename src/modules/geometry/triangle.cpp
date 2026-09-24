// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"  // IWYU pragma: keep
#include "utils/math/bounds.h"

#include <algorithm>
#include <array>

namespace pistoris::geometry {

std::array<ArxVector3, 3> facePositions(const GeometryData& geometry, const Face& face) {
  return {
      geometry.vertices[face.corners[0].vertex].position,
      geometry.vertices[face.corners[1].vertex].position,
      geometry.vertices[face.corners[2].vertex].position,
  };
}

ArxVector3 faceNormalOr(const GeometryData& geometry, const Face& face, ArxVector3 fallback) {
  std::array<ArxVector3, 3> vertices = facePositions(geometry, face);
  return math::normalizeFiniteOr(math::cross(vertices[1] - vertices[0], vertices[2] - vertices[0]), fallback);
}

void refreshFaceNormals(GeometryData& geometry) noexcept {
  for (Face& face : geometry.faces) face.normal = faceNormalOr(geometry, face, {});
}

ArxAabb triangleBounds(const std::array<ArxVector3, 3>& vertices) {
  ArxAabb out;
  out.min = vertices[0];
  out.max = vertices[0];
  math::expand(out, vertices[1]);
  math::expand(out, vertices[2]);
  return out;
}

bool degenerateTriangle(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c) {
  constexpr double kSquaredAspectEpsilon = 1.0e-12;
  const double ab_x = static_cast<double>(b.x) - a.x;
  const double ab_y = static_cast<double>(b.y) - a.y;
  const double ab_z = static_cast<double>(b.z) - a.z;
  const double ac_x = static_cast<double>(c.x) - a.x;
  const double ac_y = static_cast<double>(c.y) - a.y;
  const double ac_z = static_cast<double>(c.z) - a.z;
  const double bc_x = static_cast<double>(c.x) - b.x;
  const double bc_y = static_cast<double>(c.y) - b.y;
  const double bc_z = static_cast<double>(c.z) - b.z;
  const double cross_x = ab_y * ac_z - ab_z * ac_y;
  const double cross_y = ab_z * ac_x - ab_x * ac_z;
  const double cross_z = ab_x * ac_y - ab_y * ac_x;
  const double cross_squared = cross_x * cross_x + cross_y * cross_y + cross_z * cross_z;
  const double ab_squared = ab_x * ab_x + ab_y * ab_y + ab_z * ab_z;
  const double ac_squared = ac_x * ac_x + ac_y * ac_y + ac_z * ac_z;
  const double bc_squared = bc_x * bc_x + bc_y * bc_y + bc_z * bc_z;
  const double longest_edge_squared = std::max({ab_squared, ac_squared, bc_squared});
  return longest_edge_squared == 0.0 ||
         cross_squared <= kSquaredAspectEpsilon * longest_edge_squared * longest_edge_squared;
}

}  // namespace pistoris::geometry
