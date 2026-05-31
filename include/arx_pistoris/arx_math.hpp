// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

namespace pistoris {

template <class T>
struct Vec3 {
  T x, y, z;
};

using ArxVector3 = Vec3<float>;
static_assert(sizeof(ArxVector3) == 12);

struct ArxQuat {
  float w = 1.f;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;
};
static_assert(sizeof(ArxQuat) == 16);

// row-major storage: m[row * 3 + col]
struct ArxMat3 {
  float m[9];

  float operator()(int row, int col) const { return m[row * 3 + col]; }
  float& operator()(int row, int col) { return m[row * 3 + col]; }
};
static_assert(sizeof(ArxMat3) == 36);

struct AffineXform {
  ArxMat3 linear;
  ArxVector3 translation;
};

}  // namespace pistoris
