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
    return {HelpSection::kOptions, "--dry-run", "Perform conversion and validation without writing output files."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.dry_run = true;
    return {};
  }
};

class KeepFirstResourceModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--keep-first-resource"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--keep-first-resource", "Keep the first asset's data when output resources collide."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.keep_first_resource = true;
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
            "Search an Arx resource folder; repeat in decreasing priority order."};
  }

  bool repeatable() const noexcept override { return true; }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--mount: expected folder");
      return {.ok = false};
    }
    ctx.options.read_mounts.emplace_back(ctx.argv[++ctx.index]);
    return {};
  }
};

class AutoMountModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--auto-mount"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--auto-mount", "Use standard game folders for reads and game-layout writes."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.auto_mount = true;
    return {};
  }
};

class WriteMountModule final : public SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--write-mount"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--write-mount <FOLDER>", "Write relative outputs under this folder."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--write-mount: expected folder");
      return {.ok = false};
    }
    ctx.options.write_mount = ctx.argv[++ctx.index];
    if (ctx.options.write_mount.empty()) {
      diagnostic(DiagnosticCode::kMissingArgument, "--write-mount: expected non-empty folder");
      return {.ok = false};
    }
    return {};
  }
};

}  // namespace

const Module& overwriteModule() { return moduleInstance<OverwriteModule>(); }

const Module& noOverwriteModule() { return moduleInstance<NoOverwriteModule>(); }

const Module& dryRunModule() { return moduleInstance<DryRunModule>(); }

const Module& keepFirstResourceModule() { return moduleInstance<KeepFirstResourceModule>(); }

const Module& mountModule() { return moduleInstance<MountModule>(); }

const Module& autoMountModule() { return moduleInstance<AutoMountModule>(); }

const Module& writeMountModule() { return moduleInstance<WriteMountModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {
      helpModule,
      versionModule,
      kindModule,
      logLevelModule,
      overwriteModule,
      noOverwriteModule,
      dryRunModule,
      keepFirstResourceModule,
      mountModule,
      autoMountModule,
      writeMountModule,
      resourceListingModule,
  };
  return kModules;
}

}  // namespace cli::modules::system
