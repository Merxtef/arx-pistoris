// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {
namespace {

class FlattenPortalsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--flatten-portals"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--flatten-portals", "Flatten Level portal quads onto their canonical planes."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().flatten_portals = true;
    return {};
  }
};

}  // namespace

const Module& flattenPortalsModule() { return moduleInstance<FlattenPortalsModule>(); }

}  // namespace cli::level::options
