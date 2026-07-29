// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class FtlReferenceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--ftl-reference"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--ftl-reference <PATH>", "Load base FTL for reference repair operations."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {
        autosizeToReferenceModule,
        snapBoneOriginsModule,
        snapActionPointsModule,
        copySyntheticSelectionAffiliationsModule,
    };
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--ftl-reference: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<ModelOptions>().reference_ftl = ctx.argv[++ctx.index];
    return {};
  }
};

class AutosizeToReferenceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--autosize-to-reference"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--autosize-to-reference", "Scale and shift model to reference landmarks."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().autosize_to_reference = true;
    return {};
  }
};

class SnapActionPointsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--snap-action-points-to-reference"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--snap-action-points-to-reference",
            "Copy exact reference action point positions by name."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().snap_action_points = true;
    return {};
  }
};

class CopySyntheticSelectionAffiliationsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--copy-synthetic-selection-affiliations"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--copy-synthetic-selection-affiliations",
            "Copy reference selection membership for synthetic vertices."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().copy_reference_affiliations = true;
    return {};
  }
};

}  // namespace

const Module& ftlReferenceModule() { return moduleInstance<FtlReferenceModule>(); }

const Module& autosizeToReferenceModule() { return moduleInstance<AutosizeToReferenceModule>(); }

const Module& snapActionPointsModule() { return moduleInstance<SnapActionPointsModule>(); }

const Module& copySyntheticSelectionAffiliationsModule() {
  return moduleInstance<CopySyntheticSelectionAffiliationsModule>();
}

}  // namespace cli::model::options
