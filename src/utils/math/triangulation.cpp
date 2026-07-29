// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/math/triangulation.h"

#include "arx_pistoris/arx_math.h"

#include "mapbox/earcut.hpp"
#include "utils/math/geometry.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace pistoris::math {
namespace {

bool simplePolygon(std::span<const ArxVector2> points) {
  for (std::size_t first = 0; first < points.size(); ++first) {
    const std::size_t first_next = (first + 1) % points.size();
    for (std::size_t second = first + 1; second < points.size(); ++second) {
      const std::size_t second_next = (second + 1) % points.size();
      if (first_next == second || second_next == first) continue;
      if (segmentsIntersectInclusive(points[first], points[first_next], points[second], points[second_next], 0.0))
        return false;
    }
  }
  return true;
}

bool validTriangles(std::span<const ArxVector2> points, std::span<const std::uint32_t> triangles) {
  if (triangles.size() % 3 != 0 || triangles.size() / 3 != points.size() - 2) return false;
  for (std::size_t i = 0; i < triangles.size(); i += 3) {
    const std::uint32_t a = triangles[i];
    const std::uint32_t b = triangles[i + 1];
    const std::uint32_t c = triangles[i + 2];
    if (a >= points.size() || b >= points.size() || c >= points.size() ||
        orient2d(points[a], points[b], points[c]) == 0.0)
      return false;
  }
  return true;
}

}  // namespace

TriangulationResult triangulateSimplePolygon(std::span<const ArxVector2> points,
                                             std::vector<std::uint32_t>& triangles) {
  triangles.clear();
  if (points.size() < 3 || points.size() > std::numeric_limits<std::uint32_t>::max())
    return TriangulationResult::kFailed;
  if (!simplePolygon(points)) return TriangulationResult::kNonSimple;

  using Point = std::array<double, 2>;
  std::vector<std::vector<Point>> polygon(1);
  polygon.front().reserve(points.size());
  for (const ArxVector2& point : points)
    polygon.front().push_back({static_cast<double>(point.x), static_cast<double>(point.y)});

  triangles = mapbox::earcut<std::uint32_t>(polygon);
  if (!validTriangles(points, triangles)) {
    triangles.clear();
    return TriangulationResult::kFailed;
  }
  mapbox::refine(triangles, polygon.front());
  if (!validTriangles(points, triangles)) {
    triangles.clear();
    return TriangulationResult::kFailed;
  }
  return TriangulationResult::kSuccess;
}

}  // namespace pistoris::math
