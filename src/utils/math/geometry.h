// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace pistoris::math {

inline Vec2<double> xz(const ArxVector3& value) { return {value.x, value.z}; }

inline ArxVector3 lerp(const ArxVector3& a, const ArxVector3& b, double ratio) {
  return a * static_cast<float>(1.0 - ratio) + b * static_cast<float>(ratio);
}

template <class T>
inline Vec2<double> lerp(const Vec2<T>& a, const Vec2<T>& b, double ratio) {
  return {static_cast<double>(a.x) * (1.0 - ratio) + static_cast<double>(b.x) * ratio,
          static_cast<double>(a.y) * (1.0 - ratio) + static_cast<double>(b.y) * ratio};
}

inline bool pointInCircle(const Vec2<double>& point, const Vec2<double>& center, double radius) {
  Vec2<double> delta = point - center;
  return dot(delta, delta) <= radius * radius;
}

inline int dominantAxis(const ArxVector3& value) {
  float x = std::abs(value.x);
  float z = std::abs(value.z);
  float y = std::abs(value.y);
  if (x >= z && x >= y) return 0;
  if (z >= y) return 2;
  return 1;
}

inline ArxVector2 projectExcludingAxis(const ArxVector3& value, int axis) {
  if (axis == 0) return {value.z, value.y};
  if (axis == 2) return {value.x, value.y};
  return {value.x, value.z};
}

inline double orient2d(const ArxVector2& a, const ArxVector2& b, const ArxVector2& c) {
  return static_cast<double>(b.x - a.x) * (c.y - a.y) - static_cast<double>(b.y - a.y) * (c.x - a.x);
}

inline bool betweenInclusive(float value, float a, float b, double tolerance) {
  return static_cast<double>(value) >= std::min(a, b) - tolerance &&
         static_cast<double>(value) <= std::max(a, b) + tolerance;
}

inline bool onSegmentInclusive(const ArxVector2& a, const ArxVector2& b, const ArxVector2& point, double tolerance) {
  return std::abs(orient2d(a, b, point)) <= tolerance && betweenInclusive(point.x, a.x, b.x, tolerance) &&
         betweenInclusive(point.y, a.y, b.y, tolerance);
}

inline bool segmentsIntersectInclusive(const ArxVector2& a, const ArxVector2& b, const ArxVector2& c,
                                       const ArxVector2& d, double tolerance) {
  double ab_c = orient2d(a, b, c);
  double ab_d = orient2d(a, b, d);
  double cd_a = orient2d(c, d, a);
  double cd_b = orient2d(c, d, b);
  if (((ab_c > tolerance && ab_d < -tolerance) || (ab_c < -tolerance && ab_d > tolerance)) &&
      ((cd_a > tolerance && cd_b < -tolerance) || (cd_a < -tolerance && cd_b > tolerance)))
    return true;
  return onSegmentInclusive(a, b, c, tolerance) || onSegmentInclusive(a, b, d, tolerance) ||
         onSegmentInclusive(c, d, a, tolerance) || onSegmentInclusive(c, d, b, tolerance);
}

template <class T>
inline Vec2<double> closestPointOnSegment(const Vec2<T>& point, const Vec2<T>& a, const Vec2<T>& b) {
  Vec2<double> segment_start = {static_cast<double>(a.x), static_cast<double>(a.y)};
  Vec2<double> segment_delta = {static_cast<double>(b.x) - segment_start.x, static_cast<double>(b.y) - segment_start.y};
  Vec2<double> projected = {static_cast<double>(point.x), static_cast<double>(point.y)};
  double len_squared = lengthSquared(segment_delta);
  if (len_squared <= std::numeric_limits<double>::epsilon()) return segment_start;
  double t = std::clamp(dot(projected - segment_start, segment_delta) / len_squared, 0.0, 1.0);
  return segment_start + segment_delta * t;
}

template <class T>
inline double distancePointSegment(const Vec2<T>& point, const Vec2<T>& a, const Vec2<T>& b) {
  return length(Vec2<double>{static_cast<double>(point.x), static_cast<double>(point.y)} -
                closestPointOnSegment(point, a, b));
}

