// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "io/policy.h"
#include "modules/module.h"
#include "modules/system/modules.h"
#include "pipeline/options.h"

#include <span>

namespace cli::modules::system {
namespace {

class OverwriteModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--overwrite"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--overwrite", "Overwrite existing outputs without asking."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {noOverwriteModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.overwrite = OverwriteMode::kAlwaysYes;
    return {};
  }
};

class NoOverwriteModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--no-overwrite"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--no-overwrite", "Skip existing outputs without asking."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {overwriteModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.overwrite = OverwriteMode::kAlwaysNo;
    return {};
  }
};

class DryRunModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--dry-run"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--dry-run", "Run normally but make output writes no-ops."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.dry_run = true;
    return {};
  }
};

class MountModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--mount"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--mount <FOLDER>",
            "Mount an Arx resource folder; repeat in decreasing priority order."};
  }

  bool repeatable() const noexcept override { return true; }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--mount: expected folder");
      return {.ok = false};
    }
    ctx.options.mounts.emplace_back(ctx.argv[++ctx.index]);
    return {};
  }
};

}  // namespace

const Module& overwriteModule() { return moduleInstance<OverwriteModule>(); }

const Module& noOverwriteModule() { return moduleInstance<NoOverwriteModule>(); }

const Module& dryRunModule() { return moduleInstance<DryRunModule>(); }

const Module& mountModule() { return moduleInstance<MountModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {
      helpModule,
      versionModule,
      kindModule,
      logLevelModule,
      overwriteModule,
      noOverwriteModule,
      dryRunModule,
      mountModule,
      resourceListingModule,
  };
  return kModules;
}

}  // namespace cli::modules::system
