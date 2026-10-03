// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "modules/module.h"
#include "modules/transform/modules.h"
#include "routes/cinematic/options.h"
#include "routes/cinematic/options/modules.h"

#include <span>

namespace cli::cinematic::options {
namespace {

ModuleParseResult parseDirectory(ModuleParseContext& ctx, DirectoryRebaseRequest& out) {
  if (ctx.index + 1 >= ctx.argc) {
    diagnostic(DiagnosticCode::kMissingArgument, "%s: expected argument", ctx.argv[ctx.index]);
    return {.ok = false};
  }
  out.requested = true;
  out.directory = ctx.argv[++ctx.index];
  return {};
}

class RebaseSfxModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rebase-sfx"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--rebase-sfx <RESOURCE-DIRECTORY>",
            "Rewrite Cinematic sound-effect paths under this resource directory."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {modules::transform::rebaseSoundsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return parseDirectory(ctx, ctx.routeOptions<CinematicOptions>().effects);
  }
};

class RebaseSpeechModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rebase-speech"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--rebase-speech <RESOURCE-DIRECTORY>",
            "Rewrite Cinematic speech paths under this resource directory."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {modules::transform::rebaseSoundsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return parseDirectory(ctx, ctx.routeOptions<CinematicOptions>().speech);
  }
};

}  // namespace

const Module& rebaseSfxModule() { return moduleInstance<RebaseSfxModule>(); }

const Module& rebaseSpeechModule() { return moduleInstance<RebaseSpeechModule>(); }

}  // namespace cli::cinematic::options
