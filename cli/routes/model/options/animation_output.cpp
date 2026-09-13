// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/format.h"
#include "modules/module.h"
#include "pipeline/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class AllowEmptyAnimationModule final : public OutputFormatModule<formatBit(Format::kFtl) | formatBit(Format::kJson)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--allow-empty-animation"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--allow-empty-animation",
            "Write Animation sidecars with zero groups and no root motion, footsteps, or referenced sounds."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.format.allow_empty_animation = true;
    return {};
  }
};

}  // namespace

const Module& allowEmptyAnimationModule() { return moduleInstance<AllowEmptyAnimationModule>(); }

}  // namespace cli::model::options
