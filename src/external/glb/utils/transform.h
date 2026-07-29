// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"

#include "utils/math/mat3.h"
#include "utils/math/mat4.h"

namespace pistoris::glb {

struct DecomposedTransform {
  ArxVector3 translation = {};
  ArxMat3 rotation = math::kIdentityMat3;
  ArxVector3 scale = {1.0f, 1.0f, 1.0f};
};

bool decomposeTransform(const math::Mat4& world, DecomposedTransform& out) noexcept;

}  // namespace pistoris::glb
