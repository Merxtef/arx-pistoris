// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include <cmath>

namespace pistoris {

// operators live in pistoris (not pistoris::math) so ADL finds them for ArxVector3 args

inline ArxVector3 operator+(const ArxVector3& a, const ArxVector3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }

inline ArxVector3 operator-(const ArxVector3& a, const ArxVector3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

inline ArxVector3 operator-(const ArxVector3& v) { return {-v.x, -v.y, -v.z}; }

inline ArxVector3 operator*(const ArxVector3& v, float s) { return {v.x * s, v.y * s, v.z * s}; }

inline ArxVector3 operator*(float s, const ArxVector3& v) { return v * s; }

inline ArxVector3 operator/(const ArxVector3& v, float s) {
  float inv = 1.0f / s;
  return {v.x * inv, v.y * inv, v.z * inv};
}

inline bool operator==(const ArxVector3& a, const ArxVector3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

namespace math {

inline float dot(const ArxVector3& a, const ArxVector3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline ArxVector3 cross(const ArxVector3& a, const ArxVector3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float lengthSquared(const ArxVector3& v) { return dot(v, v); }

inline float length(const ArxVector3& v) { return std::sqrt(lengthSquared(v)); }

// zero input -> zero vector
inline ArxVector3 normalize(const ArxVector3& v) {
  float len2 = lengthSquared(v);
  if (len2 == 0.0f) return {0.0f, 0.0f, 0.0f};
  float inv = 1.0f / std::sqrt(len2);
  return {v.x * inv, v.y * inv, v.z * inv};
}

// zero input -> fallback (caller-supplied, typically a default axis)
inline ArxVector3 normalizeOr(const ArxVector3& v, const ArxVector3& fallback) {
  float len2 = lengthSquared(v);
  if (len2 == 0.0f) return fallback;
  float inv = 1.0f / std::sqrt(len2);
  return {v.x * inv, v.y * inv, v.z * inv};
}

inline ArxVector3 componentMin(const ArxVector3& a, const ArxVector3& b) {
  return {a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z};
}

inline ArxVector3 componentMax(const ArxVector3& a, const ArxVector3& b) {
  return {a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z};
}

}  // namespace math
}  // namespace pistoris
