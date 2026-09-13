// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/route.h"

#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/textures/modules.h"
#include "modules/transform/modules.h"
#include "pipeline/execution_context.h"
#include "routes/descriptor.h"
#include "routes/level/invocation.h"
#include "routes/level/operations.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"
#include "routes/level/resolution.h"
#include "routes/level/save.h"
#include "routes/level/state.h"
#include "routes/options.h"
#include "routes/types.h"

#include <cstddef>
#include <memory>
#include <variant>

namespace cli::level {
namespace {

bool isPrimaryCompatible(FileFacts facts) {
  return facts.format == Format::kFts || facts.format == Format::kDlf || facts.format == Format::kGlb ||
         (facts.format == Format::kJson && facts.kind == PayloadKind::kFts);
}

std::unique_ptr<RouteOptions> createRouteOptions() { return std::make_unique<LevelOptions>(); }

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

ProbeResult probe(const RouteProbeContext& context, RouteInvocation& route_invocation) {
  Invocation& invocation = static_cast<Invocation&>(route_invocation);
  if (context.inputs.empty()) return {ProbeStatus::kInvalid, "Level route requires an input"};
  if (!isPrimaryCompatible(context.inputs[0].facts)) return {};

  invocation.input = 0;
  invocation.output = context.output;
  const bool primary_fts = context.inputs[0].facts.kind == PayloadKind::kFts;
  const bool primary_json = context.inputs[0].facts.format == Format::kJson;
  const bool primary_glb = context.inputs[0].facts.format == Format::kGlb;
  const bool primary_dlf = context.inputs[0].facts.format == Format::kDlf;
  if (primary_dlf && context.inputs.size() > 1) {
    return {ProbeStatus::kInvalid, "DLF Level input does not accept explicit extra inputs"};
  }
  if (context.inputs[0].resource_kind == ARX_RESOURCE_KIND_LEVEL) {
    const std::size_t positional = context.inputs[0].positional_index;
    for (const ClassifiedPath& input : context.inputs) {
      if (input.resource_kind != ARX_RESOURCE_KIND_LEVEL || input.positional_index != positional) {
        return {ProbeStatus::kInvalid, "level: input cannot be combined with other Level inputs"};
      }
    }
  }
  for (std::size_t index = 1; index < context.inputs.size(); ++index) {
    const Format format = context.inputs[index].facts.format;
    const bool native_llf_extra =
        primary_fts &&
        ((primary_json && format == Format::kJson && context.inputs[index].facts.kind == PayloadKind::kLlf) ||
         (!primary_json && format == Format::kLlf));
    const bool native_dlf_extra =
        primary_fts &&
        ((primary_json && format == Format::kJson && context.inputs[index].facts.kind == PayloadKind::kDlf) ||
         (!primary_json && format == Format::kDlf));
    if (native_llf_extra) {
      if (invocation.llf != kNoClassifiedPath) {
        return {ProbeStatus::kInvalid, "Level route accepts at most one LLF input"};
      }
      invocation.llf = index;
      continue;
    }
    if (native_dlf_extra) {
      if (invocation.dlf != kNoClassifiedPath) {
        return {ProbeStatus::kInvalid, "Level route accepts at most one DLF input"};
      }
      invocation.dlf = index;
      continue;
    }
    if (primary_glb && (format == Format::kLlf || format == Format::kDlf)) {
      return {ProbeStatus::kInvalid, "Level GLB input does not accept native extras"};
    }
    return {ProbeStatus::kInvalid, "Level route accepts only LLF/DLF extras for native FTS input"};
  }
  return {ProbeStatus::kMatch};
}

bool resolve(const RouteResolveContext& context, RouteInvocation& invocation) {
  return resolveInvocation(context, static_cast<Invocation&>(invocation));
}

int dispatch(const ResolvedLevelInvocation& resolved) {
  if (resolved.common.route().input == Format::kUnknown || resolved.invocation.input == kNoClassifiedPath ||
      std::holds_alternative<std::monostate>(resolved.invocation.state)) {
    diagnostic(DiagnosticCode::kLevelUnsupportedInput, "Unsupported Level input format");
    return 1;
  }
  const OutputConverterDescriptor* converter = resolved.invocation.output_converter;
  if (!converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Unsupported Level output converter");
    return 1;
  }
  if (NativeLevelFiles* files = std::get_if<NativeLevelFiles>(&resolved.invocation.state)) {
    return writeNativeOutput(*files, resolved.common, resolved.invocation) ? 0 : 1;
  }

  IntermediateLevel* level = std::get_if<IntermediateLevel>(&resolved.invocation.state);
  if (!level) {
    diagnostic(DiagnosticCode::kLevelOutputFailed, "Level input state is unavailable");
    return 1;
  }

  operations::OperationDiagnostics diagnostics;
  if (converter->create_diagnostics) diagnostics = converter->create_diagnostics();
  if (!operations::apply(level->level, resolved.invocation.options, diagnostics)) return 1;
  return writeIntermediateOutput(*level, resolved.common, resolved.invocation, diagnostics) ? 0 : 1;
}

int execute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedLevelInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

const RouteDescriptor& routeDescriptor() {
  static constexpr ModuleRef kSupportedModules[] = {
      modules::textures::skipTextureExportModule,
      modules::textures::inputTextureFolderModule,
      modules::transform::rebaseTexturesModule,
  };
  static constexpr HelpExample kHelpExamples[] = {
      {"--auto-mount level:1 level1.glb", "Export a mounted Level to GLB."},
      {"--auto-mount level1.glb level:1", "Bake an authored GLB into the game resource layout."},
      {"level.fts level.glb", "Export a loose native Level bundle to GLB."},
  };
  static const RouteDescriptor kDescriptor{
      .kind = RouteKind::kLevel,
      .name = "level",
      .primary_input_formats =
          formatBit(Format::kFts) | formatBit(Format::kDlf) | formatBit(Format::kGlb) | formatBit(Format::kJson),
      .extra_input_formats = formatBit(Format::kLlf) | formatBit(Format::kDlf) | formatBit(Format::kJson),
      .output_formats =
          formatBit(Format::kFts) | formatBit(Format::kDlf) | formatBit(Format::kGlb) | formatBit(Format::kJson),
      .supported_modules = kSupportedModules,
      .modules = options::rootModules(),
      .create_options = createRouteOptions,
      .create_invocation = createRouteInvocation,
      .probe = probe,
      .resolve = resolve,
      .execute = execute,
      .help = {.synopsis = "<level> [companions...] <output>",
               .summary = "Convert native Level bundles, compatible JSON, and editable Level GLB.",
               .examples = kHelpExamples},
  };
  return kDescriptor;
}

}  // namespace cli::level
