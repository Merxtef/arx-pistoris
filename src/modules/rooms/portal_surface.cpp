// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"
#include "utils/math/finite.h"
#include "utils/math/geometry_algorithms.h"

#include <cmath>
#include <cstddef>
#include <optional>

namespace pistoris::rooms {

std::optional<PortalSurface> projectPortalSurface(const Portal& portal) noexcept {
  PortalSurface surface;
  surface.vertex_count = portalVertexCount(portal.shape);
  if (surface.vertex_count == 0) return std::nullopt;
  for (std::size_t vertex = 0; vertex < surface.vertex_count; ++vertex) {
    if (!math::finite(portal.vertices[vertex])) return std::nullopt;
    surface.vertices[vertex] = math::toVec3d(portal.vertices[vertex]);
  }
  if (portal.shape == PortalShape::kTriangle) return surface;

  const Vec3<double> normal =
      math::cross(surface.vertices[1] - surface.vertices[0], surface.vertices[3] - surface.vertices[0]);
  const double normal_length_squared = math::lengthSquared(normal);
  if (!(normal_length_squared > 0.0) || !std::isfinite(normal_length_squared)) return std::nullopt;
  surface.vertices[2] = surface.vertices[2] -
                        normal * (math::dot(surface.vertices[2] - surface.vertices[0], normal) / normal_length_squared);
  return surface;
}

Vec3<double> closestPointOnPortalSurface(const PortalSurface& surface, const Vec3<double>& point) noexcept {
  if (surface.vertex_count == 3U)
    return math::closestPointOnTriangle(point, surface.vertices[0], surface.vertices[1], surface.vertices[2]);
  if (surface.vertex_count != 4U) return point;

  const Vec3<double> first =
      math::closestPointOnTriangle(point, surface.vertices[0], surface.vertices[1], surface.vertices[3]);
  const Vec3<double> second =
      math::closestPointOnTriangle(point, surface.vertices[3], surface.vertices[2], surface.vertices[1]);
  return math::lengthSquared(point - first) <= math::lengthSquared(point - second) ? first : second;
}

}  // namespace pistoris::rooms
