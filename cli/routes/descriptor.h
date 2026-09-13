// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/classification.h"
#include "modules/module.h"
#include "resources/selector.h"
#include "routes/types.h"

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

namespace cli {

class IoService;
class ExecutionContext;
struct FormatModifierOptions;
struct FormatOptions;
struct SharedConversionOptions;
struct SoundIoOptions;
struct SelectedOutputConverter;
struct TextureIoOptions;

struct RouteInvocation {
  virtual ~RouteInvocation() = default;
};

struct RouteProbeContext {
  const std::vector<ClassifiedPath>& inputs;
  const OutputTarget& output;
  Format output_format;
};

struct RouteResolveContext {
  std::vector<ClassifiedPath>& inputs;
  const OutputTarget& output;
  Route route;
  const SharedConversionOptions& conversion;
  const FormatOptions& format_options;
  const FormatModifierOptions& format_modifiers;
  const TextureIoOptions& texture_options;
  const SoundIoOptions& sound_options;
  const RouteOptions* route_options;
  const SelectedOutputConverter& output_converter;
  bool requires_intermediate;
  IoService& io;
};

using RouteOptionsFactoryFn = std::unique_ptr<RouteOptions> (*)();
using RouteInvocationFactoryFn = std::unique_ptr<RouteInvocation> (*)();
using RouteProbeFn = ProbeResult (*)(const RouteProbeContext& ctx, RouteInvocation& invocation);
using RouteResolveFn = bool (*)(const RouteResolveContext& ctx, RouteInvocation& invocation);
using RouteExecuteFn = int (*)(const ExecutionContext& context, RouteInvocation& invocation);
struct HelpExample {
  const char* arguments;
  const char* description;
};

inline constexpr std::size_t kMaxRouteHelpExamples = 3;

struct RouteHelp {
  const char* synopsis;
  const char* summary;
  std::span<const HelpExample> examples;
};

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
  RouteHelp help;
};

}  // namespace cli
