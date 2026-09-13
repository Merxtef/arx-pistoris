// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "conversion/spatial.h"

#include "arx_pistoris/base/math.h"

#include "conversion/options.h"

#include <cmath>
#include <numbers>

namespace cli {

pistoris::ArxQuat rotationQuaternion(const SharedConversionOptions& options) noexcept {
  constexpr double kDegreesToHalfRadians = std::numbers::pi / 360.0;
  const double x = static_cast<double>(options.rotate[0]) * kDegreesToHalfRadians;
  const double y = static_cast<double>(options.rotate[1]) * kDegreesToHalfRadians;
  const double z = static_cast<double>(options.rotate[2]) * kDegreesToHalfRadians;
  const double cx = std::cos(x);
  const double sx = std::sin(x);
  const double cy = std::cos(y);
  const double sy = std::sin(y);
  const double cz = std::cos(z);
  const double sz = std::sin(z);
  return {
      static_cast<float>(cx * cy * cz - sx * sy * sz),
      static_cast<float>(sx * cy * cz + cx * sy * sz),
      static_cast<float>(cx * sy * cz - sx * cy * sz),
      static_cast<float>(cx * cy * sz + sx * sy * cz),
  };
}

}  // namespace cli
