// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/route.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/sounds/modules.h"
#include "modules/textures/modules.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/operations.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/options/modules.h"
#include "routes/ambiance/resolution.h"
#include "routes/ambiance/save.h"
#include "routes/ambiance/state.h"
#include "routes/descriptor.h"
#include "routes/options.h"
#include "routes/types.h"

#include <memory>
#include <variant>

namespace cli::ambiance {
namespace {

bool isPrimaryCompatible(FileFacts facts) {
  return (facts.format == Format::kAmb && facts.kind == PayloadKind::kAmb) ||
         (facts.format == Format::kJson && facts.kind == PayloadKind::kAmb) ||
         (facts.format == Format::kGlb && facts.kind == PayloadKind::kGlb);
}

std::unique_ptr<RouteOptions> createRouteOptions() { return std::make_unique<AmbianceOptions>(); }

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

ProbeResult probe(const RouteProbeContext& context, RouteInvocation& route_invocation) {
  Invocation& invocation = static_cast<Invocation&>(route_invocation);
  if (context.inputs.empty()) return {ProbeStatus::kInvalid, "Ambiance route requires an input"};
  if (!isPrimaryCompatible(context.inputs.front().facts)) return {};
  if (context.inputs.size() != 1) return {ProbeStatus::kInvalid, "Ambiance route accepts exactly one input"};
  invocation.input = 0;
  invocation.output = context.output;
  return {ProbeStatus::kMatch};
}

bool resolve(const RouteResolveContext& context, RouteInvocation& invocation) {
  return resolveInvocation(context, static_cast<Invocation&>(invocation));
}

int dispatch(const ResolvedAmbianceInvocation& resolved) {
  Invocation& invocation = resolved.invocation;
  if (resolved.common.route().input == Format::kUnknown || invocation.input == kNoClassifiedPath ||
      std::holds_alternative<std::monostate>(invocation.state)) {
    diagnostic(DiagnosticCode::kAmbianceUnsupportedInput, "Unsupported input format");
    return 1;
  }
  if (NativeAmbiance* native = std::get_if<NativeAmbiance>(&invocation.state)) {
    return writeNativeOutput(*native, resolved.common, invocation) ? 0 : 1;
  }
  IntermediateAmbiance* intermediate = std::get_if<IntermediateAmbiance>(&invocation.state);
  if (!intermediate) {
    diagnostic(DiagnosticCode::kAmbianceOutputFailed, "Ambiance input state is unavailable");
    return 1;
  }
  if (!operations::apply(intermediate->ambiance, invocation.options)) return 1;
  return writeIntermediateOutput(*intermediate, resolved.common, invocation) ? 0 : 1;
}

int execute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedAmbianceInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {modules::textures::inputTextureFolderModule,
                                                    modules::sounds::inputSoundFolderModule,
                                                    modules::sounds::skipSoundExportModule,
                                                    modules::transform::rebaseSoundsModule};
  static constexpr HelpExample kHelpExamples[] = {
      {"--auto-mount ambiance:ambient_cave_a ambient_cave_a.glb", "Export a mounted Ambiance to GLB."},
      {"--auto-mount ambient_cave_a.glb ambiance:ambient_cave_a",
       "Bake an authored GLB into the game resource layout."},
      {"ambient_cave_a.amb ambient_cave_a.json", "Export a loose native Ambiance to JSON."},
  };
  static const RouteDescriptor kDescriptor{
      .kind = RouteKind::kAmbiance,
      .name = "ambiance",
      .primary_input_formats = formatBit(Format::kAmb) | formatBit(Format::kJson) | formatBit(Format::kGlb),
      .extra_input_formats = kNoFormats,
      .output_formats = formatBit(Format::kAmb) | formatBit(Format::kJson) | formatBit(Format::kGlb),
      .supported_modules = kSupportedModules,
      .modules = options::rootModules(),
      .create_options = createRouteOptions,
      .create_invocation = createRouteInvocation,
      .probe = probe,
      .resolve = resolve,
      .execute = execute,
      .help = {.synopsis = "<ambiance> <output>",
               .summary = "Convert native, JSON, and editable GLB Ambiances.",
               .examples = kHelpExamples},
  };
  return kDescriptor;
}

}  // namespace cli::ambiance
