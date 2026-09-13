// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/geometry.h"  // IWYU pragma: keep

#include <cmath>

namespace pistoris::geometry {

bool segmentTriangleIntersectionT(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a,
                                  const ArxVector3& b, const ArxVector3& c, double& out_t) {
  const ArxVector3 dir = end - start;
  const ArxVector3 edge_1 = b - a;
  const ArxVector3 edge_2 = c - a;
  const ArxVector3 p = math::cross(dir, edge_2);
  const double det = math::dot(edge_1, p);
  if (std::abs(det) <= 1.0e-8) return false;
  const double inv_det = 1.0 / det;
  const ArxVector3 t = start - a;
  const double u = math::dot(t, p) * inv_det;
  if (u < 0.0 || u > 1.0) return false;
  const ArxVector3 q = math::cross(t, edge_1);
  const double v = math::dot(dir, q) * inv_det;
  if (v < 0.0 || u + v > 1.0) return false;
  const double distance = math::dot(edge_2, q) * inv_det;
  if (distance < 0.0 || distance > 1.0) return false;
  out_t = distance;
  return true;
}

bool segmentIntersectsTriangle(const ArxVector3& start, const ArxVector3& end, const ArxVector3& a, const ArxVector3& b,
                               const ArxVector3& c) {
  double t = 0.0;
  return segmentTriangleIntersectionT(start, end, a, b, c, t);
}

}  // namespace pistoris::geometry
