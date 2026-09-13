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

namespace cli::level::options {
namespace {

class GenerateStaticLightingModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--gen-static-lighting"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--gen-static-lighting", "Generate Level static corner lighting from current lights."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {
        lightAmbientModule,
        lightGlobalFactorModule,
        lightNoNormalsModule,
        lightNoShadowsModule,
    };
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().generate_static_lighting = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::StaticLightingGenOptions& generation =
        context.routeOptions<LevelOptions>().static_lighting_generation;
    const pistoris::ArxColor3& ambient = generation.ambient_color;
    if (!std::isfinite(ambient.r) || ambient.r < 0.0f || ambient.r > 1.0f || !std::isfinite(ambient.g) ||
        ambient.g < 0.0f || ambient.g > 1.0f || !std::isfinite(ambient.b) || ambient.b < 0.0f || ambient.b > 1.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--light-ambient channels must be inside [0, 1]");
      return false;
    }
    if (!std::isfinite(generation.global_factor) || generation.global_factor < 0.0f) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--light-global-factor must be non-negative");
      return false;
    }
    return true;
  }
};

class LightAmbientModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--light-ambient"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--light-ambient <R=0.25> <G=0.25> <B=0.25>",
            "Set generated static lighting ambient color."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    float values[3] = {};
    if (!modules::consumeFloats(ctx, values, 3, stableName(), "a number")) return {.ok = false};
    ctx.routeOptions<LevelOptions>().static_lighting_generation.ambient_color = {values[0], values[1], values[2]};
    return {};
  }
};

class LightGlobalFactorModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--light-global-factor"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--light-global-factor <VALUE=0.85>",
            "Set generated static lighting global multiplier."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(ctx,
                                  ctx.routeOptions<LevelOptions>().static_lighting_generation.global_factor,
                                  stableName(),
                                  "numeric multiplier")};
  }
};

class LightNoNormalsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--light-no-normals"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--light-no-normals", "Disable generated static lighting normal response."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().static_lighting_generation.use_normals = false;
    return {};
  }
};

class LightNoShadowsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--light-no-shadows"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--light-no-shadows", "Disable generated static lighting shadow visibility."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().static_lighting_generation.use_shadows = false;
    return {};
  }
};

}  // namespace

const Module& generateStaticLightingModule() { return moduleInstance<GenerateStaticLightingModule>(); }

const Module& lightAmbientModule() { return moduleInstance<LightAmbientModule>(); }

const Module& lightGlobalFactorModule() { return moduleInstance<LightGlobalFactorModule>(); }

const Module& lightNoNormalsModule() { return moduleInstance<LightNoNormalsModule>(); }

const Module& lightNoShadowsModule() { return moduleInstance<LightNoShadowsModule>(); }

}  // namespace cli::level::options
