// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/classification.h"
#include "modules/module.h"
#include "resources/selector.h"
#include "routes/types.h"

#include <cstdio>
#include <memory>
#include <span>
#include <vector>

namespace cli {

class IoService;
class ExecutionContext;
struct ParsedOptions;
struct SelectedOutputConverter;

struct RouteInvocation {
  virtual ~RouteInvocation() = default;
};

struct RouteProbeContext {
  const std::vector<ClassifiedPath>& inputs;
  const OutputTarget& output;
  Format output_format;
  RouteMask route_constraints;
};

struct RouteResolveContext {
  std::vector<ClassifiedPath>& inputs;
  const OutputTarget& output;
  Format output_format;
  Route route;
  const ParsedOptions& options;
  const RouteOptions* route_options;
  const SelectedOutputConverter& output_converter;
  std::span<const ModuleInvocation> modules;
  IoService& io;
};

using RouteOptionsFactoryFn = std::unique_ptr<RouteOptions> (*)();
using RouteInvocationFactoryFn = std::unique_ptr<RouteInvocation> (*)();
using RouteProbeFn = ProbeResult (*)(const RouteProbeContext& ctx, RouteInvocation& invocation);
using RouteResolveFn = bool (*)(const RouteResolveContext& ctx, RouteInvocation& invocation);
using RouteExecuteFn = int (*)(const ExecutionContext& context, RouteInvocation& invocation);
using RouteHelpProviderFn = bool (*)(std::FILE* output, HelpSection section);

struct RouteDescriptor {
  RouteKind kind;
  const char* name;
  FormatMask primary_input_formats;
  FormatMask extra_input_formats;
  FormatMask output_formats;
  std::span<const ModuleRef> supported_modules;
  std::span<const ModuleRef> modules;
  RouteOptionsFactoryFn create_options;
  RouteInvocationFactoryFn create_invocation;
  RouteProbeFn probe;
  RouteResolveFn resolve;
  RouteExecuteFn execute;
  RouteHelpProviderFn help_provider;
};

}  // namespace cli
