// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

namespace pistoris {

ArxMat3 operator*(const ArxMat3& a, const ArxMat3& b);

ArxVector3 operator*(const ArxMat3& m, const ArxVector3& v);

namespace math {

inline constexpr ArxMat3 kIdentityMat3 = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};

float determinant(const ArxMat3& m);

// caller ensures det != 0; used to transform normals
ArxMat3 inverseTranspose(const ArxMat3& m);

// M = R * diag(sx, sy, sz); scales must be positive
ArxQuat extractRotation(const ArxMat3& m);

// Rx * Ry * Rz (intrinsic XYZ, radians)
ArxMat3 fromEulerXYZ(float rx_rad, float ry_rad, float rz_rad);

// M * diag(s.x, s.y, s.z)
ArxMat3 scaleColumns(const ArxMat3& m, const ArxVector3& s);

}  // namespace math
}  // namespace pistoris