inline double projectedAreaXz2(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c) {
  return cross(xz(b) - xz(a), xz(c) - xz(a));
}

inline bool barycentricXz(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c, float x, float z,
                          std::array<double, 3>& out, double tolerance = 1.0e-5) {
  double denom = projectedAreaXz2(a, b, c);
  if (std::abs(denom) <= std::numeric_limits<double>::epsilon()) return false;
  double w0 = ((static_cast<double>(b.x) - x) * (static_cast<double>(c.z) - z) -
               (static_cast<double>(b.z) - z) * (static_cast<double>(c.x) - x)) /
              denom;
  double w1 = ((static_cast<double>(c.x) - x) * (static_cast<double>(a.z) - z) -
               (static_cast<double>(c.z) - z) * (static_cast<double>(a.x) - x)) /
              denom;
  double w2 = 1.0 - w0 - w1;
  if (w0 < -tolerance || w1 < -tolerance || w2 < -tolerance) return false;
  out = {w0, w1, w2};
  return true;
}

inline bool closestTriangleWeightsXz(const std::array<ArxVector3, 3>& vertices, const Vec2<double>& point,
                                     std::array<double, 3>& weights) {
  Vec2<double> a = xz(vertices[0]);
  Vec2<double> b = xz(vertices[1]);
  Vec2<double> c = xz(vertices[2]);
  Vec2<double> ab = b - a;
  Vec2<double> ac = c - a;
  if (std::abs(cross(ab, ac)) <= std::numeric_limits<double>::epsilon()) return false;

  Vec2<double> ap = point - a;
  double d1 = dot(ab, ap);
  double d2 = dot(ac, ap);
  if (d1 <= 0.0 && d2 <= 0.0) {
    weights = {1.0, 0.0, 0.0};
    return true;
  }

  Vec2<double> bp = point - b;
  double d3 = dot(ab, bp);
  double d4 = dot(ac, bp);
  if (d3 >= 0.0 && d4 <= d3) {
    weights = {0.0, 1.0, 0.0};
    return true;
  }

  double vc = d1 * d4 - d3 * d2;
  if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
    double v = d1 / (d1 - d3);
    weights = {1.0 - v, v, 0.0};
    return true;
  }

  Vec2<double> cp = point - c;
  double d5 = dot(ab, cp);
  double d6 = dot(ac, cp);
  if (d6 >= 0.0 && d5 <= d6) {
    weights = {0.0, 0.0, 1.0};
    return true;
  }

  double vb = d5 * d2 - d1 * d6;
  if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
    double w = d2 / (d2 - d6);
    weights = {1.0 - w, 0.0, w};
    return true;
  }

  double va = d3 * d6 - d5 * d4;
  if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
    double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    weights = {0.0, 1.0 - w, w};
    return true;
  }

  double denom = 1.0 / (va + vb + vc);
  double v = vb * denom;
  double w = vc * denom;
  weights = {1.0 - v - w, v, w};
  return true;
}

inline ArxVector3 interpolate(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c,
                              const std::array<double, 3>& weights) {
  return {
      static_cast<float>(weights[0] * a.x + weights[1] * b.x + weights[2] * c.x),
      static_cast<float>(weights[0] * a.y + weights[1] * b.y + weights[2] * c.y),
      static_cast<float>(weights[0] * a.z + weights[1] * b.z + weights[2] * c.z),
  };
}

inline bool closestPointOnTriangleXz(const std::array<ArxVector3, 3>& vertices, const Vec2<double>& point,
                                     ArxVector3& out) {
  std::array<double, 3> weights{};
  if (!closestTriangleWeightsXz(vertices, point, weights)) return false;
  out = interpolate(vertices[0], vertices[1], vertices[2], weights);
  return true;
}

inline float triangleArea(const ArxVector3& a, const ArxVector3& b, const ArxVector3& c) {
  return static_cast<float>(length(cross(b - a, c - a))) * 0.5f;
}

}  // namespace pistoris::math
