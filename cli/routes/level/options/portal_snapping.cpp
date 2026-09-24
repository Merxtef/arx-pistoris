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

class SnapToPortalsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--snap-to-portals"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--snap-to-portals", "Snap nearby Level geometry onto portal surfaces."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {portalSnapRadiusModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().snap_to_portals = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const float radius = context.routeOptions<LevelOptions>().portal_snapping.radius;
    if (radius > 0.0f && std::isfinite(radius)) return true;
    diagnostic(DiagnosticCode::kInvalidModuleValue, "--portal-snap-radius must be positive and finite");
    return false;
  }
};

class PortalSnapRadiusModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--portal-snap-radius"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--portal-snap-radius <UNITS=1>", "Set the maximum distance for Level portal snapping."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().portal_snapping.radius, stableName(), "numeric units")};
  }
};

}  // namespace

const Module& snapToPortalsModule() { return moduleInstance<SnapToPortalsModule>(); }

const Module& portalSnapRadiusModule() { return moduleInstance<PortalSnapRadiusModule>(); }

}  // namespace cli::level::options
