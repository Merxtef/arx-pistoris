// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/options/modules.h"

#include <span>

namespace cli::ambiance::options {
namespace {

class ReferenceModelModule final : public OutputFormatModule<formatBit(Format::kGlb)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--reference-model"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--reference-model <MODEL-PATH>",
            "Embed a Model reference and align Ambiance to its first view_attach."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--reference-model: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<AmbianceOptions>().reference_model = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& referenceModelModule() { return moduleInstance<ReferenceModelModule>(); }

}  // namespace cli::ambiance::options
