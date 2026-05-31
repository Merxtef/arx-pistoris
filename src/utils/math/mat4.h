// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include <optional>

namespace pistoris::math {

// column-major storage: m[col*4 + row]; glTF-native layout
struct Mat4 {
  float m[16];

  float operator()(int row, int col) const { return m[col * 4 + row]; }
  float& operator()(int row, int col) { return m[col * 4 + row]; }
};

inline constexpr Mat4 kIdentityMat4 = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

Mat4 operator*(const Mat4& a, const Mat4& b);

// translation column of an affine matrix
inline ArxVector3 translation(const Mat4& m) { return {m(0, 3), m(1, 3), m(2, 3)}; }

ArxVector3 xformPoint(const Mat4& m, const ArxVector3& p);

// 3x3 linear only; valid for direction vectors under rotation + uniform scale
ArxVector3 xformDir(const Mat4& m, const ArxVector3& v);

// q must be unit
Mat4 fromQuat(const ArxQuat& q);

// T * R(quat) * S(diag); quat must be unit
Mat4 fromTrs(const ArxVector3& t, const ArxQuat& r, const ArxVector3& s);

// assumes bottom row [0,0,0,1]; nullopt if 3x3 is singular
std::optional<Mat4> inverseAffine(const Mat4& m);

// equal-length, mutually orthogonal columns
bool isRotationUniformScale(const Mat4& m, float tol = 1e-3f);

}  // namespace pistoris::math
