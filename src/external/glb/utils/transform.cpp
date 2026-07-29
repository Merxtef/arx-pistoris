// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "transform.h"

#include "arx_pistoris/arx_math.hpp"

#include "utils/math/mat3.h"
#include "utils/math/mat4.h"

#include <cmath>

namespace pistoris::glb {

bool decomposeTransform(const math::Mat4& world, DecomposedTransform& out) noexcept {
  constexpr float kTolerance = 1.0e-4f;
  ArxVector3 columns[3] = {
      {world(0, 0), world(1, 0), world(2, 0)},
      {world(0, 1), world(1, 1), world(2, 1)},
      {world(0, 2), world(1, 2), world(2, 2)},
  };
  out.scale = {math::lengthf(columns[0]), math::lengthf(columns[1]), math::lengthf(columns[2])};
  if (!std::isfinite(out.scale.x) || !std::isfinite(out.scale.y) || !std::isfinite(out.scale.z) ||
      out.scale.x <= 0.0f || out.scale.y <= 0.0f || out.scale.z <= 0.0f)
    return false;
  if (std::abs(math::dotf(columns[0], columns[1])) > kTolerance * out.scale.x * out.scale.y ||
      std::abs(math::dotf(columns[0], columns[2])) > kTolerance * out.scale.x * out.scale.z ||
      std::abs(math::dotf(columns[1], columns[2])) > kTolerance * out.scale.y * out.scale.z)
    return false;
  for (int row = 0; row < 3; ++row) {
    out.rotation(row, 0) = world(row, 0) / out.scale.x;
    out.rotation(row, 1) = world(row, 1) / out.scale.y;
    out.rotation(row, 2) = world(row, 2) / out.scale.z;
  }
  if (math::determinant(out.rotation) <= 0.0f) return false;
  out.translation = math::translation(world);
  return std::isfinite(out.translation.x) && std::isfinite(out.translation.y) && std::isfinite(out.translation.z);
}

}  // namespace pistoris::glb
