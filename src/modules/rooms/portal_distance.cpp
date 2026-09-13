// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <array>

namespace pistoris::rooms {

double pointPortalDistanceSquared(const ArxVector3& point, const Portal& portal) {
  const Vec3<double> converted_point = math::toVec3d(point);
  if (portal.shape == PortalShape::kTriangle) {
    return math::distancePointTriangleSquared(converted_point,
                                              math::toVec3d(portal.vertices[0]),
                                              math::toVec3d(portal.vertices[1]),
                                              math::toVec3d(portal.vertices[2]));
  }
  const std::array<Vec3<double>, 4> vertices = {math::toVec3d(portal.vertices[0]),
                                                math::toVec3d(portal.vertices[1]),
                                                math::toVec3d(portal.vertices[2]),
                                                math::toVec3d(portal.vertices[3])};
  const double first = math::distancePointTriangleSquared(converted_point, vertices[0], vertices[1], vertices[3]);
  const double second = math::distancePointTriangleSquared(converted_point, vertices[3], vertices[2], vertices[1]);
  return std::min(first, second);
}

}  // namespace pistoris::rooms
