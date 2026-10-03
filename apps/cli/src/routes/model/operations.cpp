// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/operations.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/model.hpp"

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "conversion/spatial.h"
#include "routes/conversion_failure.h"
#include "routes/model/options.h"
#include "routes/model/state.h"

namespace cli::model::operations {
namespace {

template <class Result>
bool modelFailure(const char* what, const Result& result) {
  return conversionStageFailure(DiagnosticCode::kModelModuleFailed, what, result);
}

}  // namespace

bool apply(IntermediateModel& source, const ModelOptions& options, const SharedConversionOptions& conversion) {
  if (conversion.has_xform) {
    if (conversion.scale != 1.0f) {
      auto result = source.model.scale(conversion.scale);
      if (!result) return modelFailure("Model scale", result);
      for (pistoris::Animation& animation : source.animations) {
        const auto animation_result = animation.scale(conversion.scale);
        if (!animation_result) return modelFailure("Animation scale", animation_result);
      }
    }
    if (conversion.rotate[0] != 0.0f || conversion.rotate[1] != 0.0f || conversion.rotate[2] != 0.0f) {
      const pistoris::ArxQuat rotation = rotationQuaternion(conversion);
      auto result = source.model.rotate(rotation);
      if (!result) return modelFailure("Model rotation", result);
      for (pistoris::Animation& animation : source.animations) {
        const auto animation_result = animation.rotate(rotation);
        if (!animation_result) return modelFailure("Animation rotation", animation_result);
      }
    }
    if (conversion.offset[0] != 0.0f || conversion.offset[1] != 0.0f || conversion.offset[2] != 0.0f) {
      const pistoris::ArxVector3 offset{conversion.offset[0], conversion.offset[1], conversion.offset[2]};
      const auto result = source.model.translate(offset);
      if (!result) return modelFailure("Model translation", result);
    }
  }

  const pistoris::Model::ReferenceOptions& reference_options = options.reference;
  if (reference_options.snap_bone_positions || reference_options.copy_bone_selection_memberships ||
      reference_options.copy_action_point_selections) {
    if (!source.reference) {
      diagnostic(DiagnosticCode::kModelModuleInvalid, "Model reference operations require --ftl-reference");
      return false;
    }
    const auto result = source.model.applyReference(*source.reference, reference_options);
    if (!result) return modelFailure("Model reference", result);
  }

  if (options.infer_bone_selections) {
    const auto result = source.model.inferBoneSelectionMemberships();
    if (!result) return modelFailure("Model bone selection inference", result);
  }

  return true;
}

}  // namespace cli::model::operations
