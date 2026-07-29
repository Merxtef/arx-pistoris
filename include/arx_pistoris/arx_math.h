// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_ARX_MATH_H
#define ARX_PISTORIS_ARX_MATH_H

#ifdef __cplusplus
namespace pistoris {
#endif

typedef struct ArxVector2 {
  float x;
  float y;
} ArxVector2;

typedef struct ArxVector3 {
  float x;
  float y;
  float z;
} ArxVector3;

typedef struct ArxAngle {
#ifdef __cplusplus
  float pitch = 0.0f;
  float yaw = 0.0f;
  float roll = 0.0f;
#else
  float pitch;
  float yaw;
  float roll;
#endif
} ArxAngle;

typedef struct ArxRect {
#ifdef __cplusplus
  ArxVector2 min = {};
  ArxVector2 max = {};
#else
  ArxVector2 min;
  ArxVector2 max;
#endif
} ArxRect;

typedef struct ArxAabb {
#ifdef __cplusplus
  ArxVector3 min = {};
  ArxVector3 max = {};
#else
  ArxVector3 min;
  ArxVector3 max;
#endif
} ArxAabb;

typedef struct ArxColor3 {
#ifdef __cplusplus
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
#else
  float r;
  float g;
  float b;
#endif
} ArxColor3;

typedef struct ArxQuat {
#ifdef __cplusplus
  float w = 1.0f;
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
#else
  float w;
  float x;
  float y;
  float z;
#endif
} ArxQuat;

#define ARX_QUAT_IDENTITY_INIT {1.0f, 0.0f, 0.0f, 0.0f}

typedef struct ArxMat3 {
  float m[9];
#ifdef __cplusplus
  float operator()(int row, int col) const { return m[row * 3 + col]; }
  float& operator()(int row, int col) { return m[row * 3 + col]; }
#endif
} ArxMat3;

#ifdef __cplusplus
}  // namespace pistoris

using pistoris::ArxAabb;
using pistoris::ArxAngle;
using pistoris::ArxColor3;
using pistoris::ArxMat3;
using pistoris::ArxQuat;
using pistoris::ArxRect;
using pistoris::ArxVector2;
using pistoris::ArxVector3;
#endif

#endif /* ARX_PISTORIS_ARX_MATH_H */
