// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <cmath>
#include <cstring>
#include <span>

namespace cli::level::options {
namespace {

class WeldVerticesModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--weld-vertices"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--weld-vertices", "Weld nearby Level vertices within room boundaries."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {weldRadiusModule, weldMetricModule, weldDegenerateFacesModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().weld_vertices = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const float radius = context.routeOptions<LevelOptions>().vertex_welding.radius;
    if (radius > 0.0f && std::isfinite(radius)) return true;
    diagnostic(DiagnosticCode::kInvalidModuleValue, "--weld-radius must be positive and finite");
    return false;
  }
};

class WeldRadiusModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--weld-radius"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--weld-radius <UNITS=0.0001>", "Set the Level vertex welding radius."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().vertex_welding.radius, stableName(), "numeric units")};
  }
};

class WeldMetricModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--weld-metric"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--weld-metric <MODE=euclidean>",
            "Select euclidean or axis-aligned Level vertex distance."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "%s: expected euclidean or axis-aligned", stableName());
      return {.ok = false};
    }
    const char* mode = ctx.argv[++ctx.index];
    if (std::strcmp(mode, "euclidean") == 0) {
      ctx.routeOptions<LevelOptions>().vertex_welding.metric = pistoris::Level::PositionWeldMetric::kEuclidean;
    } else if (std::strcmp(mode, "axis-aligned") == 0) {
      ctx.routeOptions<LevelOptions>().vertex_welding.metric = pistoris::Level::PositionWeldMetric::kAxisAligned;
    } else {
      diagnostic(DiagnosticCode::kInvalidMode, "%s: unknown mode '%s'", stableName(), mode);
      return {.ok = false};
    }
    return {};
  }
};

class WeldDegenerateFacesModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--weld-degenerate-faces"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--weld-degenerate-faces <MODE=preserve>",
            "Preserve, reject, or discard faces collapsed by Level vertex welding."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "%s: expected preserve, reject, or discard", stableName());
      return {.ok = false};
    }
    const char* mode = ctx.argv[++ctx.index];
    if (std::strcmp(mode, "preserve") == 0) {
      ctx.routeOptions<LevelOptions>().vertex_welding.degenerate_faces =
          pistoris::Level::DegenerateFacePolicy::kPreserve;
    } else if (std::strcmp(mode, "reject") == 0) {
      ctx.routeOptions<LevelOptions>().vertex_welding.degenerate_faces = pistoris::Level::DegenerateFacePolicy::kReject;
    } else if (std::strcmp(mode, "discard") == 0) {
      ctx.routeOptions<LevelOptions>().vertex_welding.degenerate_faces =
          pistoris::Level::DegenerateFacePolicy::kDiscard;
    } else {
      diagnostic(DiagnosticCode::kInvalidMode, "%s: unknown mode '%s'", stableName(), mode);
      return {.ok = false};
    }
    return {};
  }
};

}  // namespace

const Module& weldVerticesModule() { return moduleInstance<WeldVerticesModule>(); }

const Module& weldRadiusModule() { return moduleInstance<WeldRadiusModule>(); }

const Module& weldMetricModule() { return moduleInstance<WeldMetricModule>(); }

const Module& weldDegenerateFacesModule() { return moduleInstance<WeldDegenerateFacesModule>(); }

}  // namespace cli::level::options
