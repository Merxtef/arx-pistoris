// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/route.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "resources/selector.h"
#include "routes/descriptor.h"
#include "routes/model/invocation.h"
#include "routes/model/load.h"
#include "routes/model/operations.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"
#include "routes/model/save.h"
#include "routes/model/state.h"
#include "routes/options.h"
#include "routes/types.h"

#include <cstdio>
#include <memory>
#include <vector>

namespace cli::model {
namespace {

bool supportsOutput(Format output) {
  switch (output) {
    case Format::kFtl:
    case Format::kObj:
    case Format::kJson:
    case Format::kGlb:
      return true;
    default:
      return false;
  }
}

bool isPrimaryCompatible(FileFacts facts) {
  switch (facts.format) {
    case Format::kFtl:
    case Format::kObj:
    case Format::kGlb:
      return facts.kind != PayloadKind::kTea;
    case Format::kJson:
      return facts.kind == PayloadKind::kFtl || facts.kind == PayloadKind::kUnknown;
    default:
      return false;
  }
}

bool isExtraCompatible(FileFacts facts) {
  if (facts.format == Format::kTea) return true;
  if (facts.format == Format::kJson) return facts.kind == PayloadKind::kTea || facts.kind == PayloadKind::kUnknown;
  return false;
}

std::unique_ptr<RouteOptions> createRouteOptions() { return std::make_unique<ModelOptions>(); }

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

int executeRoute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedModelInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

ProbeResult probe(const std::vector<ClassifiedPath>& inputs, const OutputTarget& output, Format output_format,
                  Invocation& inv) {
  if (!supportsOutput(output_format)) return {};
  if (inputs.empty()) return {ProbeStatus::kInvalid, "Model route requires an input"};
  if (!isPrimaryCompatible(inputs[0].facts)) return {};

  inv.input = 0;
  inv.output = output;
  for (std::size_t i = 1; i < inputs.size(); ++i) {
    if (!isExtraCompatible(inputs[i].facts)) return {ProbeStatus::kInvalid, "Model route accepts only TEA extras"};
    inv.extras.push_back(i);
  }
  return {ProbeStatus::kMatch};
}

ProbeResult probeRoute(const RouteProbeContext& ctx, RouteInvocation& invocation) {
  return probe(ctx.inputs, ctx.output, ctx.output_format, static_cast<Invocation&>(invocation));
}

bool resolveRoute(const RouteResolveContext& context, RouteInvocation& invocation) {
  Invocation& model = static_cast<Invocation&>(invocation);
  if (context.route_options) model.options = static_cast<const ModelOptions&>(*context.route_options);
  model.conversion = context.options.conversion;
  model.format = context.options.format;
  return true;
}

bool printHelpSection(std::FILE* output, HelpSection section) {
  if (section != HelpSection::kConventions) return false;
  std::fprintf(output, "  Model reference repair modules require --ftl-reference.\n");
  return true;
}

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {
      modules::transform::rotateModule,
      modules::transform::scaleModule,
      modules::transform::offsetModule,
  };
  static const RouteDescriptor kDescriptor{
      RouteKind::kModel,
      "model",
      formatBit(Format::kFtl) | formatBit(Format::kObj) | formatBit(Format::kJson) | formatBit(Format::kGlb),
      formatBit(Format::kTea) | formatBit(Format::kJson),
      formatBit(Format::kFtl) | formatBit(Format::kObj) | formatBit(Format::kJson) | formatBit(Format::kGlb),
      kSupportedModules,
      options::rootModules(),
      createRouteOptions,
      createRouteInvocation,
      probeRoute,
      resolveRoute,
      executeRoute,
      printHelpSection,
  };
  return kDescriptor;
}

int dispatch(const ResolvedModelInvocation& inv) {
  Invocation& invocation = inv.invocation;
  if (inv.common.route().input == Format::kUnknown || invocation.input == kNoClassifiedPath) {
    diagnostic(DiagnosticCode::kModelUnsupportedInput, "Unsupported input format");
    return 1;
  }

  Context ctx;
  if (!loadInput(inv.common.inputs(), invocation, inv.common.route(), ctx)) return 1;
  if (!loadExtras(inv.common.inputs(), invocation, ctx)) return 1;
  if (!validateTeaCompatibility(ctx)) return 1;
  if (invocation.options.reference_ftl && !loadReferenceFtl(invocation.options.reference_ftl, ctx)) return 1;
  if (!applyModules(ctx, invocation.options, invocation.conversion)) return 1;
  return saveOutput(ctx, invocation.format, inv.common.io(), invocation, inv.common.route()) ? 0 : 1;
}

}  // namespace cli::model
