// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <cmath>
#include <span>

namespace cli::level::options {
namespace {

class PruneNavSurfaceIslandsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--prune-nav-surface-islands"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--prune-nav-surface-islands",
            "Prune small connected islands from the current navigation surface."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {navPruneRatioModule, navPruneMinAreaModule};
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().prune_nav_surface_islands = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::NavSurfacePruneOptions& pruning = context.routeOptions<LevelOptions>().nav_surface_pruning;
    if (!std::isfinite(pruning.min_component_area_ratio) || pruning.min_component_area_ratio < 0.0f ||
        pruning.min_component_area_ratio > 1.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--nav-prune-ratio must be inside [0, 1]");
      return false;
    }
    if (!std::isfinite(pruning.min_component_area) || pruning.min_component_area < 0.0) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--nav-prune-min-area must be non-negative");
      return false;
    }
    return true;
  }
};

class NavPruneRatioModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-prune-ratio"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--nav-prune-ratio <RATIO=0.05>",
            "Set minimum component area relative to the largest component."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(ctx,
                                  ctx.routeOptions<LevelOptions>().nav_surface_pruning.min_component_area_ratio,
                                  stableName(),
                                  "numeric ratio")};
  }
};

class NavPruneMinAreaModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-prune-min-area"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--nav-prune-min-area <SQUARE_UNITS=0>",
            "Set minimum retained navigation component area."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeDouble(ctx,
                                   ctx.routeOptions<LevelOptions>().nav_surface_pruning.min_component_area,
                                   stableName(),
                                   "numeric square units")};
  }
};

}  // namespace

const Module& pruneNavSurfaceIslandsModule() { return moduleInstance<PruneNavSurfaceIslandsModule>(); }

const Module& navPruneRatioModule() { return moduleInstance<NavPruneRatioModule>(); }

const Module& navPruneMinAreaModule() { return moduleInstance<NavPruneMinAreaModule>(); }

}  // namespace cli::level::options
