// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.hpp"

#include "utils/math/mat3.h"
#include "utils/math/mat4.h"

namespace pistoris::glb {

struct DecomposedTransform {
  ArxVector3 translation = {};
  ArxMat3 rotation = math::kIdentityMat3;
  ArxVector3 scale = {1.0f, 1.0f, 1.0f};
};

bool isAffineTransform(const math::Mat4& transform) noexcept;
bool canonicalizeAffineTransform(math::Mat4& transform) noexcept;
bool decomposeTransform(const math::Mat4& world, DecomposedTransform& out) noexcept;

}  // namespace pistoris::glb
