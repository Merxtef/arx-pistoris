// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/operations.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "conversion/spatial.h"
#include "routes/animation/state.h"

namespace cli::animation::operations {
namespace {

bool animationFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kAnimationModuleFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

}  // namespace

bool apply(IntermediateAnimation& animation, const SharedConversionOptions& conversion) {
  if (!conversion.has_xform) return true;
  if (conversion.scale != 1.0f) {
    const ArxReturnCode rc = animation.animation.scale(conversion.scale);
    if (rc != ARX_OK) return animationFailure("Animation scale", rc);
  }
  if (conversion.rotate[0] != 0.0f || conversion.rotate[1] != 0.0f || conversion.rotate[2] != 0.0f) {
    const pistoris::ArxQuat rotation = rotationQuaternion(conversion);
    const ArxReturnCode rc = animation.animation.rotate(rotation);
    if (rc != ARX_OK) return animationFailure("Animation rotation", rc);
  }
  return true;
}

}  // namespace cli::animation::operations
