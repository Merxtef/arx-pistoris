// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include <cmath>

namespace pistoris {

// Hamilton product; a*b applies b first then a (rotation composition order)
inline ArxQuat operator*(const ArxQuat& a, const ArxQuat& b) {
  return ArxQuat{a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                 a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

inline bool operator==(const ArxQuat& a, const ArxQuat& b) {
  return a.w == b.w && a.x == b.x && a.y == b.y && a.z == b.z;
}

namespace math {

inline constexpr ArxQuat kIdentityQuat = {1.0f, 0.0f, 0.0f, 0.0f};

inline ArxQuat conjugate(const ArxQuat& q) { return {q.w, -q.x, -q.y, -q.z}; }

inline float norm(const ArxQuat& q) { return std::sqrt(q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z); }

// zero-norm -> identity
inline ArxQuat normalize(const ArxQuat& q) {
  float n2 = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
  if (n2 == 0.0f) return kIdentityQuat;
  float inv = 1.0f / std::sqrt(n2);
  return {q.w * inv, q.x * inv, q.y * inv, q.z * inv};
}

}  // namespace math
}  // namespace pistoris
