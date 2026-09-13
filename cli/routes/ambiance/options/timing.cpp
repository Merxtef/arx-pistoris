// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/options/modules.h"

#include <span>

namespace cli::ambiance::options {
namespace {

class TrimTracksToMasterModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--trim-to-master"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--trim-to-master", "Trim non-master tracks to the master's shortest nominal duration."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<AmbianceOptions>().trim_tracks_to_master = true;
    return {};
  }
};

}  // namespace

const Module& trimTracksToMasterModule() { return moduleInstance<TrimTracksToMasterModule>(); }

}  // namespace cli::ambiance::options
