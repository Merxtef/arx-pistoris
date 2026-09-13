// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/transform/modules.h"
#include "pipeline/options.h"

#include <span>

namespace cli::modules::transform {
namespace {

class RebaseTexturesModule final : public SharedConversionModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rebase-textures"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--rebase-textures <RESOURCE-DIRECTORY>",
            "Rewrite texture paths under this resource directory before export."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--rebase-textures: expected argument");
      return {.ok = false};
    }
    ctx.options.conversion.rebase_textures = true;
    ctx.options.conversion.texture_directory = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& rebaseTexturesModule() { return moduleInstance<RebaseTexturesModule>(); }

}  // namespace cli::modules::transform
