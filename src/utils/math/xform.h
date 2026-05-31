// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once
#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

namespace pistoris {

// Euler XYZ intrinsic (deg); M = Rx*Ry*Rz*diag(sx,sy,sz); p' = M*p + t
AffineXform makeAffineXform(float rx_deg, float ry_deg, float rz_deg, float sx, float sy, float sz, float tx, float ty,
                            float tz);

bool isIdentityXform(const AffineXform& x);

// origin vertex skips translation; ARX_INVALID_XFORM if det(M) <= 0
ArxReturnCode applyXformFtl(ftl::Data& d, const AffineXform& x);

// per-bone quat/zoom unchanged (uniform-scale assumption)
ArxReturnCode applyXformTea(tea::Data& d, const AffineXform& x);

}  // namespace pistoris
