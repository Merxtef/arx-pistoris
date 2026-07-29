// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/operations.h"

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "conversion/xform.h"
#include "routes/animation/state.h"

#include <cstddef>

namespace cli::animation {

bool applyModules(Context& ctx, const SharedConversionOptions& conversion) {
  if (!conversion.has_xform) return true;

  pistoris::AffineXform xform = cli::makeAffineXform(conversion);
  for (std::size_t index = 0; index < ctx.teas.size(); ++index) {
    ArxReturnCode rc = pistoris::applyTransform(ctx.teas[index], xform);
    if (rc != ARX_OK) {
      cli::diagnostic(cli::DiagnosticCode::kAnimationModuleFailed,
                      "Animation transform failed at family index %zu: %s (code %d)",
                      index,
                      pistoris::errorString(rc),
                      static_cast<int>(rc));
      return false;
    }
  }
  return true;
}

}  // namespace cli::animation
