// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/math/mat3.h"

#include <cmath>

namespace pistoris {

ArxMat3 operator*(const ArxMat3& a, const ArxMat3& b) {
  ArxMat3 r{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      float s = 0.0f;
      for (int k = 0; k < 3; ++k) s += a(i, k) * b(k, j);
      r(i, j) = s;
    }
  return r;
}

ArxVector3 operator*(const ArxMat3& m, const ArxVector3& v) {
  return {m(0, 0) * v.x + m(0, 1) * v.y + m(0, 2) * v.z, m(1, 0) * v.x + m(1, 1) * v.y + m(1, 2) * v.z,
          m(2, 0) * v.x + m(2, 1) * v.y + m(2, 2) * v.z};
}

namespace math {

float determinant(const ArxMat3& m) {
  return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) - m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
         m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
}

ArxMat3 inverseTranspose(const ArxMat3& m) {
  float inv_det = 1.0f / determinant(m);
  ArxMat3 r{};

  r(0, 0) = (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) * inv_det;
  r(0, 1) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) * inv_det;
  r(0, 2) = (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0)) * inv_det;
  r(1, 0) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) * inv_det;
  r(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) * inv_det;
  r(1, 2) = (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1)) * inv_det;
  r(2, 0) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) * inv_det;
  r(2, 1) = (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2)) * inv_det;
  r(2, 2) = (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0)) * inv_det;
  return r;
}

ArxQuat extractRotation(const ArxMat3& m) {
  float sx     = std::sqrt(m(0, 0) * m(0, 0) + m(1, 0) * m(1, 0) + m(2, 0) * m(2, 0));
  float sy     = std::sqrt(m(0, 1) * m(0, 1) + m(1, 1) * m(1, 1) + m(2, 1) * m(2, 1));
  float sz     = std::sqrt(m(0, 2) * m(0, 2) + m(1, 2) * m(1, 2) + m(2, 2) * m(2, 2));
  float inv[3] = {1.0f / sx, 1.0f / sy, 1.0f / sz};
  ArxMat3 r{};
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) r(i, j) = m(i, j) * inv[j];

  float trace = r(0, 0) + r(1, 1) + r(2, 2);
  float w, x, y, z;
  if (trace > 0.0f) {
    float s = 0.5f / std::sqrt(trace + 1.0f);
    w       = 0.25f / s;
    x       = (r(2, 1) - r(1, 2)) * s;
    y       = (r(0, 2) - r(2, 0)) * s;
    z       = (r(1, 0) - r(0, 1)) * s;
  } else if (r(0, 0) > r(1, 1) && r(0, 0) > r(2, 2)) {
    float s = 2.0f * std::sqrt(1.0f + r(0, 0) - r(1, 1) - r(2, 2));
    w       = (r(2, 1) - r(1, 2)) / s;
    x       = 0.25f * s;
    y       = (r(0, 1) + r(1, 0)) / s;
    z       = (r(0, 2) + r(2, 0)) / s;
  } else if (r(1, 1) > r(2, 2)) {
    float s = 2.0f * std::sqrt(1.0f + r(1, 1) - r(0, 0) - r(2, 2));
    w       = (r(0, 2) - r(2, 0)) / s;
    x       = (r(0, 1) + r(1, 0)) / s;
    y       = 0.25f * s;
    z       = (r(1, 2) + r(2, 1)) / s;
  } else {
    float s = 2.0f * std::sqrt(1.0f + r(2, 2) - r(0, 0) - r(1, 1));
    w       = (r(1, 0) - r(0, 1)) / s;
    x       = (r(0, 2) + r(2, 0)) / s;
    y       = (r(1, 2) + r(2, 1)) / s;
    z       = 0.25f * s;
  }
  return ArxQuat{w, x, y, z};
}

ArxMat3 fromEulerXYZ(float rx_rad, float ry_rad, float rz_rad) {
  float cx = std::cos(rx_rad), sx = std::sin(rx_rad);
  float cy = std::cos(ry_rad), sy = std::sin(ry_rad);
  float cz = std::cos(rz_rad), sz = std::sin(rz_rad);

  ArxMat3 rx = kIdentityMat3;
  rx(1, 1)   = cx;
  rx(1, 2)   = -sx;
  rx(2, 1)   = sx;
  rx(2, 2)   = cx;

  ArxMat3 ry = kIdentityMat3;
  ry(0, 0)   = cy;
  ry(0, 2)   = sy;
  ry(2, 0)   = -sy;
  ry(2, 2)   = cy;

  ArxMat3 rz = kIdentityMat3;
  rz(0, 0)   = cz;
  rz(0, 1)   = -sz;
  rz(1, 0)   = sz;
  rz(1, 1)   = cz;

  return rx * ry * rz;
}

ArxMat3 scaleColumns(const ArxMat3& m, const ArxVector3& s) {
  ArxMat3 r = m;
  for (int row = 0; row < 3; ++row) {
    r(row, 0) *= s.x;
    r(row, 1) *= s.y;
    r(row, 2) *= s.z;
  }
  return r;
}

}  // namespace math
}  // namespace pistoris
