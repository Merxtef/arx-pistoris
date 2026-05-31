// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/math/mat4.h"

#include "utils/math/vec3.h"

#include <algorithm>
#include <cmath>

namespace pistoris::math {

Mat4 operator*(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row) {
      float s = 0.0f;
      for (int k = 0; k < 4; ++k) s += a(row, k) * b(k, col);
      r(row, col) = s;
    }
  return r;
}

ArxVector3 xformPoint(const Mat4& m, const ArxVector3& p) {
  return {m(0, 0) * p.x + m(0, 1) * p.y + m(0, 2) * p.z + m(0, 3),
          m(1, 0) * p.x + m(1, 1) * p.y + m(1, 2) * p.z + m(1, 3),
          m(2, 0) * p.x + m(2, 1) * p.y + m(2, 2) * p.z + m(2, 3)};
}

ArxVector3 xformDir(const Mat4& m, const ArxVector3& v) {
  return {m(0, 0) * v.x + m(0, 1) * v.y + m(0, 2) * v.z, m(1, 0) * v.x + m(1, 1) * v.y + m(1, 2) * v.z,
          m(2, 0) * v.x + m(2, 1) * v.y + m(2, 2) * v.z};
}

Mat4 fromQuat(const ArxQuat& q) {
  float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
  float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
  float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  Mat4 r  = kIdentityMat4;
  r(0, 0) = 1.0f - 2.0f * (yy + zz);
  r(1, 0) = 2.0f * (xy + wz);
  r(2, 0) = 2.0f * (xz - wy);
  r(0, 1) = 2.0f * (xy - wz);
  r(1, 1) = 1.0f - 2.0f * (xx + zz);
  r(2, 1) = 2.0f * (yz + wx);
  r(0, 2) = 2.0f * (xz + wy);
  r(1, 2) = 2.0f * (yz - wx);
  r(2, 2) = 1.0f - 2.0f * (xx + yy);
  return r;
}

Mat4 fromTrs(const ArxVector3& t, const ArxQuat& r, const ArxVector3& s) {
  Mat4 m = fromQuat(r);
  for (int row = 0; row < 3; ++row) {
    m(row, 0) *= s.x;
    m(row, 1) *= s.y;
    m(row, 2) *= s.z;
  }
  m(0, 3) = t.x;
  m(1, 3) = t.y;
  m(2, 3) = t.z;
  return m;
}

std::optional<Mat4> inverseAffine(const Mat4& m) {
  float a00 = m(0, 0), a01 = m(0, 1), a02 = m(0, 2);
  float a10 = m(1, 0), a11 = m(1, 1), a12 = m(1, 2);
  float a20 = m(2, 0), a21 = m(2, 1), a22 = m(2, 2);

  float det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
  if (std::abs(det) < 1e-30f) return std::nullopt;
  float id = 1.0f / det;

  Mat4 out  = kIdentityMat4;
  out(0, 0) = (a11 * a22 - a12 * a21) * id;
  out(0, 1) = -(a01 * a22 - a02 * a21) * id;
  out(0, 2) = (a01 * a12 - a02 * a11) * id;
  out(1, 0) = -(a10 * a22 - a12 * a20) * id;
  out(1, 1) = (a00 * a22 - a02 * a20) * id;
  out(1, 2) = -(a00 * a12 - a02 * a10) * id;
  out(2, 0) = (a10 * a21 - a11 * a20) * id;
  out(2, 1) = -(a00 * a21 - a01 * a20) * id;
  out(2, 2) = (a00 * a11 - a01 * a10) * id;

  float tx = m(0, 3), ty = m(1, 3), tz = m(2, 3);
  out(0, 3) = -(out(0, 0) * tx + out(0, 1) * ty + out(0, 2) * tz);
  out(1, 3) = -(out(1, 0) * tx + out(1, 1) * ty + out(1, 2) * tz);
  out(2, 3) = -(out(2, 0) * tx + out(2, 1) * ty + out(2, 2) * tz);
  return out;
}

bool isRotationUniformScale(const Mat4& m, float tol) {
  ArxVector3 c0{m(0, 0), m(1, 0), m(2, 0)};
  ArxVector3 c1{m(0, 1), m(1, 1), m(2, 1)};
  ArxVector3 c2{m(0, 2), m(1, 2), m(2, 2)};
  float l0 = length(c0), l1 = length(c1), l2 = length(c2);
  if (l0 == 0.0f || l1 == 0.0f || l2 == 0.0f) return false;
  float ref = std::max({l0, l1, l2});
  if (std::abs(l0 - l1) > tol * ref || std::abs(l0 - l2) > tol * ref) return false;
  if (std::abs(dot(c0, c1)) > tol * l0 * l1) return false;
  if (std::abs(dot(c0, c2)) > tol * l0 * l2) return false;
  if (std::abs(dot(c1, c2)) > tol * l1 * l2) return false;
  return true;
}

}  // namespace pistoris::math
