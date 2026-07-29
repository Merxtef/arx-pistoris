// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/route.h"

#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "pipeline/execution_context.h"
#include "resources/selector.h"
#include "routes/descriptor.h"
#include "routes/level/invocation.h"
#include "routes/level/mounted_textures.h"
#include "routes/level/operations.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"
#include "routes/level/resolution.h"
#include "routes/level/save.h"
#include "routes/level/state.h"
#include "routes/options.h"
#include "routes/types.h"

#include <cstdio>
#include <memory>
#include <variant>
#include <vector>

namespace cli::level {
namespace {

bool supportsOutput(Format output) {
  switch (output) {
    case Format::kGlb:
    case Format::kFts:
    case Format::kDlf:
    case Format::kJson:
      return true;
    default:
      return false;
  }
}

bool isPrimaryCompatible(FileFacts facts) {
  return facts.format == Format::kFts || facts.format == Format::kDlf || facts.format == Format::kGlb ||
         (facts.format == Format::kJson && facts.kind == PayloadKind::kFts);
}

std::unique_ptr<RouteOptions> createRouteOptions() { return std::make_unique<LevelOptions>(); }

std::unique_ptr<RouteInvocation> createRouteInvocation() { return std::make_unique<Invocation>(); }

int executeRoute(const ExecutionContext& context, RouteInvocation& invocation) {
  ResolvedLevelInvocation resolved{context, static_cast<Invocation&>(invocation)};
  return dispatch(resolved);
}

}  // namespace

ProbeResult probe(const std::vector<ClassifiedPath>& inputs, const OutputTarget& output, Format output_format,
                  Invocation& inv) {
  if (!supportsOutput(output_format)) return {};
  if (inputs.empty()) return {ProbeStatus::kInvalid, "Level route requires an input"};
  if (!isPrimaryCompatible(inputs[0].facts)) return {};

  inv.input = 0;
  inv.output = output;
  bool primary_fts = inputs[0].facts.kind == PayloadKind::kFts;
  bool primary_json = inputs[0].facts.format == Format::kJson;
  bool primary_glb = inputs[0].facts.format == Format::kGlb;
  bool primary_dlf = inputs[0].facts.format == Format::kDlf;
  if (primary_dlf && inputs.size() > 1) {
    return {ProbeStatus::kInvalid, "DLF Level input does not accept explicit extra inputs"};
  }
  if (inputs[0].resource_kind == ARX_RESOURCE_KIND_LEVEL) {
    const std::size_t positional = inputs[0].positional_index;
    for (const ClassifiedPath& input : inputs) {
      if (input.resource_kind != ARX_RESOURCE_KIND_LEVEL || input.positional_index != positional) {
        return {ProbeStatus::kInvalid, "level: input cannot be combined with other Level inputs"};
      }
    }
  }
  for (std::size_t i = 1; i < inputs.size(); ++i) {
    Format format = inputs[i].facts.format;
    bool native_llf_extra =
        primary_fts && ((primary_json && format == Format::kJson && inputs[i].facts.kind == PayloadKind::kLlf) ||
                        (!primary_json && format == Format::kLlf));
    bool native_dlf_extra =
        primary_fts && ((primary_json && format == Format::kJson && inputs[i].facts.kind == PayloadKind::kDlf) ||
                        (!primary_json && format == Format::kDlf));
    if (native_llf_extra) {
      if (inv.llf != kNoClassifiedPath) return {ProbeStatus::kInvalid, "Level route accepts at most one LLF input"};
      inv.llf = i;
      continue;
    }
    if (native_dlf_extra) {
      if (inv.dlf != kNoClassifiedPath) return {ProbeStatus::kInvalid, "Level route accepts at most one DLF input"};
      inv.dlf = i;
      continue;
    }
    if (primary_glb && (format == Format::kLlf || format == Format::kDlf)) {
      return {ProbeStatus::kInvalid, "Level GLB input does not accept native extras"};
    }
    return {ProbeStatus::kInvalid, "Level route accepts only LLF/DLF extras for native FTS input"};
  }
  return {ProbeStatus::kMatch};
}

ProbeResult probeRoute(const RouteProbeContext& ctx, RouteInvocation& invocation) {
  return probe(ctx.inputs, ctx.output, ctx.output_format, static_cast<Invocation&>(invocation));
}

bool resolveRoute(const RouteResolveContext& context, RouteInvocation& invocation) {
  return resolveInvocation(context, static_cast<Invocation&>(invocation));
}

bool printHelpSection(std::FILE* output, HelpSection section) {
  if (section != HelpSection::kConventions) return false;
  std::fprintf(output, "  Level GLB examples: docs/AUTHORING_GUIDE.md.\n");
  std::fprintf(output, "  Exact Level GLB naming: docs/AUTHORING_REFERENCE.md.\n");
  return true;
}

const RouteDescriptor& routeDescriptor() {
  static const RouteDescriptor kDescriptor{
      RouteKind::kLevel,
      "level",
      formatBit(Format::kFts) | formatBit(Format::kDlf) | formatBit(Format::kGlb) | formatBit(Format::kJson),
      formatBit(Format::kLlf) | formatBit(Format::kDlf) | formatBit(Format::kJson),
      formatBit(Format::kFts) | formatBit(Format::kDlf) | formatBit(Format::kGlb) | formatBit(Format::kJson),
      {},
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

int dispatch(const ResolvedLevelInvocation& inv) {
  if (inv.common.route().input == Format::kUnknown || inv.invocation.input == kNoClassifiedPath ||
      std::holds_alternative<std::monostate>(inv.invocation.state)) {
    diagnostic(DiagnosticCode::kLevelUnsupportedInput, "Unsupported Level input format");
    return 1;
  }
  const OutputConverterDescriptor* converter = inv.invocation.output_converter;
  if (!converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Unsupported Level output converter");
    return 1;
  }
  if (NativeLevelFiles* files = std::get_if<NativeLevelFiles>(&inv.invocation.state)) {
    return writeNativeOutput(*files, inv.common, inv.invocation) ? 0 : 1;
  }

  IntermediateLevel* level = std::get_if<IntermediateLevel>(&inv.invocation.state);
  if (!level) {
    diagnostic(DiagnosticCode::kLevelOutputFailed, "Level input state is unavailable");
    return 1;
  }

  operations::OperationDiagnostics diagnostics;
  if (converter->create_diagnostics) diagnostics = converter->create_diagnostics();
  if (!operations::applyLevelOperations(level->level, inv.invocation.options, diagnostics)) return 1;
  if (converter->requires_texture_images && inv.invocation.options.export_textures &&
      !inv.invocation.options.dlf_only && hasMissingReferencedTextureImages(level->level)) {
    loadMountedTextureImages(level->level, inv.common.io(), inv.invocation.textures);
  }
  return writeIntermediateOutput(*level, inv.common, inv.invocation, diagnostics) ? 0 : 1;
}

}  // namespace cli::level
