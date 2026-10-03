// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/level.hpp"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <span>

namespace cli::level::options {
namespace {

bool parseFaceFlags(const char* text, pistoris::FaceType& out) {
  if (!text) return false;
  char* end = nullptr;
  errno = 0;
  unsigned long value = std::strtoul(text, &end, 0);
  if (end == text || *end != '\0' || errno == ERANGE ||
      (value & ~static_cast<unsigned long>(pistoris::kLevelFaceBitsAll)) != 0)
    return false;
  out = static_cast<pistoris::FaceType>(value);
  return true;
}

class GenerateNavSurfaceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--gen-nav-surface"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--gen-nav-surface", "Generate Level navigation surface from static geometry."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {
        navFromFloorModule,
        navRadiusModule,
        navHeightModule,
        navClearanceModule,
        navMaxStepUpModule,
        navMaxSlopeDegreesModule,
        navIgnoreFlagsModule,
    };
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().generate_nav_surface = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::NavSurfaceGenOptions& generation =
        context.routeOptions<LevelOptions>().nav_surface_generation;
    if (!std::isfinite(generation.radius) || generation.radius < pistoris::kMinAnchorRadius) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--nav-radius must be at least %.3g",
                 static_cast<double>(pistoris::kMinAnchorRadius));
      return false;
    }
    if (!std::isfinite(generation.height) || generation.height > pistoris::kMinAnchorHeight) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--nav-height must be at least %.3g",
                 static_cast<double>(-pistoris::kMinAnchorHeight));
      return false;
    }
    if (!std::isfinite(generation.clearance) || generation.clearance < 0.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--nav-clearance must be non-negative");
      return false;
    }
    if (!std::isfinite(generation.max_step_up) || generation.max_step_up < 0.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--nav-max-step-up must be non-negative");
      return false;
    }
    if (!std::isfinite(generation.support_min_up_cos) || generation.support_min_up_cos < 0.0f ||
        generation.support_min_up_cos > 1.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--nav-max-slope must be inside [0, 90]");
      return false;
    }
    if ((generation.support_ignore_flags & ~pistoris::kLevelFaceBitsAll) != 0) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--nav-ignore-flags contains unknown bits");
      return false;
    }
    return true;
  }
};

class NavFromFloorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-from-floor"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--nav-from-floor",
            "Use filtered raw floor polygons as the generated navigation surface."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {navRadiusModule, navHeightModule, navMaxStepUpModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().nav_surface_from_floor = true;
    return {};
  }
};

class NavRadiusModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-radius"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--nav-radius <UNITS=50>", "Set generated navigation proxy cylinder radius."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().nav_surface_generation.radius, stableName(), "numeric units")};
  }
};

class NavHeightModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-height"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--nav-height <UNITS=165>", "Set generated navigation proxy cylinder height."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    float height = 0.0f;
    if (!modules::consumeFloat(ctx, height, stableName(), "numeric units")) return {.ok = false};
    ctx.routeOptions<LevelOptions>().nav_surface_generation.height = -height;
    return {};
  }
};

class NavClearanceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-clearance"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--nav-clearance <UNITS=5>",
            "Set generated navigation surface clearance above support."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().nav_surface_generation.clearance, stableName(), "numeric units")};
  }
};

class NavMaxStepUpModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-max-step-up"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--nav-max-step-up <UNITS=55>", "Set generated navigation support probe tolerance."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().nav_surface_generation.max_step_up, stableName(), "numeric units")};
  }
};

class NavMaxSlopeDegreesModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-max-slope"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--nav-max-slope <DEG=53.9726>", "Set generated navigation support slope limit."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    float degrees = 0.0f;
    if (!modules::consumeFloat(ctx, degrees, stableName(), "numeric degrees")) return {.ok = false};
    ctx.routeOptions<LevelOptions>().nav_surface_generation.support_min_up_cos =
        degrees < 0.0f || degrees > 90.0f ? -1.0f
        : degrees == 90.0f                ? 0.0f
                                          : std::cos(degrees * 3.14159265358979323846f / 180.0f);
    return {};
  }
};

class NavIgnoreFlagsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--nav-ignore-flags"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--nav-ignore-flags <BITS=0xC00C>",
            "Set generated navigation support ignore flags bitmask."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc ||
        !parseFaceFlags(ctx.argv[ctx.index + 1],
                        ctx.routeOptions<LevelOptions>().nav_surface_generation.support_ignore_flags)) {
      diagnostic(DiagnosticCode::kInvalidNumber, "%s: expected face flag bitmask", stableName());
      return {.ok = false};
    }
    ++ctx.index;
    return {};
  }
};

}  // namespace

const Module& generateNavSurfaceModule() { return moduleInstance<GenerateNavSurfaceModule>(); }

const Module& navFromFloorModule() { return moduleInstance<NavFromFloorModule>(); }

const Module& navRadiusModule() { return moduleInstance<NavRadiusModule>(); }

const Module& navHeightModule() { return moduleInstance<NavHeightModule>(); }

const Module& navClearanceModule() { return moduleInstance<NavClearanceModule>(); }

const Module& navMaxStepUpModule() { return moduleInstance<NavMaxStepUpModule>(); }

const Module& navMaxSlopeDegreesModule() { return moduleInstance<NavMaxSlopeDegreesModule>(); }

const Module& navIgnoreFlagsModule() { return moduleInstance<NavIgnoreFlagsModule>(); }

}  // namespace cli::level::options
