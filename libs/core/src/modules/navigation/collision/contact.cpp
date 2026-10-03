// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.hpp"

#include "modules/navigation/collision/internal.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace pistoris::navigation::collision {
namespace {

bool pointInCircle(float x, float z, const Cylinder& cylinder) {
  double radius = std::max(0.0f, cylinder.radius - 0.01f);
  return math::pointInCircle({x, z}, math::xz(cylinder.origin), radius);
}

bool overlapsCylinderXzBounds(const TraversalFace& face, const Cylinder& cylinder) {
  float radius = broadphaseRadius(cylinder);
  return face.min.x <= cylinder.origin.x + radius && face.max.x >= cylinder.origin.x - radius &&
         face.min.z <= cylinder.origin.z + radius && face.max.z >= cylinder.origin.z - radius;
}

void addFootprintSample(const Cylinder& cylinder, const ArxVector3& sample, FootprintHit& hit, bool& has_sample) {
  if (!pointInCircle(sample.x, sample.z, cylinder)) return;
  if (!has_sample) {
    hit.point = sample;
    hit.min_y = sample.y;
    hit.max_y = sample.y;
    has_sample = true;
    return;
  }
  hit.min_y = std::min(hit.min_y, sample.y);
  hit.max_y = std::max(hit.max_y, sample.y);
}

void addEdgeFootprintSamples(const ArxVector3& a, const ArxVector3& b, const Cylinder& cylinder, FootprintHit& hit,
                             bool& has_sample) {
  Vec2<double> av = math::xz(a);
  Vec2<double> bv = math::xz(b);
  Vec2<double> ab = bv - av;
  double ab_len_squared = math::dot(ab, ab);
  if (ab_len_squared <= std::numeric_limits<double>::epsilon()) {
    addFootprintSample(cylinder, a, hit, has_sample);
    return;
  }

  Vec2<double> center{cylinder.origin.x, cylinder.origin.z};
  double closest = std::clamp(math::dot(center - av, ab) / ab_len_squared, 0.0, 1.0);
  addFootprintSample(cylinder, math::lerp(a, b, closest), hit, has_sample);

  Vec2<double> ac = av - center;
  double aa = ab_len_squared;
  double bb = 2.0 * math::dot(ac, ab);
  double cc = math::dot(ac, ac) - static_cast<double>(cylinder.radius) * cylinder.radius;
  double discriminant = bb * bb - 4.0 * aa * cc;
  if (discriminant < 0.0) return;
  double root = std::sqrt(discriminant);
  for (double t : {(-bb - root) / (2.0 * aa), (-bb + root) / (2.0 * aa)}) {
    if (t >= 0.0 && t <= 1.0) addFootprintSample(cylinder, math::lerp(a, b, t), hit, has_sample);
  }
}

}  // namespace

float broadphaseRadius(const Cylinder& cylinder) {
  return std::max(0.0f, cylinder.radius) + kCollisionBroadphaseEpsilon;
}

std::optional<FootprintHit> footprintHit(const TraversalFace& face, const Cylinder& cylinder) {
  if (!overlapsCylinderXzBounds(face, cylinder)) return std::nullopt;

  FootprintHit hit;
  hit.normal = face.normal;
  bool has_sample = false;
  ArxVector3 projected{};
  if (math::closestPointOnTriangleXz(face.vertices, {cylinder.origin.x, cylinder.origin.z}, projected))
    addFootprintSample(cylinder, projected, hit, has_sample);
  for (std::size_t i = 0; i < face.vertices.size(); ++i) {
    addFootprintSample(cylinder, face.vertices[i], hit, has_sample);
    addEdgeFootprintSamples(face.vertices[i], face.vertices[(i + 1) % face.vertices.size()], cylinder, hit, has_sample);
  }

  if (!has_sample) return std::nullopt;
  return hit;
}

}  // namespace pistoris::navigation::collision
