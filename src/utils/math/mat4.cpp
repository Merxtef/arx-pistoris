// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/math/mat4.h"

#include "arx_pistoris/base/math.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace pistoris::math {
namespace {

bool finiteMatrix(const Mat4& matrix) noexcept {
  for (float value : matrix.m)
    if (!std::isfinite(value)) return false;
  return true;
}

std::optional<float> finiteFloat(double value) noexcept {
  if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) return std::nullopt;
  return static_cast<float>(value);
}

}  // namespace

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
  return {m(0, 0) * v.x + m(0, 1) * v.y + m(0, 2) * v.z,
          m(1, 0) * v.x + m(1, 1) * v.y + m(1, 2) * v.z,
          m(2, 0) * v.x + m(2, 1) * v.y + m(2, 2) * v.z};
}

Mat4 fromQuat(const ArxQuat& q) {
  float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
  float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
  float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  Mat4 r = kIdentityMat4;
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
  if (!finiteMatrix(m)) return std::nullopt;
  const double a00 = m(0, 0), a01 = m(0, 1), a02 = m(0, 2);
  const double a10 = m(1, 0), a11 = m(1, 1), a12 = m(1, 2);
  const double a20 = m(2, 0), a21 = m(2, 1), a22 = m(2, 2);

  const double det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) + a02 * (a10 * a21 - a11 * a20);
  if (det == 0.0 || !std::isfinite(det)) return std::nullopt;
  const double inverse_det = 1.0 / det;

  const std::array<double, 9> linear = {
      (a11 * a22 - a12 * a21) * inverse_det,
      -(a01 * a22 - a02 * a21) * inverse_det,
      (a01 * a12 - a02 * a11) * inverse_det,
      -(a10 * a22 - a12 * a20) * inverse_det,
      (a00 * a22 - a02 * a20) * inverse_det,
      -(a00 * a12 - a02 * a10) * inverse_det,
      (a10 * a21 - a11 * a20) * inverse_det,
      -(a00 * a21 - a01 * a20) * inverse_det,
      (a00 * a11 - a01 * a10) * inverse_det,
  };

  Mat4 out = kIdentityMat4;
  for (std::size_t row = 0; row < 3U; ++row)
    for (std::size_t column = 0; column < 3U; ++column) {
      const std::optional<float> value = finiteFloat(linear[row * 3U + column]);
      if (!value) return std::nullopt;
      out(static_cast<int>(row), static_cast<int>(column)) = *value;
    }

  const double tx = m(0, 3), ty = m(1, 3), tz = m(2, 3);
  for (std::size_t row = 0; row < 3U; ++row) {
    const std::optional<float> value =
        finiteFloat(-(linear[row * 3U] * tx + linear[row * 3U + 1U] * ty + linear[row * 3U + 2U] * tz));
    if (!value) return std::nullopt;
    out(static_cast<int>(row), 3) = *value;
  }
  return out;
}

bool isRotationUniformScale(const Mat4& m, float tol) {
  if (!finiteMatrix(m) || !std::isfinite(tol) || tol < 0.0f) return false;
  const std::array<double, 3> c0 = {m(0, 0), m(1, 0), m(2, 0)};
  const std::array<double, 3> c1 = {m(0, 1), m(1, 1), m(2, 1)};
  const std::array<double, 3> c2 = {m(0, 2), m(1, 2), m(2, 2)};
  const double l0 = std::hypot(c0[0], c0[1], c0[2]);
  const double l1 = std::hypot(c1[0], c1[1], c1[2]);
  const double l2 = std::hypot(c2[0], c2[1], c2[2]);
  if (l0 == 0.0f || l1 == 0.0f || l2 == 0.0f) return false;
  const double ref = std::max({l0, l1, l2});
  if (std::abs(l0 - l1) > tol * ref || std::abs(l0 - l2) > tol * ref) return false;
  const auto dot = [](const std::array<double, 3>& left, const std::array<double, 3>& right) {
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  };
  if (std::abs(dot(c0, c1)) > tol * l0 * l1) return false;
  if (std::abs(dot(c0, c2)) > tol * l0 * l2) return false;
  if (std::abs(dot(c1, c2)) > tol * l1 * l2) return false;
  return true;
}

}  // namespace pistoris::math
