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

class ConnectAnchorsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--connect-anchors"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--connect-anchors", "Generate Level anchor links from current anchors."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {anchorLinkDistanceModule, anchorLinkRadiusScaleModule};
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().connect_anchors = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::AnchorGenOptions generation =
        effectiveAnchorGenerationOptions(context.routeOptions<LevelOptions>());
    const pistoris::Level::AnchorConnectionGenOptions connection =
        effectiveAnchorConnectionOptions(context.routeOptions<LevelOptions>(), generation);
    if (!std::isfinite(connection.max_distance) || connection.max_distance <= 0.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--anchor-link-distance must be positive");
      return false;
    }
    if (!std::isfinite(connection.radius_scale) || connection.radius_scale < 0.5f || connection.radius_scale > 1.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--anchor-link-radius-scale must be inside [0.5, 1]");
      return false;
    }
    return true;
  }
};

class AnchorLinkDistanceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-link-distance"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--anchor-link-distance <UNITS=1.5*spacing>",
            "Set generated anchor XZ link search distance."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (!modules::consumeFloat(
            ctx, ctx.routeOptions<LevelOptions>().anchor_connection.max_distance, stableName(), "numeric units"))
      return {.ok = false};
    ctx.routeOptions<LevelOptions>().anchor_link_distance_specified = true;
    return {};
  }
};

class AnchorLinkRadiusScaleModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-link-radius-scale"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--anchor-link-radius-scale <RATIO=0.9>", "Scale anchor radius during link traversal."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().anchor_connection.radius_scale, stableName(), "numeric ratio")};
  }
};

}  // namespace

const Module& connectAnchorsModule() { return moduleInstance<ConnectAnchorsModule>(); }

const Module& anchorLinkDistanceModule() { return moduleInstance<AnchorLinkDistanceModule>(); }

const Module& anchorLinkRadiusScaleModule() { return moduleInstance<AnchorLinkRadiusScaleModule>(); }

}  // namespace cli::level::options
