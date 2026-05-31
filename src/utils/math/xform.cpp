// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/math/xform.h"

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "arx/ftl.h"
#include "arx/tea.h"
#include "utils/math/mat3.h"
#include "utils/math/quat.h"
#include "utils/math/vec3.h"

#include <cstddef>
#include <numbers>

namespace pistoris {

static constexpr float kPi = std::numbers::pi_v<float>;

AffineXform makeAffineXform(float rx_deg, float ry_deg, float rz_deg, float sx, float sy, float sz, float tx, float ty,
                            float tz) {
  float ax  = rx_deg * kPi / 180.0f;
  float ay  = ry_deg * kPi / 180.0f;
  float az  = rz_deg * kPi / 180.0f;
  ArxMat3 r = math::fromEulerXYZ(ax, ay, az);
  return AffineXform{math::scaleColumns(r, {sx, sy, sz}), {tx, ty, tz}};
}

bool isIdentityXform(const AffineXform& x) {
  for (int row = 0; row < 3; ++row)
    for (int col = 0; col < 3; ++col)
      if (x.linear(row, col) != (row == col ? 1.0f : 0.0f)) return false;
  return x.translation.x == 0.0f && x.translation.y == 0.0f && x.translation.z == 0.0f;
}

ArxReturnCode applyXformFtl(ftl::Data& d, const AffineXform& x) {
  if (math::determinant(x.linear) <= 0.0f) return ARX_INVALID_XFORM;

  ArxMat3 mit = math::inverseTranspose(x.linear);

  for (size_t vi = 0; vi < d.vertices.size(); ++vi) {
    auto& v = d.vertices[vi];
    if (vi == d.header.origin)
      v.position = x.linear * v.position;
    else
      v.position = x.linear * v.position + x.translation;
    v.normal = math::normalize(mit * v.normal);
  }
  for (auto& f : d.faces) f.norm = math::normalize(mit * f.norm);

  return validateFtl(&d);
}

ArxReturnCode applyXformTea(tea::Data& d, const AffineXform& x) {
  if (math::determinant(x.linear) <= 0.0f) return ARX_INVALID_XFORM;

  ArxQuat r_quat = math::extractRotation(x.linear);
  ArxQuat r_inv  = math::conjugate(r_quat);

  for (auto& kf : d.keyframes) {
    if (kf.translate) *kf.translate = x.linear * *kf.translate + x.translation;
    if (kf.quat) *kf.quat = math::normalize(r_quat * *kf.quat * r_inv);
    for (auto& ga : kf.groups) {
      ga.quat      = math::normalize(r_quat * ga.quat * r_inv);
      ga.translate = x.linear * ga.translate;
    }
  }

  return validateTea(&d);
}

}  // namespace pistoris
