// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/operations.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "conversion/spatial.h"
#include "routes/model/options.h"
#include "routes/model/state.h"

#include <memory>

namespace cli::model::operations {
namespace {

bool modelFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kModelModuleFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

}  // namespace

bool apply(IntermediateModel& source, const ModelOptions& options, const SharedConversionOptions& conversion) {
  if (conversion.has_xform) {
    if (conversion.scale != 1.0f) {
      ArxReturnCode rc = source.model.scale(conversion.scale);
      if (rc != ARX_OK) return modelFailure("Model scale", rc);
      for (const std::unique_ptr<pistoris::Animation>& animation : source.animations) {
        rc = animation->scale(conversion.scale);
        if (rc != ARX_OK) return modelFailure("Animation scale", rc);
      }
    }
    if (conversion.rotate[0] != 0.0f || conversion.rotate[1] != 0.0f || conversion.rotate[2] != 0.0f) {
      const pistoris::ArxQuat rotation = rotationQuaternion(conversion);
      ArxReturnCode rc = source.model.rotate(rotation);
      if (rc != ARX_OK) return modelFailure("Model rotation", rc);
      for (const std::unique_ptr<pistoris::Animation>& animation : source.animations) {
        rc = animation->rotate(rotation);
        if (rc != ARX_OK) return modelFailure("Animation rotation", rc);
      }
    }
    if (conversion.offset[0] != 0.0f || conversion.offset[1] != 0.0f || conversion.offset[2] != 0.0f) {
      const pistoris::ArxVector3 offset{conversion.offset[0], conversion.offset[1], conversion.offset[2]};
      const ArxReturnCode rc = source.model.translate(offset);
      if (rc != ARX_OK) return modelFailure("Model translation", rc);
    }
  }

  const pistoris::Model::ReferenceOptions& reference_options = options.reference;
  if (reference_options.snap_bone_origins || reference_options.copy_bone_origin_selections ||
      reference_options.copy_action_point_selections) {
    if (!source.reference) {
      diagnostic(DiagnosticCode::kModelModuleInvalid, "Model reference operations require --ftl-reference");
      return false;
    }
    const ArxReturnCode rc = source.model.applyReference(*source.reference, reference_options);
    if (rc != ARX_OK) return modelFailure("Model reference", rc);
  }

  if (options.infer_bone_selections) {
    const ArxReturnCode rc = source.model.inferBoneOriginSelections();
    if (rc != ARX_OK) return modelFailure("Model bone-origin selection inference", rc);
  }

  return true;
}

}  // namespace cli::model::operations
