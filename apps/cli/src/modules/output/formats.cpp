// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/format.h"
#include "modules/module.h"
#include "modules/output/modules.h"
#include "pipeline/options.h"

#include <span>

namespace cli::modules::output {
namespace {

class PrettyModule final : public OutputFormatModule<formatBit(Format::kJson)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--pretty"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--pretty", "Pretty-print JSON output."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.format.pretty = true;
    return {};
  }
};

class NoCompressionModule final
    : public OutputFormatModule<formatBit(Format::kFtl) | formatBit(Format::kFts) | formatBit(Format::kDlf)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--no-compression"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--no-compression", "Write native output without PKWARE DCL compression."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.format.compress = false;
    return {};
  }
};

}  // namespace

const Module& prettyModule() { return moduleInstance<PrettyModule>(); }

const Module& noCompressionModule() { return moduleInstance<NoCompressionModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {prettyModule, noCompressionModule};
  return kModules;
}

}  // namespace cli::modules::output
