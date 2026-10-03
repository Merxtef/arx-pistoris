// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/operations.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/math.hpp"

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "conversion/spatial.h"
#include "routes/animation/state.h"
#include "routes/conversion_failure.h"

namespace cli::animation::operations {
namespace {

template <class Result>
bool animationFailure(const char* what, const Result& result) {
  return conversionStageFailure(DiagnosticCode::kAnimationModuleFailed, what, result);
}

}  // namespace

bool apply(IntermediateAnimation& animation, const SharedConversionOptions& conversion) {
  if (!conversion.has_xform) return true;
  if (conversion.scale != 1.0f) {
    const auto result = animation.animation.scale(conversion.scale);
    if (!result) return animationFailure("Animation scale", result);
  }
  if (conversion.rotate[0] != 0.0f || conversion.rotate[1] != 0.0f || conversion.rotate[2] != 0.0f) {
    const pistoris::ArxQuat rotation = rotationQuaternion(conversion);
    const auto result = animation.animation.rotate(rotation);
    if (!result) return animationFailure("Animation rotation", result);
  }
  return true;
}

}  // namespace cli::animation::operations
