// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/route.h"

#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "resources/selector.h"
#include "routes/animation/invocation.h"
#include "routes/animation/load.h"
#include "routes/animation/operations.h"
#include "routes/animation/save.h"
#include "routes/animation/state.h"
#include "routes/descriptor.h"
#include "routes/types.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace cli::animation {
namespace {

bool supportsOutput(cli::Format output) {
  switch (output) {
    case cli::Format::kTea:
    case cli::Format::kJson:
      return true;
    default:
      return false;
  }
}

bool isPrimaryCompatible(cli::FileFacts facts) {
  if (facts.format == cli::Format::kTea) return true;
  if (facts.format == cli::Format::kJson) {
    return facts.kind == cli::PayloadKind::kTea || facts.kind == cli::PayloadKind::kUnknown;
  }
  return false;
}

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

int executeRoute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedAnimationInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

cli::ProbeResult probe(const std::vector<cli::ClassifiedPath>& inputs, const OutputTarget& output,
                       cli::Format output_format, Invocation& inv) {
  if (!supportsOutput(output_format)) return {};
  if (inputs.empty()) return {cli::ProbeStatus::kInvalid, "Animation route requires an input"};
  if (!isPrimaryCompatible(inputs[0].facts)) return {};
  if (inputs.size() > 1) {
    const std::size_t positional = inputs[0].positional_index;
    for (const ClassifiedPath& input : inputs) {
      if (input.resource_kind != ARX_RESOURCE_KIND_ANIMATION || input.positional_index != positional) {
        return {cli::ProbeStatus::kInvalid, "Standalone Animation accepts one raw input or one anim: selector"};
      }
    }
  }

  inv.output = output;
  inv.inputs.reserve(inputs.size());
  for (std::size_t index = 0; index < inputs.size(); ++index) inv.inputs.push_back(index);
  return {cli::ProbeStatus::kMatch};
}

cli::ProbeResult probeRoute(const cli::RouteProbeContext& ctx, RouteInvocation& invocation) {
  return probe(ctx.inputs, ctx.output, ctx.output_format, static_cast<Invocation&>(invocation));
}

bool resolveRoute(const RouteResolveContext& context, RouteInvocation& invocation) {
  Invocation& animation = static_cast<Invocation&>(invocation);
  animation.conversion = context.options.conversion;
  animation.format = context.options.format;
  return true;
}

bool printHelpSection(std::FILE*, cli::HelpSection) { return false; }

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {cli::modules::transform::rotateModule};
  static const RouteDescriptor kDescriptor{
      RouteKind::kAnimation,
      "animation",
      formatBit(Format::kTea) | formatBit(Format::kJson),
      kAllFormats,
      formatBit(Format::kTea) | formatBit(Format::kJson),
      kSupportedModules,
      {},
      nullptr,
      createRouteInvocation,
      probeRoute,
      resolveRoute,
      executeRoute,
      printHelpSection,
  };
  return kDescriptor;
}

int dispatch(const ResolvedAnimationInvocation& inv) {
  Invocation& invocation = inv.invocation;
  if (inv.common.route().input == cli::Format::kUnknown || invocation.inputs.empty()) {
    cli::diagnostic(cli::DiagnosticCode::kAnimationUnsupportedInput, "Unsupported input format");
    return 1;
  }
  Context ctx;
  if (!loadInput(inv.common.inputs(), invocation, inv.common.route(), ctx)) return 1;

  if (!applyModules(ctx, invocation.conversion)) return 1;

  return saveOutput(ctx, invocation.format, inv.common.io(), invocation, inv.common.route()) ? 0 : 1;
}

}  // namespace cli::animation
