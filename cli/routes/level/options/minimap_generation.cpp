// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/level.hpp"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <cmath>
#include <span>
#include <string>

namespace cli::level::options {
namespace {

bool consumeColor(ModuleParseContext& context, pistoris::ArxColor3& out, const char* name) {
  float values[3] = {};
  if (!modules::consumeFloats(context, values, 3, name, "a number")) return false;
  out = {values[0], values[1], values[2]};
  return true;
}

bool consumeImagePath(ModuleParseContext& context, std::string& out, const char* name) {
  if (context.index + 1 >= context.argc) {
    diagnostic(DiagnosticCode::kMissingArgument, "%s: expected image path", name);
    return false;
  }
  const char* path = context.argv[++context.index];
  if (*path == '\0') {
    diagnostic(DiagnosticCode::kInvalidModuleValue, "%s: image path must not be empty", name);
    return false;
  }
  out = path;
  return true;
}

bool validColor(const pistoris::ArxColor3& color) {
  return std::isfinite(color.r) && color.r >= 0.0f && color.r <= 1.0f && std::isfinite(color.g) && color.g >= 0.0f &&
         color.g <= 1.0f && std::isfinite(color.b) && color.b >= 0.0f && color.b <= 1.0f;
}

bool validateColor(const char* name, const pistoris::ArxColor3& color) {
  if (validColor(color)) return true;
  diagnostic(DiagnosticCode::kInvalidModuleValue, "%s channels must be inside [0, 1]", name);
  return false;
}

class GenerateMinimapModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--gen-minimap"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--gen-minimap", "Generate a full-domain Level minimap from current geometry."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {
        minimapForegroundColorModule,
        minimapForegroundImageModule,
        minimapBackgroundColorModule,
        minimapBackgroundImageModule,
        minimapWaterColorModule,
        minimapWaterImageModule,
        minimapLavaColorModule,
        minimapLavaImageModule,
        minimapHaloColorModule,
        minimapHaloRadiusModule,
    };
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    context.routeOptions<LevelOptions>().generate_minimap = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::MinimapGenerationOptions& generation =
        context.routeOptions<LevelOptions>().minimap_generation;
    if (!validateColor("--minimap-fg-color", generation.foreground.color) ||
        !validateColor("--minimap-bg-color", generation.background.color) ||
        !validateColor("--minimap-water-color", generation.water.color) ||
        !validateColor("--minimap-lava-color", generation.lava.color) ||
        !validateColor("--minimap-halo-color", generation.halo_color)) {
      return false;
    }
    return true;
  }
};

class MinimapBorderColorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-border-color"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--minimap-border-color <R=1> <G=1> <B=1>",
            "Set the one-pixel border color for game-layout minimap output."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    pistoris::ArxColor3 color{};
    if (!consumeColor(context, color, stableName())) return {.ok = false};
    context.routeOptions<LevelOptions>().minimap_border_color = color;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const auto& color = context.routeOptions<LevelOptions>().minimap_border_color;
    return !color || validateColor(stableName(), *color);
  }
};

class MinimapForegroundColorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-fg-color"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--minimap-fg-color <R=0.18> <G=0.34> <B=0.80>",
            "Set generated minimap foreground color or image multiplier."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {
        consumeColor(context, context.routeOptions<LevelOptions>().minimap_generation.foreground.color, stableName())};
  }
};

class MinimapForegroundImageModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-fg-image"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--minimap-fg-image <PATH>", "Sample generated minimap foreground color from an image."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {
        consumeImagePath(context, context.routeOptions<LevelOptions>().minimap_sampler_paths.foreground, stableName())};
  }
};

class MinimapBackgroundColorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-bg-color"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--minimap-bg-color <R=0.56> <G=0.68> <B=0.90>",
            "Set generated minimap background color or image multiplier."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {
        consumeColor(context, context.routeOptions<LevelOptions>().minimap_generation.background.color, stableName())};
  }
};

class MinimapBackgroundImageModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-bg-image"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--minimap-bg-image <PATH>", "Sample generated minimap background color from an image."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {
        consumeImagePath(context, context.routeOptions<LevelOptions>().minimap_sampler_paths.background, stableName())};
  }
};

class MinimapWaterColorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-water-color"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--minimap-water-color <R=0.72> <G=0.60> <B=0.45>",
            "Set generated minimap water color or image multiplier."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {consumeColor(context, context.routeOptions<LevelOptions>().minimap_generation.water.color, stableName())};
  }
};

class MinimapWaterImageModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-water-image"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--minimap-water-image <PATH>", "Sample generated minimap water color from an image."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {consumeImagePath(context, context.routeOptions<LevelOptions>().minimap_sampler_paths.water, stableName())};
  }
};

class MinimapLavaColorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-lava-color"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--minimap-lava-color <R=0.25> <G=0.80> <B=0.90>",
            "Set generated minimap lava color or image multiplier."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {consumeColor(context, context.routeOptions<LevelOptions>().minimap_generation.lava.color, stableName())};
  }
};

class MinimapLavaImageModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-lava-image"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--minimap-lava-image <PATH>", "Sample generated minimap lava color from an image."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {consumeImagePath(context, context.routeOptions<LevelOptions>().minimap_sampler_paths.lava, stableName())};
  }
};

class MinimapHaloColorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-halo-color"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--minimap-halo-color <R=1> <G=1> <B=1>", "Set generated minimap halo color."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    return {consumeColor(context, context.routeOptions<LevelOptions>().minimap_generation.halo_color, stableName())};
  }
};

class MinimapHaloRadiusModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--minimap-halo-radius"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--minimap-halo-radius <PIXELS=5>", "Set generated minimap halo radius; zero disables."};
  }

  ModuleParseResult parse(ModuleParseContext& context) const override {
    if (context.index + 1 >= context.argc ||
        !modules::parseUint32(context.argv[context.index + 1],
                              context.routeOptions<LevelOptions>().minimap_generation.halo_radius)) {
      diagnostic(DiagnosticCode::kInvalidNumber, "%s: expected unsigned integer", stableName());
      return {.ok = false};
    }
    ++context.index;
    return {};
  }
};

}  // namespace

const Module& generateMinimapModule() { return moduleInstance<GenerateMinimapModule>(); }

const Module& minimapBorderColorModule() { return moduleInstance<MinimapBorderColorModule>(); }

const Module& minimapForegroundColorModule() { return moduleInstance<MinimapForegroundColorModule>(); }

const Module& minimapForegroundImageModule() { return moduleInstance<MinimapForegroundImageModule>(); }

const Module& minimapBackgroundColorModule() { return moduleInstance<MinimapBackgroundColorModule>(); }

const Module& minimapBackgroundImageModule() { return moduleInstance<MinimapBackgroundImageModule>(); }

const Module& minimapWaterColorModule() { return moduleInstance<MinimapWaterColorModule>(); }

const Module& minimapWaterImageModule() { return moduleInstance<MinimapWaterImageModule>(); }

const Module& minimapLavaColorModule() { return moduleInstance<MinimapLavaColorModule>(); }

const Module& minimapLavaImageModule() { return moduleInstance<MinimapLavaImageModule>(); }

const Module& minimapHaloColorModule() { return moduleInstance<MinimapHaloColorModule>(); }

const Module& minimapHaloRadiusModule() { return moduleInstance<MinimapHaloRadiusModule>(); }

}  // namespace cli::level::options
