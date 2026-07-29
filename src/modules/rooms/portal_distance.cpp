// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"

#include "modules/rooms.h"

#include <algorithm>
#include <cmath>

namespace pistoris::rooms {
namespace {

using Vec3d = Vec3<double>;

Vec3d toDouble(const ArxVector3& value) { return {value.x, value.y, value.z}; }

Vec3d subtract(const Vec3d& lhs, const Vec3d& rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z}; }

double distanceSquared(const Vec3d& lhs, const Vec3d& rhs) {
  const Vec3d delta = subtract(lhs, rhs);
  return math::dot(delta, delta);
}

double distanceToCombinationSquared(const Vec3d& point, const Vec3d& origin, const Vec3d& first, const Vec3d& second,
                                    double first_weight, double second_weight) {
  const Vec3d closest = {origin.x + first.x * first_weight + second.x * second_weight,
                         origin.y + first.y * first_weight + second.y * second_weight,
                         origin.z + first.z * first_weight + second.z * second_weight};
  return distanceSquared(point, closest);
}

double pointSegmentDistanceSquared(const Vec3d& point, const Vec3d& first, const Vec3d& second) {
  const Vec3d segment = subtract(second, first);
  const double length_2 = math::dot(segment, segment);
  if (length_2 == 0.0) return distanceSquared(point, first);
  const double proportion = std::clamp(math::dot(subtract(point, first), segment) / length_2, 0.0, 1.0);
  return distanceToCombinationSquared(point, first, segment, {}, proportion, 0.0);
}

double pointTriangleDistanceSquared(const ArxVector3& source_point, const ArxVector3& source_a,
                                    const ArxVector3& source_b, const ArxVector3& source_c) {
  const Vec3d point = toDouble(source_point);
  const Vec3d a = toDouble(source_a);
  const Vec3d b = toDouble(source_b);
  const Vec3d c = toDouble(source_c);
  const Vec3d ab = subtract(b, a);
  const Vec3d ac = subtract(c, a);
  const Vec3d ap = subtract(point, a);
  const double d1 = math::dot(ab, ap);
  const double d2 = math::dot(ac, ap);
  if (d1 <= 0.0 && d2 <= 0.0) return distanceSquared(point, a);

  const Vec3d bp = subtract(point, b);
  const double d3 = math::dot(ab, bp);
  const double d4 = math::dot(ac, bp);
  if (d3 >= 0.0 && d4 <= d3) return distanceSquared(point, b);

  const double vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
    const double weight = d1 / (d1 - d3);
    return distanceToCombinationSquared(point, a, ab, ac, weight, 0.0);
  }

  const Vec3d cp = subtract(point, c);
  const double d5 = math::dot(ab, cp);
  const double d6 = math::dot(ac, cp);
  if (d6 >= 0.0 && d5 <= d6) return distanceSquared(point, c);

  const double vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
    const double weight = d2 / (d2 - d6);
    return distanceToCombinationSquared(point, a, ab, ac, 0.0, weight);
  }

  const double va = d3 * d6 - d5 * d4;
  if (va <= 0.0 && d4 >= d3 && d5 >= d6) {
    const double weight = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return distanceToCombinationSquared(point, a, ab, ac, 1.0 - weight, weight);
  }

  const double denominator = va + vb + vc;
  if (!(denominator > 0.0) || !std::isfinite(denominator)) {
    return std::min({pointSegmentDistanceSquared(point, a, b),
                     pointSegmentDistanceSquared(point, b, c),
                     pointSegmentDistanceSquared(point, c, a)});
  }
  const double inverse = 1.0 / denominator;
  return distanceToCombinationSquared(point, a, ab, ac, vb * inverse, vc * inverse);
}

}  // namespace

double pointPortalDistanceSquared(const ArxVector3& point, const Portal& portal) {
  if (portal.shape == PortalShape::kTriangle) {
    return pointTriangleDistanceSquared(point, portal.vertices[0], portal.vertices[1], portal.vertices[2]);
  }
  const double first = pointTriangleDistanceSquared(point, portal.vertices[0], portal.vertices[1], portal.vertices[3]);
  const double second = pointTriangleDistanceSquared(point, portal.vertices[3], portal.vertices[2], portal.vertices[1]);
  return std::min(first, second);
}

}  // namespace pistoris::rooms
