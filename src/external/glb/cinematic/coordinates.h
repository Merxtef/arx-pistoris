// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "external/glb/accessor.h"

namespace pistoris::glb_cinematic {

inline constexpr float kUnitsPerGlbUnit = 100.0f;
inline constexpr float kScreenHalfWidth = 320.0f;
inline constexpr float kScreenHalfHeight = 240.0f;
inline constexpr float kCameraFocalLength = 350.0f;
inline constexpr float kLightPlaneDepth = kCameraFocalLength / kUnitsPerGlbUnit;
inline constexpr ArxQuat kContentBasis = {0.7071067811865476f, 0.7071067811865476f, 0.0f, 0.0f};

struct KeyOrientation {
  ArxQuat rotation = {1.0f, 0.0f, 0.0f, 0.0f};
  float roll = 0.0f;
  bool corrected = false;
};

float gameVerticalFov() noexcept;
glb::Vec3 toGlbPoint(const ArxVector3& value) noexcept;
glb::Vec3 toGlbLightPoint(const ArxVector3& value) noexcept;
ArxQuat toGlbRotation(float roll) noexcept;
bool resolveKeyOrientation(const ArxMat3& rotation, KeyOrientation& out) noexcept;
bool toArxLightPoint(const ArxVector3& local, float vertical_fov, ArxVector3& out) noexcept;

}  // namespace pistoris::glb_cinematic
