// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/route.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/sounds/modules.h"
#include "modules/textures/modules.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "resources/model_input.h"
#include "routes/descriptor.h"
#include "routes/model/invocation.h"
#include "routes/model/operations.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"
#include "routes/model/resolution.h"
#include "routes/model/save.h"
#include "routes/model/state.h"
#include "routes/options.h"
#include "routes/types.h"

#include <cstddef>
#include <memory>
#include <variant>

namespace cli::model {
namespace {

bool isPrimaryCompatible(FileFacts facts) { return isModelInput(facts); }

bool isExtraCompatible(FileFacts facts) {
  if (facts.format == Format::kTea) return true;
  if (facts.format == Format::kJson) return facts.kind == PayloadKind::kTea || facts.kind == PayloadKind::kUnknown;
  return false;
}

std::unique_ptr<RouteOptions> createRouteOptions() { return std::make_unique<ModelOptions>(); }

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

ProbeResult probe(const RouteProbeContext& context, RouteInvocation& route_invocation) {
  Invocation& invocation = static_cast<Invocation&>(route_invocation);
  if (context.inputs.empty()) return {ProbeStatus::kInvalid, "Model route requires an input"};
  if (!isPrimaryCompatible(context.inputs[0].facts)) return {};

  invocation.input = 0;
  invocation.output = context.output;
  for (std::size_t index = 1; index < context.inputs.size(); ++index) {
    if (!isExtraCompatible(context.inputs[index].facts)) {
      return {ProbeStatus::kInvalid, "Model route accepts only TEA extras"};
    }
    invocation.extras.push_back(index);
  }
  return {ProbeStatus::kMatch};
}

bool resolve(const RouteResolveContext& context, RouteInvocation& invocation) {
  return resolveInvocation(context, static_cast<Invocation&>(invocation));
}

int dispatch(const ResolvedModelInvocation& resolved) {
  Invocation& invocation = resolved.invocation;
  if (resolved.common.route().input == Format::kUnknown || invocation.input == kNoClassifiedPath ||
      std::holds_alternative<std::monostate>(invocation.state)) {
    diagnostic(DiagnosticCode::kModelUnsupportedInput, "Unsupported input format");
    return 1;
  }
  if (NativeModelFiles* native = std::get_if<NativeModelFiles>(&invocation.state)) {
    return writeNativeOutput(*native, resolved.common, invocation) ? 0 : 1;
  }
  IntermediateModel* intermediate = std::get_if<IntermediateModel>(&invocation.state);
  if (!intermediate) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "Model input state is unavailable");
    return 1;
  }
  if (!operations::apply(*intermediate, invocation.options, invocation.conversion)) return 1;
  return writeIntermediateOutput(*intermediate, resolved.common, invocation) ? 0 : 1;
}

int execute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedModelInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {
      modules::transform::rotateModule,
      modules::transform::scaleModule,
      modules::transform::offsetModule,
      modules::textures::skipTextureExportModule,
      modules::textures::inputTextureFolderModule,
      modules::transform::rebaseTexturesModule,
      modules::sounds::skipSoundExportModule,
      modules::sounds::inputSoundFolderModule,
      modules::transform::rebaseSoundsModule,
  };
  static constexpr HelpExample kHelpExamples[] = {
      {"--auto-mount model:npc:human_base anim:npc:human_normal_walk human_base.glb",
       "Export a mounted Model and Animation to GLB."},
      {"--auto-mount human_base.glb model:npc:human_base", "Bake an authored GLB into the game resource layout."},
      {"model.obj model.glb", "Convert a loose static Model to GLB."},
  };
  static const RouteDescriptor kDescriptor{
      .kind = RouteKind::kModel,
      .name = "model",
      .primary_input_formats =
          formatBit(Format::kFtl) | formatBit(Format::kObj) | formatBit(Format::kJson) | formatBit(Format::kGlb),
      .extra_input_formats = formatBit(Format::kTea) | formatBit(Format::kJson),
      .output_formats =
          formatBit(Format::kFtl) | formatBit(Format::kObj) | formatBit(Format::kJson) | formatBit(Format::kGlb),
      .supported_modules = kSupportedModules,
      .modules = options::rootModules(),
      .create_options = createRouteOptions,
      .create_invocation = createRouteInvocation,
      .probe = probe,
      .resolve = resolve,
      .execute = execute,
      .help = {.synopsis = "<model> [animations...] <output>",
               .summary = "Convert editable Models and optional Animation sidecars.",
               .examples = kHelpExamples},
  };
  return kDescriptor;
}

}  // namespace cli::model
