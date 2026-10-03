// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {
namespace {

class SignLevelModule final
    : public OutputFormatModule<formatBit(Format::kFts) | formatBit(Format::kDlf) | formatBit(Format::kJson)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--sign-level"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--sign-level <TEXT>", "Append attribution text to DLF and LLF output metadata."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--sign-level: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<LevelOptions>().signer = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& signLevelModule() { return moduleInstance<SignLevelModule>(); }

}  // namespace cli::level::options
