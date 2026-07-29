// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

ModuleParseResult consumeString(ModuleParseContext& ctx, const char* name, const char*& out) {
  if (ctx.index + 1 >= ctx.argc) {
    diagnostic(DiagnosticCode::kMissingArgument, "%s: expected argument", name);
    return {.ok = false};
  }
  out = ctx.argv[++ctx.index];
  return {};
}

class OverwriteTextureModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--overwrite-texture"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--overwrite-texture <PATH>", "Overwrite FTL texture paths when FTL is present."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return consumeString(ctx, stableName(), ctx.routeOptions<ModelOptions>().overwrite_texture);
  }
};

class RenameSelectionsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rename-selections"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--rename-selections <CSV>",
            "Rename FTL selections by position; empty CSV fields skip."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return consumeString(ctx, stableName(), ctx.routeOptions<ModelOptions>().rename_selections);
  }
};

}  // namespace

const Module& overwriteTextureModule() { return moduleInstance<OverwriteTextureModule>(); }

const Module& renameSelectionsModule() { return moduleInstance<RenameSelectionsModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {overwriteTextureModule, renameSelectionsModule, ftlReferenceModule};
  return kModules;
}

}  // namespace cli::model::options
