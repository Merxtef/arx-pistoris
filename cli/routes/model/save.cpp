// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/save.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/format.h"
#include "formats/options.h"
#include "io/paths.h"
#include "resources/animation_output.h"
#include "resources/output.h"
#include "resources/selector.h"
#include "routes/model/invocation.h"
#include "routes/model/state.h"
#include "routes/types.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli::model {
namespace {

bool outputFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kModelOutputFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

enum class TeaOutputFormat : std::uint8_t {
  kNative,
  kJson,
};

bool writeTeas(IoService& io, const Context& ctx, const FormatOptions& options, std::span<const OutputTarget> targets,
               TeaOutputFormat tea_format) {
  bool ok = true;
  for (std::size_t index = 0; index < ctx.teas.size(); ++index) {
    const OutputTarget& target = targets[index];

    if (tea_format == TeaOutputFormat::kNative) {
      std::vector<std::uint8_t> tea_data;
      ArxReturnCode rc = pistoris::writeTea(ctx.teas[index], tea_data);
      if (rc != ARX_OK) {
        diagnostic(DiagnosticCode::kModelOutputFailed,
                   "TEA write failed (%s): %s (code %d)",
                   target.path.c_str(),
                   pistoris::errorString(rc),
                   static_cast<int>(rc));
        ok = false;
        continue;
      }
      if (!writeOutput(io, target, tea_data.data(), tea_data.size())) {
        ok = false;
        continue;
      }
    } else {
      std::string tea_json;
      ArxReturnCode rc = pistoris::exportJson(ctx.teas[index], tea_json, options.pretty);
      if (rc != ARX_OK) {
        diagnostic(DiagnosticCode::kModelOutputFailed,
                   "TEA JSON output failed (%s): %s (code %d)",
                   target.path.c_str(),
                   pistoris::errorString(rc),
                   static_cast<int>(rc));
        ok = false;
        continue;
      }
      if (!writeOutput(io, target, tea_json.data(), tea_json.size())) {
        ok = false;
        continue;
      }
    }
  }
  return ok;
}

void warnTeasIgnored(const Context& ctx, const char* output_format) {
  if (!ctx.teas.empty()) log(ARX_LOG_WARN, "%s output ignores %zu TEA animation(s)", output_format, ctx.teas.size());
}

}  // namespace

bool saveOutput(const Context& ctx, const FormatOptions& format, IoService& io, const Invocation& invocation,
                Route route) {
  switch (route.output) {
    case Format::kFtl: {
      std::string animation_directory;
      if (invocation.output.selector.kind != ARX_RESOURCE_KIND_NONE) {
        if (!pistoris::paths::animationDirectory(invocation.output.selector.type, animation_directory)) {
          diagnostic(DiagnosticCode::kModelOutputFailed,
                     "invalid Model resource type '%s'",
                     invocation.output.selector.type.c_str());
          return false;
        }
      }
      std::vector<OutputTarget> animation_targets;
      std::string target_error;
      if (!buildAnimationTargets(
              ctx.teas, invocation.output, animation_directory, Format::kTea, {}, animation_targets, target_error)) {
        diagnostic(DiagnosticCode::kModelOutputFailed, "%s", target_error.c_str());
        return false;
      }

      std::vector<std::uint8_t> out;
      ArxReturnCode rc = pistoris::writeFtl(ctx.ftl, out, format.compress);
      if (rc != ARX_OK) return outputFailure("FTL output", rc);
      if (!writeOutput(io, invocation.output, out.data(), out.size())) return false;

      if (!ctx.teas.empty() && !writeTeas(io, ctx, format, animation_targets, TeaOutputFormat::kNative)) {
        return false;
      }
      return true;
    }

    case Format::kObj: {
      warnTeasIgnored(ctx, "OBJ");

      const char* output_path = invocation.output.path.c_str();
      std::string_view object_stem(output_path, fileExtension(output_path) - output_path);
      auto separator = object_stem.find_last_of("/\\");
      if (separator != std::string_view::npos) object_stem = object_stem.substr(separator + 1);

      pistoris::Obj object;
      ArxReturnCode rc = pistoris::exportObj(ctx.ftl, object_stem, object);
      if (rc != ARX_OK) return outputFailure("OBJ output", rc);

      if (!writeOutput(io, invocation.output, object.text.data(), object.text.size())) return false;

      if (!object.mtl.empty()) {
        std::string mtl_path(output_path, fileExtension(output_path));
        mtl_path += ".mtl";
        OutputTarget mtl_target;
        mtl_target.path = std::move(mtl_path);
        mtl_target.address = invocation.output.address;
        mtl_target.format = Format::kUnknown;
        if (!writeOutput(io, mtl_target, object.mtl.data(), object.mtl.size())) return false;
      }
      return true;
    }

    case Format::kJson: {
      std::vector<OutputTarget> animation_targets;
      std::string target_error;
      const std::span<const OutputTarget> reserved(&invocation.output, 1);
      if (!buildAnimationTargets(
              ctx.teas, invocation.output, {}, Format::kJson, reserved, animation_targets, target_error)) {
        diagnostic(DiagnosticCode::kModelOutputFailed, "%s", target_error.c_str());
        return false;
      }

      std::string out;
      ArxReturnCode rc = pistoris::exportJson(ctx.ftl, out, format.pretty);
      if (rc != ARX_OK) return outputFailure("JSON output", rc);
      if (!writeOutput(io, invocation.output, out.data(), out.size())) return false;
      if (!ctx.teas.empty() && !writeTeas(io, ctx, format, animation_targets, TeaOutputFormat::kJson)) return false;
      return true;
    }

    case Format::kGlb: {
      std::vector<std::uint8_t> out;
      ArxReturnCode rc = pistoris::exportGlb(ctx.ftl, ctx.teas, out);
      if (rc != ARX_OK) return outputFailure("GLB output", rc);
      return writeOutput(io, invocation.output, out.data(), out.size());
    }

    case Format::kUnset:
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedOutput, "Unsupported output format");
      return false;
  }
}

}  // namespace cli::model
