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

class GenerateAnchorsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--generate-anchors"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--generate-anchors", "Generate Level anchors from arx_nav_surface."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {anchorSpacingModule, anchorRadiusModule, anchorHeightModule};
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().generate_anchors = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::AnchorGenOptions generation =
        effectiveAnchorGenerationOptions(context.routeOptions<LevelOptions>());
    if (!std::isfinite(generation.radius) || generation.radius < pistoris::kMinAnchorRadius) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--anchor-radius must be at least %.3g",
                 static_cast<double>(pistoris::kMinAnchorRadius));
      return false;
    }
    if (!std::isfinite(generation.sample_spacing) || generation.sample_spacing < pistoris::kMinAnchorSpacing) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--anchor-spacing must be at least %.3g",
                 static_cast<double>(pistoris::kMinAnchorSpacing));
      return false;
    }
    if (!std::isfinite(generation.height) || generation.height > pistoris::kMinAnchorHeight) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--anchor-height must be at least %.3g",
                 static_cast<double>(-pistoris::kMinAnchorHeight));
      return false;
    }
    return true;
  }
};

class AnchorSpacingModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-spacing"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--anchor-spacing <UNITS=2*radius>", "Set generated anchor sample spacing."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (!modules::consumeFloat(
            ctx, ctx.routeOptions<LevelOptions>().anchor_generation.sample_spacing, stableName(), "numeric units"))
      return {.ok = false};
    ctx.routeOptions<LevelOptions>().anchor_spacing_specified = true;
    return {};
  }
};

class AnchorRadiusModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-radius"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--anchor-radius <UNITS=50>", "Set generated anchor cylinder radius."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().anchor_generation.radius, stableName(), "numeric units")};
  }
};

class AnchorHeightModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--anchor-height"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--anchor-height <UNITS=165>", "Set generated anchor cylinder height."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    float height = 0.0f;
    if (!modules::consumeFloat(ctx, height, stableName(), "numeric units")) return {.ok = false};
    ctx.routeOptions<LevelOptions>().anchor_generation.height = -height;
    return {};
  }
};

}  // namespace

const Module& generateAnchorsModule() { return moduleInstance<GenerateAnchorsModule>(); }

const Module& anchorSpacingModule() { return moduleInstance<AnchorSpacingModule>(); }

const Module& anchorRadiusModule() { return moduleInstance<AnchorRadiusModule>(); }

const Module& anchorHeightModule() { return moduleInstance<AnchorHeightModule>(); }

}  // namespace cli::level::options
