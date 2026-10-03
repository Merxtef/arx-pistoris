// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <limits>
#include <optional>

namespace pistoris::rooms {

double pointPortalDistanceSquared(const ArxVector3& point, const Portal& portal) {
  const Vec3<double> converted_point{point.x, point.y, point.z};
  const std::optional<PortalSurface> surface = projectPortalSurface(portal);
  if (!surface) return std::numeric_limits<double>::infinity();
  return math::lengthSquared(converted_point - closestPointOnPortalSurface(*surface, converted_point));
}

}  // namespace pistoris::rooms
