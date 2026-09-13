// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "utils/math/finite.h"

#include <cmath>

namespace pistoris::math {

inline constexpr double kRotationMinimumLength = 1.0e-6;
inline constexpr double kRotationUnitTolerance = 1.0e-4;

inline ArxVector3 rotateHalfTurnX(const ArxVector3& value) noexcept { return {value.x, -value.y, -value.z}; }

inline ArxVector3 rotateAroundY(const ArxVector3& value, float radians) noexcept {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {cosine * value.x + sine * value.z, value.y, -sine * value.x + cosine * value.z};
}

inline double rotationLengthSquared(const ArxQuat& rotation) noexcept {
  return static_cast<double>(rotation.w) * rotation.w + static_cast<double>(rotation.x) * rotation.x +
         static_cast<double>(rotation.y) * rotation.y + static_cast<double>(rotation.z) * rotation.z;
}

inline bool normalizeRotation(ArxQuat& rotation) noexcept {
  if (!finite(rotation)) return false;
  const double length_squared = rotationLengthSquared(rotation);
  if (!std::isfinite(length_squared) || length_squared <= kRotationMinimumLength * kRotationMinimumLength) return false;
  const double inverse_length = 1.0 / std::sqrt(length_squared);
  rotation = {
      static_cast<float>(static_cast<double>(rotation.w) * inverse_length),
      static_cast<float>(static_cast<double>(rotation.x) * inverse_length),
      static_cast<float>(static_cast<double>(rotation.y) * inverse_length),
      static_cast<float>(static_cast<double>(rotation.z) * inverse_length),
  };
  return finite(rotation);
}

inline bool validRotation(const ArxQuat& rotation) noexcept {
  if (!finite(rotation)) return false;
  const double length_squared = rotationLengthSquared(rotation);
  return std::isfinite(length_squared) && std::abs(std::sqrt(length_squared) - 1.0) <= kRotationUnitTolerance;
}

}  // namespace pistoris::math
