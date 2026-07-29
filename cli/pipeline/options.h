// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris_types.h"

#include "conversion/options.h"
#include "formats/format.h"
#include "formats/modifiers.h"
#include "formats/options.h"
#include "io/policy.h"
#include "modules/module.h"
#include "resources/discovery.h"
#include "routes/types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cli {

struct SelectedOutputConverter {
  const Module* module = nullptr;
};

struct RouteDescriptor;

struct RouteOptionEntry {
  const RouteDescriptor* route = nullptr;
  std::unique_ptr<RouteOptions> options;
};

struct ParsedOptions {
  std::vector<const char*> help_topics;
  std::vector<std::string> mounts;
  bool help = false;
  bool version = false;

  SharedConversionOptions conversion;
  FormatOptions format;
  FormatModifierOptions format_modifiers;
  std::vector<RouteOptionEntry> route_options;

  RouteKind kind_override = RouteKind::kUnknown;
  ResourceListingKind resource_listing = ResourceListingKind::kNone;
  ArxLogLevel log_level = ARX_LOG_INFO;
  OverwriteMode overwrite = OverwriteMode::kAsk;
  bool dry_run = false;
};

RouteOptions* ensureRouteOptions(ParsedOptions& options, const RouteDescriptor& route);
const RouteOptions* findRouteOptions(const ParsedOptions& options, const RouteDescriptor& route) noexcept;

}  // namespace cli
