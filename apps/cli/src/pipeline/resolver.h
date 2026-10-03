// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/classification.h"
#include "pipeline/parsed.h"
#include "resources/selector.h"
#include "routes/descriptor.h"
#include "routes/types.h"

#include <memory>
#include <span>
#include <vector>

namespace cli {

struct OutputFormatResolution {
  Format extension_format = Format::kUnknown;
  FormatMask candidates = 0;
  Format selected = Format::kUnknown;
};

struct CliResolution {
  Format output = Format::kUnknown;
  OutputTarget output_target;
  OutputFormatResolution output_resolution;
  std::vector<ClassifiedPath> inputs;

  const RouteDescriptor* route_descriptor = nullptr;
  std::unique_ptr<RouteInvocation> route_invocation;
  ProbeResult route_probe;
  Route route;
  SelectedOutputConverter output_converter;
};

class IoService;

bool resolveCli(const ParsedCli& parsed, std::span<const ModuleInvocation> effective_modules, IoService& io,
                CliResolution& resolved);

}  // namespace cli
