// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "modules/transform/modules.h"
#include "pipeline/options.h"

#include <cmath>
#include <span>

namespace cli::modules::transform {
namespace {

bool finite3(const float (&values)[3]) {
  return std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2]);
}

class RotateModule final : public SharedConversionModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rotate"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--rotate <RX=0> <RY=0> <RZ=0>", "Apply Euler XYZ rotation in degrees."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (!consumeFloats(ctx, ctx.options.conversion.rotate, 3, "--rotate", "a number")) return {.ok = false};
    ctx.options.conversion.has_xform = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const ParsedOptions& options = context.options;
    if (finite3(options.conversion.rotate)) return true;
    diagnostic(DiagnosticCode::kInvalidModuleValue, "--rotate values must be finite");
    return false;
  }
};

class ScaleModule final : public SharedConversionModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--scale"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--scale <S=1>", "Apply uniform scale."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ParsedOptions& options = ctx.options;
    if (ctx.index + 1 >= ctx.argc || !parseFloat(ctx.argv[ctx.index + 1], options.conversion.scale)) {
      diagnostic(DiagnosticCode::kScaleInvalid, "--scale: expected one uniform numeric value");
      return {.ok = false};
    }
    options.conversion.has_xform = true;
    ++ctx.index;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const ParsedOptions& options = context.options;
    if (std::isfinite(options.conversion.scale) && options.conversion.scale > 0.0f) return true;
    diagnostic(DiagnosticCode::kInvalidModuleValue, "--scale must be positive and finite");
    return false;
  }
};

class OffsetModule final : public SharedConversionModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--offset"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--offset <OX=0> <OY=0> <OZ=0>", "Apply translation."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (!consumeFloats(ctx, ctx.options.conversion.offset, 3, "--offset", "a number")) return {.ok = false};
    ctx.options.conversion.has_xform = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const ParsedOptions& options = context.options;
    if (finite3(options.conversion.offset)) return true;
    diagnostic(DiagnosticCode::kInvalidModuleValue, "--offset values must be finite");
    return false;
  }
};

}  // namespace

const Module& rotateModule() { return moduleInstance<RotateModule>(); }

const Module& scaleModule() { return moduleInstance<ScaleModule>(); }

const Module& offsetModule() { return moduleInstance<OffsetModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {
      rotateModule, scaleModule, offsetModule, rebaseTexturesModule, rebaseSoundsModule};
  return kModules;
}

}  // namespace cli::modules::transform
