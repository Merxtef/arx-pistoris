// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/format.h"
#include "modules/module.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {
namespace {

class LoadPreviewsModule final : public OutputFormatModule<formatBit(Format::kGlb)> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--load-previews"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--load-previews",
            "Load referenced entity Models and attach static previews to Level GLB entities."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().load_previews = true;
    return {};
  }
};

}  // namespace

const Module& loadPreviewsModule() { return moduleInstance<LoadPreviewsModule>(); }

}  // namespace cli::level::options
