// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "pipeline/resolver.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "modules/module.h"
#include "modules/registry.h"
#include "pipeline/options.h"
#include "pipeline/parsed.h"
#include "resources/input.h"
#include "resources/layout.h"
#include "resources/selector.h"
#include "routes/descriptor.h"
#include "routes/registry.h"
#include "routes/types.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {

bool hasSingleRoute(cli::RouteMask routes, cli::RouteKind& out) {
  cli::RouteKind found = cli::RouteKind::kUnknown;
  cli::RouteRegistryView routes_view = cli::routeRegistry();
  for (std::size_t i = 0; i < routes_view.count; ++i) {
    const cli::RouteDescriptor& route = routes_view.routes[i];
    if ((routes & cli::routeBit(route.kind)) == 0) continue;
    if (found != cli::RouteKind::kUnknown) return false;
    found = route.kind;
  }
  out = found;
  return found != cli::RouteKind::kUnknown;
}

bool moduleRouteConstraint(const cli::Module& module, cli::RouteMask& out) {
  const cli::RegisteredModule* registered = cli::registeredModule(module);
  if (registered && registered->owner) {
    out = cli::routeBit(registered->owner->kind);
    return true;
  }
  switch (module.category()) {
    case cli::ModuleCategory::kSystem:
    case cli::ModuleCategory::kTerminalAction:
    case cli::ModuleCategory::kFormatModifier:
      return false;
    case cli::ModuleCategory::kOutputFormat:
      out = cli::routesSupportingModule(module);
      return out != cli::kNoRoutes;
    case cli::ModuleCategory::kRoute:
    case cli::ModuleCategory::kSharedConversion:
    case cli::ModuleCategory::kOutputConverter:
    case cli::ModuleCategory::kNativeBakeModifier:
    case cli::ModuleCategory::kInputModifier:
      out = cli::routesSupportingModule(module);
      return true;
  }
  return false;
}

cli::FormatMask allRouteOutputFormats() {
  cli::FormatMask formats = 0;
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t i = 0; i < routes.count; ++i) formats |= routes.routes[i].output_formats;
  return formats;
}

bool singleFormatFromMask(cli::FormatMask mask, cli::Format& out) {
  cli::Format found = cli::Format::kUnknown;
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t i = 0; i < routes.count; ++i) {
    cli::FormatMask remaining = routes.routes[i].output_formats & mask;
    for (unsigned bit = 0; remaining != 0; ++bit) {
      cli::FormatMask current = static_cast<cli::FormatMask>(1u << bit);
      if ((remaining & current) == 0) continue;
      remaining &= ~current;
      cli::Format format = static_cast<cli::Format>(bit);
      if (found != cli::Format::kUnknown && found != format) return false;
      found = format;
    }
  }
  out = found;
  return found != cli::Format::kUnknown;
}

cli::FormatMask moduleOutputConstraint(const cli::Module& module) {
  cli::FormatMask mask = module.outputFormats();
  if (module.requestedOutputFormat() != cli::Format::kUnknown) {
    cli::FormatMask requested = cli::formatBit(module.requestedOutputFormat());
    mask &= requested;
  }
  return mask;
}

bool resolveOutputFormat(const cli::ParsedCli& parsed, std::span<const cli::ModuleInvocation> modules,
                         const cli::IoService& io, cli::OutputTarget& target, cli::OutputFormatResolution& resolved) {
  if (!cli::resolveOutputTarget(parsed.output, io, target)) return false;
  resolved.extension_format = target.format;
  if (resolved.extension_format == cli::Format::kUnknown) {
    cli::diagnostic(cli::DiagnosticCode::kUnsupportedOutputFormat, "Unsupported output format: %s", parsed.output);
    return false;
  }

  resolved.candidates = allRouteOutputFormats() & cli::formatBit(resolved.extension_format);
  if (resolved.candidates == 0) {
    cli::diagnostic(cli::DiagnosticCode::kUnsupportedOutputFormat, "Unsupported output format: %s", parsed.output);
    return false;
  }

  for (const cli::ModuleInvocation& invocation : modules) {
    const cli::Module* module = invocation.module;
    if (!module) continue;

    cli::FormatMask constraint = moduleOutputConstraint(*module);
    if ((resolved.candidates & constraint) != 0) {
      resolved.candidates &= constraint;
      continue;
    }

    cli::diagnostic(cli::DiagnosticCode::kModuleOutputMismatch,
                    "%s is not compatible with %s output",
                    module->stableName(),
                    cli::formatName(resolved.extension_format));
    return false;
  }

  if (!singleFormatFromMask(resolved.candidates, resolved.selected)) {
    cli::diagnostic(cli::DiagnosticCode::kAmbiguousOutputFormat, "Ambiguous output format for: %s", parsed.output);
    return false;
  }
  return true;
}

cli::RouteMask initialRouteCandidates(cli::Format output) {
  cli::RouteMask candidates = cli::kNoRoutes;
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t i = 0; i < routes.count; ++i) {
    const cli::RouteDescriptor& route = routes.routes[i];
    if ((route.output_formats & cli::formatBit(output)) == 0) continue;
    candidates |= cli::routeBit(route.kind);
  }
  return candidates;
}

