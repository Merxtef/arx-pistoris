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

class PruneAnchorIslandsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--prune-anchor-islands"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--prune-anchor-islands",
            "Prune small connected islands from the current anchor graph."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {anchorPruneRatioModule, anchorPruneMinCountModule};
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().prune_anchor_islands = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::AnchorPruneOptions& pruning = context.routeOptions<LevelOptions>().anchor_pruning;
    if (!std::isfinite(pruning.min_component_anchor_ratio) || pruning.min_component_anchor_ratio < 0.0f ||
        pruning.min_component_anchor_ratio > 1.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--anchor-prune-ratio must be inside [0, 1]");
      return false;
    }
    return true;
  }
};

class AnchorPruneRatioModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-prune-ratio"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--anchor-prune-ratio <RATIO=0.05>",
            "Set minimum anchor count relative to the largest component."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(ctx,
                                  ctx.routeOptions<LevelOptions>().anchor_pruning.min_component_anchor_ratio,
                                  stableName(),
                                  "numeric ratio")};
  }
};

class AnchorPruneMinCountModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-prune-min-count"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--anchor-prune-min-count <COUNT=1>", "Set minimum retained anchors per component."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc ||
        !modules::parseUint32(ctx.argv[ctx.index + 1],
                              ctx.routeOptions<LevelOptions>().anchor_pruning.min_component_anchor_count)) {
      diagnostic(DiagnosticCode::kInvalidNumber, "%s: expected unsigned integer", stableName());
      return {.ok = false};
    }
    ++ctx.index;
    return {};
  }
};

}  // namespace

const Module& pruneAnchorIslandsModule() { return moduleInstance<PruneAnchorIslandsModule>(); }

const Module& anchorPruneRatioModule() { return moduleInstance<AnchorPruneRatioModule>(); }

const Module& anchorPruneMinCountModule() { return moduleInstance<AnchorPruneMinCountModule>(); }

}  // namespace cli::level::options
