// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/save.h"

#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "formats/options.h"
#include "resources/animation_output.h"
#include "resources/output.h"
#include "resources/selector.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cli::animation {
namespace {

void outputFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kAnimationOutputFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
}

OutputTarget numberedJsonTarget(const OutputTarget& base, std::size_t index) {
  if (index == 0) return base;

  OutputTarget target = base;
  std::size_t separator = target.path.find_last_of("/\\");
  std::size_t dot = target.path.find_last_of('.');
  if (dot == std::string::npos || (separator != std::string::npos && dot < separator)) dot = target.path.size();
  target.path.insert(dot, std::to_string(index + 1));
  return target;
}

}  // namespace

bool saveOutput(const Context& ctx, const FormatOptions& format, IoService& io, const Invocation& invocation,
                Route route) {
  switch (route.output) {
    case Format::kTea: {
      std::vector<OutputTarget> targets;
      std::string error;
      if (!buildNativeAnimationTargets(ctx.teas, invocation.output, {}, targets, error)) {
        diagnostic(DiagnosticCode::kAnimationOutputFailed, "%s", error.c_str());
        return false;
      }
      bool success = true;
      for (std::size_t index = 0; index < ctx.teas.size(); ++index) {
        std::vector<std::uint8_t> out;
        ArxReturnCode rc = pistoris::writeTea(ctx.teas[index], out);
        if (rc != ARX_OK) {
          outputFailure("TEA output", rc);
          success = false;
          continue;
        }
        if (!writeOutput(io, targets[index], out.data(), out.size())) success = false;
      }
      return success;
    }

    case Format::kJson: {
      bool success = true;
      for (std::size_t index = 0; index < ctx.teas.size(); ++index) {
        std::string out;
        ArxReturnCode rc = pistoris::exportJson(ctx.teas[index], out, format.pretty);
        if (rc != ARX_OK) {
          outputFailure("JSON output", rc);
          success = false;
          continue;
        }
        OutputTarget target = numberedJsonTarget(invocation.output, index);
        if (!writeOutput(io, target, out.data(), out.size())) success = false;
      }
      return success;
    }

    default:
      diagnostic(DiagnosticCode::kAnimationUnsupportedOutput, "Unsupported output format");
      return false;
  }
}

}  // namespace cli::animation