cli::RouteMask constrainedRouteCandidates(const cli::ParsedCli& parsed, std::span<const cli::ModuleInvocation> modules,
                                          cli::Format output) {
  const cli::ParsedOptions& options = parsed.options;
  cli::RouteMask candidates = initialRouteCandidates(output);

  if (options.kind_override != cli::RouteKind::kUnknown) {
    candidates &= cli::routeBit(options.kind_override);
  }

  for (const cli::ModuleInvocation& invocation : modules) {
    const cli::Module* module = invocation.module;
    if (!module) continue;
    cli::RouteMask constraint = cli::kNoRoutes;
    if (!moduleRouteConstraint(*module, constraint)) continue;
    if (constraint == cli::kNoRoutes) return cli::kNoRoutes;
    candidates &= constraint;
  }
  return candidates;
}

bool formatAllowed(cli::FormatMask formats, cli::Format format) { return (formats & cli::formatBit(format)) != 0; }

bool routeMetadataAllowsInputs(const cli::RouteDescriptor& route, const std::vector<cli::ClassifiedPath>& inputs) {
  if (inputs.empty()) return true;
  if (!formatAllowed(route.primary_input_formats, inputs[0].facts.format)) return false;
  for (std::size_t i = 1; i < inputs.size(); ++i)
    if (!formatAllowed(route.extra_input_formats, inputs[i].facts.format)) return false;
  return true;
}

cli::RouteMask filterRouteCandidatesByInputMetadata(cli::RouteMask candidates,
                                                    const std::vector<cli::ClassifiedPath>& inputs) {
  cli::RouteMask filtered = cli::kNoRoutes;
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t i = 0; i < routes.count; ++i) {
    const cli::RouteDescriptor& route = routes.routes[i];
    if ((candidates & cli::routeBit(route.kind)) == 0) continue;
    if (routeMetadataAllowsInputs(route, inputs)) filtered |= cli::routeBit(route.kind);
  }
  return filtered;
}

enum class RouteResolutionPhase : std::uint8_t {
  kConstraints,
  kInputMetadata,
  kProbe,
};

cli::DiagnosticCode noRouteDiagnostic(RouteResolutionPhase phase) {
  switch (phase) {
    case RouteResolutionPhase::kConstraints:
      return cli::DiagnosticCode::kRouteConstraintConflict;
    case RouteResolutionPhase::kInputMetadata:
      return cli::DiagnosticCode::kRouteInputMismatch;
    case RouteResolutionPhase::kProbe:
      return cli::DiagnosticCode::kNoRoute;
  }
  return cli::DiagnosticCode::kNoRoute;
}

const char* noRouteMessage(RouteResolutionPhase phase) {
  switch (phase) {
    case RouteResolutionPhase::kConstraints:
      return "Route constraints from output, --kind, and modules conflict";
    case RouteResolutionPhase::kInputMetadata:
      return "No route accepts the classified input shape with the selected output and modules";
    case RouteResolutionPhase::kProbe:
      return "No route claims the selected input, output, and options";
  }
  return "No route can satisfy the selected input, output, and options";
}

bool resolveClaimedRoute(cli::RouteMask candidates, cli::RouteKind& out, RouteResolutionPhase phase) {
  cli::RouteKind resolved = cli::RouteKind::kUnknown;
  if (hasSingleRoute(candidates, resolved)) {
    out = resolved;
    return true;
  }

  if (candidates == cli::kNoRoutes) {
    cli::diagnostic(noRouteDiagnostic(phase), "%s", noRouteMessage(phase));
    return false;
  }

  cli::diagnosticPrefix(cli::DiagnosticCode::kAmbiguousRoute);
  std::fprintf(stderr, "Ambiguous route; use --kind");
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t i = 0; i < routes.count; ++i)
    if ((candidates & cli::routeBit(routes.routes[i].kind)) != 0) std::fprintf(stderr, " %s", routes.routes[i].name);
  std::fprintf(stderr, "\n");
  return false;
}

cli::SelectedOutputConverter selectedOutputConverter(std::span<const cli::ModuleInvocation> modules) {
  for (const cli::ModuleInvocation& invocation : modules) {
    if (invocation.module && invocation.module->category() == cli::ModuleCategory::kOutputConverter) {
      return {.module = invocation.module};
    }
  }
  return {};
}

bool requiresIntermediate(std::span<const cli::ModuleInvocation> modules) {
  return std::ranges::any_of(modules, [](const cli::ModuleInvocation& invocation) {
    if (!invocation.module) return false;
    switch (invocation.module->category()) {
      case cli::ModuleCategory::kRoute:
      case cli::ModuleCategory::kSharedConversion:
      case cli::ModuleCategory::kNativeBakeModifier:
        return true;
      default:
        return false;
    }
  });
}

bool validateSelectedProbe(const cli::CliResolution& resolved) {
  if (resolved.route_probe.status != cli::ProbeStatus::kInvalid) return true;
  cli::diagnostic(cli::DiagnosticCode::kRouteInvocationInvalid,
                  "%s",
                  resolved.route_probe.message ? resolved.route_probe.message : "Selected route invocation is invalid");
  return false;
}

