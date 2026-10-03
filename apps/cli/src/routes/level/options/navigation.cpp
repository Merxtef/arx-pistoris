// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {
namespace {

class GenerateNavigationModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--gen-navigation"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--gen-navigation",
            "Generate and prune the Level navigation surface, anchors, and links."};
  }

  std::span<const ModuleImplication> implications() const noexcept override {
    static constexpr ModuleImplication kImplications[] = {
        {generateNavSurfaceModule},
        {pruneNavSurfaceIslandsModule},
        {generateAnchorsModule},
        {connectAnchorsModule},
        {pruneAnchorIslandsModule},
    };
    return kImplications;
  }

  ModuleParseResult parse(ModuleParseContext&) const override { return {}; }
};

}  // namespace

const Module& generateNavigationModule() { return moduleInstance<GenerateNavigationModule>(); }

}  // namespace cli::level::options
