// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/cinematic/route.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/sounds/modules.h"
#include "modules/textures/modules.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/options.h"
#include "routes/cinematic/options/modules.h"
#include "routes/cinematic/resolution.h"
#include "routes/cinematic/save.h"
#include "routes/cinematic/state.h"
#include "routes/descriptor.h"
#include "routes/options.h"
#include "routes/types.h"

#include <memory>
#include <variant>

namespace cli::cinematic {
namespace {

bool isPrimaryCompatible(FileFacts facts) {
  return (facts.format == Format::kCin && facts.kind == PayloadKind::kCin) ||
         (facts.format == Format::kGlb && facts.kind == PayloadKind::kGlb);
}

std::unique_ptr<RouteOptions> createRouteOptions() { return std::make_unique<CinematicOptions>(); }

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

ProbeResult probe(const RouteProbeContext& context, RouteInvocation& route_invocation) {
  Invocation& invocation = static_cast<Invocation&>(route_invocation);
  if (context.inputs.empty()) return {ProbeStatus::kInvalid, "Cinematic route requires an input"};
  if (!isPrimaryCompatible(context.inputs.front().facts)) return {};
  if (context.inputs.size() != 1) return {ProbeStatus::kInvalid, "Cinematic route accepts exactly one input"};
  invocation.input = 0;
  invocation.output = context.output;
  return {ProbeStatus::kMatch};
}

bool resolve(const RouteResolveContext& context, RouteInvocation& invocation) {
  return resolveInvocation(context, static_cast<Invocation&>(invocation));
}

int execute(const ExecutionContext& context, RouteInvocation& route_invocation) {
  Invocation& invocation = static_cast<Invocation&>(route_invocation);
  if (context.route().input == Format::kUnknown || invocation.input == kNoClassifiedPath ||
      std::holds_alternative<std::monostate>(invocation.state)) {
    diagnostic(DiagnosticCode::kCinematicUnsupportedInput, "Unsupported Cinematic input format");
    return 1;
  }
  if (NativeCinematic* native = std::get_if<NativeCinematic>(&invocation.state))
    return writeNativeOutput(*native, context, invocation) ? 0 : 1;
  IntermediateCinematic* intermediate = std::get_if<IntermediateCinematic>(&invocation.state);
  if (!intermediate) {
    diagnostic(DiagnosticCode::kCinematicOutputFailed, "Cinematic input state is unavailable");
    return 1;
  }
  return writeIntermediateOutput(*intermediate, context, invocation) ? 0 : 1;
}

}  // namespace

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {modules::textures::inputTextureFolderModule,
                                                    modules::textures::skipTextureExportModule,
                                                    modules::sounds::inputSoundFolderModule,
                                                    modules::sounds::skipSoundExportModule,
                                                    modules::transform::rebaseTexturesModule,
                                                    modules::transform::rebaseSoundsModule};
  static constexpr HelpExample kHelpExamples[] = {
      {"--auto-mount cinematic:intro intro.glb", "Export a mounted Cinematic to editable GLB."},
      {"--auto-mount intro.glb cinematic:intro", "Bake an authored GLB into the game resource layout."},
      {"intro.cin intro.glb", "Export a game-layout CIN to GLB."},
  };
  static const RouteDescriptor kDescriptor{
      .kind = RouteKind::kCinematic,
      .name = "cinematic",
      .primary_input_formats = formatBit(Format::kCin) | formatBit(Format::kGlb),
      .extra_input_formats = kNoFormats,
      .output_formats = formatBit(Format::kCin) | formatBit(Format::kGlb),
      .supported_modules = kSupportedModules,
      .modules = options::rootModules(),
      .create_options = createRouteOptions,
      .create_invocation = createRouteInvocation,
      .probe = probe,
      .resolve = resolve,
      .execute = execute,
      .help = {.synopsis = "<cinematic> <output>",
               .summary = "Convert native CIN and editable GLB Cinematics.",
               .examples = kHelpExamples},
  };
  return kDescriptor;
}

}  // namespace cli::cinematic
