// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "console/help_request.h"
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
#include <optional>
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

struct TextureIoOptions {
  bool export_files = true;
  bool input_folder_specified = false;
  std::string input_folder;
};

struct SoundIoOptions {
  bool export_files = true;
  bool input_folder_specified = false;
  std::string input_folder;
};

struct ParsedOptions {
  std::optional<HelpRequest> help;
  std::vector<std::string> read_mounts;
  std::string write_mount;
  bool auto_mount = false;
  pistoris::NativeTextMode native_text_mode = pistoris::NativeTextMode::kAuto;
  bool version = false;

  SharedConversionOptions conversion;
  FormatOptions format;
  FormatModifierOptions format_modifiers;
  TextureIoOptions textures;
  SoundIoOptions sounds;
  std::vector<RouteOptionEntry> route_options;

  RouteKind kind_override = RouteKind::kUnknown;
  ResourceListingKind resource_listing = ResourceListingKind::kNone;
  ArxLogLevel log_level = ARX_LOG_INFO;
  OverwriteMode overwrite = OverwriteMode::kAsk;
  bool dry_run = false;
  bool keep_first_resource = false;
};

RouteOptions* ensureRouteOptions(ParsedOptions& options, const RouteDescriptor& route);
const RouteOptions* findRouteOptions(const ParsedOptions& options, const RouteDescriptor& route) noexcept;

}  // namespace cli