bool validateFormatModifiers(std::span<const cli::ModuleInvocation> modules, const cli::Route& route) {
  const cli::FormatMask route_formats = cli::formatBit(route.input) | cli::formatBit(route.output);
  for (const cli::ModuleInvocation& invocation : modules) {
    const cli::Module* module = invocation.module;
    if (!module || module->category() != cli::ModuleCategory::kFormatModifier) continue;
    if ((module->modifiedFormats() & route_formats) != 0) continue;
    cli::diagnostic(cli::DiagnosticCode::kModuleFormatMismatch,
                    "%s requires its format on either the input or output conversion leg",
                    module->stableName());
    return false;
  }
  return true;
}

struct ProbedRoute {
  const cli::RouteDescriptor* descriptor = nullptr;
  std::unique_ptr<cli::RouteInvocation> invocation;
  cli::ProbeResult result;
};

cli::RouteMask probeRoutes(cli::RouteMask route_candidates, const cli::CliResolution& resolved,
                           std::vector<ProbedRoute>& probed) {
  cli::RouteMask claimed_routes = cli::kNoRoutes;
  cli::RouteProbeContext ctx{resolved.inputs, resolved.output_target, resolved.output};
  cli::RouteRegistryView routes = cli::routeRegistry();
  for (std::size_t i = 0; i < routes.count; ++i) {
    const cli::RouteDescriptor& route = routes.routes[i];
    if ((route_candidates & cli::routeBit(route.kind)) == 0) continue;

    if (!route.create_invocation || !route.probe) continue;
    std::unique_ptr<cli::RouteInvocation> invocation = route.create_invocation();
    if (!invocation) continue;
    cli::ProbeResult result = route.probe(ctx, *invocation);
    if (result.status == cli::ProbeStatus::kNoMatch) continue;
    claimed_routes |= cli::routeBit(route.kind);
    probed.push_back({&route, std::move(invocation), result});
  }
  return claimed_routes;
}

}  // namespace

namespace cli {

bool resolveCli(const ParsedCli& parsed, std::span<const ModuleInvocation> effective_modules, IoService& io,
                CliResolution& resolved) {
  const ParsedOptions& options = parsed.options;
  if (!resolveOutputFormat(parsed, effective_modules, io, resolved.output_target, resolved.output_resolution)) {
    return false;
  }
  resolved.output = resolved.output_resolution.selected;
  if (options.auto_mount && options.write_mount.empty() && resolved.output_target.layout == ResourceLayout::kGame &&
      !io.useDefaultGameWriteMount())
    return false;

  RouteMask route_candidates = constrainedRouteCandidates(parsed, effective_modules, resolved.output);
  if (route_candidates == kNoRoutes) {
    RouteKind ignored = RouteKind::kUnknown;
    resolveClaimedRoute(route_candidates, ignored, RouteResolutionPhase::kConstraints);
    return false;
  }

  if (!loadClassifiedInputs(parsed.inputs, io, resolved.inputs)) return false;
  route_candidates = filterRouteCandidatesByInputMetadata(route_candidates, resolved.inputs);
  if (route_candidates == kNoRoutes) {
    RouteKind ignored = RouteKind::kUnknown;
    resolveClaimedRoute(route_candidates, ignored, RouteResolutionPhase::kInputMetadata);
    return false;
  }

  std::vector<ProbedRoute> probed;
  RouteMask claimed_routes = probeRoutes(route_candidates, resolved, probed);
  RouteKind route_kind = RouteKind::kUnknown;
  if (!resolveClaimedRoute(claimed_routes, route_kind, RouteResolutionPhase::kProbe)) return false;

  auto selected = std::ranges::find_if(probed, [&](const ProbedRoute& candidate) {
    return candidate.descriptor && candidate.descriptor->kind == route_kind;
  });
  if (selected == probed.end()) return false;
  resolved.route_descriptor = selected->descriptor;
  resolved.route_invocation = std::move(selected->invocation);
  resolved.route_probe = selected->result;
  if (!validateSelectedProbe(resolved)) return false;
  if (!resolved.route_invocation) return false;
  resolved.route_invocation->native_text_mode = options.native_text_mode;

  resolved.route = {.kind = route_kind, .input = resolved.inputs[0].facts.format, .output = resolved.output};
  if (!validateFormatModifiers(effective_modules, resolved.route)) return false;
  resolved.output_converter = selectedOutputConverter(effective_modules);
  const RouteDescriptor* descriptor = resolved.route_descriptor;
  if (!descriptor || !descriptor->resolve || !resolved.route_invocation) return false;
  RouteResolveContext ctx{resolved.inputs,
                          resolved.output_target,
                          resolved.route,
                          options.conversion,
                          options.format,
                          options.format_modifiers,
                          options.textures,
                          options.sounds,
                          findRouteOptions(options, *descriptor),
                          resolved.output_converter,
                          requiresIntermediate(effective_modules),
                          io};
  return descriptor->resolve(ctx, *resolved.route_invocation);
}

}  // namespace cli
