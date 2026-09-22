// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "coordinates.h"

#include "arx_pistoris/base/math.hpp"

#include "external/glb/accessor.h"
#include "utils/math/quat.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace pistoris::glb_cinematic {

namespace {

// Native camera faces +Z with +Y down; glTF camera faces -Z with +Y up
constexpr ArxQuat kCameraFrame = {0.0f, 1.0f, 0.0f, 0.0f};

}  // namespace

float gameVerticalFov() noexcept { return 2.0f * std::atan(kScreenHalfHeight / kCameraFocalLength); }

glb::Vec3 toGlbPoint(const ArxVector3& value) noexcept {
  return {value.x / kUnitsPerGlbUnit, value.y / kUnitsPerGlbUnit, value.z / kUnitsPerGlbUnit};
}

glb::Vec3 toGlbLightPoint(const ArxVector3& value) noexcept {
  return {value.x / kUnitsPerGlbUnit, -kLightPlaneDepth, value.y / kUnitsPerGlbUnit};
}

ArxQuat toGlbRotation(float roll) noexcept {
  return math::angleToQuat({0.0f, 0.0f, -roll}) * kCameraFrame * kContentBasis;
}

bool resolveKeyOrientation(const ArxMat3& rotation, KeyOrientation& out) noexcept {
  const ArxQuat source = math::rotationToQuat(rotation);
  const ArxVector3 forward = math::rotate(source, {0.0f, 0.0f, -1.0f});
  const ArxVector3 right = math::rotate(source, {1.0f, 0.0f, 0.0f});
  constexpr ArxVector3 kDown = {0.0f, -1.0f, 0.0f};
  const ArxVector3 axis = math::cross(forward, kDown);
  const float sine = math::lengthf(axis);
  const float cosine = std::clamp(math::dotf(forward, kDown), -1.0f, 1.0f);
  const float tilt = std::atan2(sine, cosine);
  if (!std::isfinite(tilt)) return false;

  ArxQuat leveling = math::kIdentityQuat;
  if (sine > 1.0e-5f) {
    leveling = math::axisAngle(axis.x / sine, axis.y / sine, axis.z / sine, tilt);
  } else if (cosine < 0.0f) {
    // Opposite direction: keep the camera-right axis fixed
    leveling = math::axisAngle(right.x, right.y, right.z, std::numbers::pi_v<float>);
  }

  const ArxVector3 leveled_right = math::rotate(leveling, right);
  const float horizontal_length = std::hypot(leveled_right.x, leveled_right.z);
  if (!std::isfinite(horizontal_length) || horizontal_length <= 1.0e-5f) return false;
  const float roll = std::atan2(leveled_right.z, leveled_right.x) * math::kDegreesPerRadian;
  if (!std::isfinite(roll)) return false;

  out.roll = roll;
  out.corrected = tilt > 1.0e-5f;
  out.rotation =
      out.corrected ? math::normalize(kContentBasis * toGlbRotation(roll) * math::conjugate(kContentBasis)) : source;
  return true;
}

bool toArxLightPoint(const ArxVector3& local, float vertical_fov, ArxVector3& out) noexcept {
  const float depth = -local.z;
  const float half_height = depth * std::tan(vertical_fov * 0.5f);
  if (!std::isfinite(half_height) || half_height <= 0.0f) return false;
  const float half_width = half_height * (kScreenHalfWidth / kScreenHalfHeight);
  const ArxVector3 result{local.x * kScreenHalfWidth / half_width, -local.y * kScreenHalfHeight / half_height, 0.0f};
  if (!std::isfinite(result.x) || !std::isfinite(result.y)) return false;
  out = result;
  return true;
}

}  // namespace pistoris::glb_cinematic
