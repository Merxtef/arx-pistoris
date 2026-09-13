// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/route.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/sounds/modules.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "routes/animation/invocation.h"
#include "routes/animation/operations.h"
#include "routes/animation/resolution.h"
#include "routes/animation/save.h"
#include "routes/animation/state.h"
#include "routes/descriptor.h"
#include "routes/types.h"

#include <memory>
#include <variant>

namespace cli::animation {
namespace {

bool isPrimaryCompatible(FileFacts facts) {
  if (facts.format == Format::kTea) return true;
  if (facts.format == Format::kJson) {
    return facts.kind == PayloadKind::kTea || facts.kind == PayloadKind::kUnknown;
  }
  return false;
}

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

ProbeResult probe(const RouteProbeContext& context, RouteInvocation& route_invocation) {
  Invocation& invocation = static_cast<Invocation&>(route_invocation);
  if (context.inputs.empty()) return {ProbeStatus::kInvalid, "Animation route requires an input"};
  if (!isPrimaryCompatible(context.inputs[0].facts)) return {};
  if (context.inputs.size() != 1) {
    return {ProbeStatus::kInvalid, "Standalone Animation accepts exactly one input"};
  }

  invocation.output = context.output;
  invocation.input = 0;
  return {ProbeStatus::kMatch};
}

bool resolve(const RouteResolveContext& context, RouteInvocation& invocation) {
  return resolveInvocation(context, static_cast<Invocation&>(invocation));
}

int dispatch(const ResolvedAnimationInvocation& resolved) {
  Invocation& invocation = resolved.invocation;
  if (resolved.common.route().input == Format::kUnknown || invocation.input == kNoClassifiedPath ||
      std::holds_alternative<std::monostate>(invocation.state)) {
    diagnostic(DiagnosticCode::kAnimationUnsupportedInput, "Unsupported input format");
    return 1;
  }
  if (NativeAnimation* native = std::get_if<NativeAnimation>(&invocation.state)) {
    return writeNativeOutput(*native, resolved.common, invocation) ? 0 : 1;
  }
  IntermediateAnimation* intermediate = std::get_if<IntermediateAnimation>(&invocation.state);
  if (!intermediate) {
    diagnostic(DiagnosticCode::kAnimationOutputFailed, "Animation input state is unavailable");
    return 1;
  }
  if (!operations::apply(*intermediate, invocation.conversion)) return 1;
  return writeIntermediateOutput(*intermediate, resolved.common, invocation) ? 0 : 1;
}

int execute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedAnimationInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {
      modules::transform::rotateModule,
      modules::transform::scaleModule,
      modules::sounds::skipSoundExportModule,
      modules::sounds::inputSoundFolderModule,
      modules::transform::rebaseSoundsModule,
  };
  static constexpr HelpExample kHelpExamples[] = {
      {"--auto-mount anim:npc:human_normal_walk human_normal_walk.json", "Export a mounted Animation to JSON."},
      {"human_normal_walk.tea human_normal_walk.json", "Export a loose native Animation to JSON."},
      {"human_normal_walk.json human_normal_walk.tea", "Bake Animation JSON to TEA."},
  };
  static const RouteDescriptor kDescriptor{
      .kind = RouteKind::kAnimation,
      .name = "animation",
      .primary_input_formats = formatBit(Format::kTea) | formatBit(Format::kJson),
      .extra_input_formats = kNoFormats,
      .output_formats = formatBit(Format::kTea) | formatBit(Format::kJson),
      .supported_modules = kSupportedModules,
      .modules = {},
      .create_options = nullptr,
      .create_invocation = createRouteInvocation,
      .probe = probe,
      .resolve = resolve,
      .execute = execute,
      .help = {.synopsis = "<animation> <output>",
               .summary = "Convert standalone native and JSON Animations.",
               .examples = kHelpExamples},
  };
  return kDescriptor;
}

}  // namespace cli::animation
