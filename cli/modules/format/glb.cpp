// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/math.h"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "formats/modifiers.h"
#include "modules/format/modules.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "pipeline/options.h"
#include "routes/descriptor.h"
#include "routes/types.h"

#include <cmath>
#include <span>

namespace cli::modules::format {
namespace {

class GlbUnitsModule final : public FormatModifierModule<formatBit(Format::kGlb)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--glb-arx-units-per-unit"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor* route) const noexcept override {
    const char* usage = "--glb-arx-units-per-unit <UNITS>";
    if (route && route->kind == RouteKind::kModel) usage = "--glb-arx-units-per-unit <UNITS=10>";
    if (route && route->kind == RouteKind::kAmbiance) usage = "--glb-arx-units-per-unit <UNITS=10>";
    if (route && route->kind == RouteKind::kLevel) usage = "--glb-arx-units-per-unit <UNITS=100>";
    return {HelpSection::kOptions, usage, "Override Arx units represented by one GLB unit where supported."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    float units = 0.0f;
    if (ctx.index + 1 >= ctx.argc || !cli::modules::parseFloat(ctx.argv[ctx.index + 1], units) ||
        !std::isfinite(units) || units < kMinGlbArxUnitsPerUnit || units > kMaxGlbArxUnitsPerUnit) {
      diagnostic(DiagnosticCode::kInvalidNumber,
                 "%s: expected finite numeric units in [%g, %g]",
                 stableName(),
                 kMinGlbArxUnitsPerUnit,
                 kMaxGlbArxUnitsPerUnit);
      return {.ok = false};
    }
    ++ctx.index;
    ctx.options.format_modifiers.glb.arx_units_per_unit = units;
    return {};
  }
};

class GlbOffsetModule final : public FormatModifierModule<formatBit(Format::kGlb)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--glb-offset"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor* route) const noexcept override {
    if (route && route->kind != RouteKind::kLevel) return {};
    return {HelpSection::kOptions,
            "--glb-offset <X=0> <Y=0> <Z=0>",
            "Set the Arx-space origin used for Level GLB conversion."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    float values[3] = {};
    if (!cli::modules::consumeFloats(ctx, values, 3, stableName(), "a finite number") || !std::isfinite(values[0]) ||
        !std::isfinite(values[1]) || !std::isfinite(values[2])) {
      if (std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2])) return {.ok = false};
      diagnostic(DiagnosticCode::kInvalidNumber, "%s: expected three finite numeric coordinates", stableName());
      return {.ok = false};
    }
    ctx.options.format_modifiers.glb.arx_offset = pistoris::ArxVector3{values[0], values[1], values[2]};
    return {};
  }
};

}  // namespace

const Module& glbUnitsModule() { return moduleInstance<GlbUnitsModule>(); }

const Module& glbOffsetModule() { return moduleInstance<GlbOffsetModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {glbUnitsModule, glbOffsetModule};
  return kModules;
}

}  // namespace cli::modules::format
