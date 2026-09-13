// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class InferBoneSelectionsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--infer-bone-selections"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--infer-bone-selections",
            "Infer bone-origin selection memberships from directly owned vertices."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().infer_bone_selections = true;
    return {};
  }
};

}  // namespace

const Module& inferBoneSelectionsModule() { return moduleInstance<InferBoneSelectionsModule>(); }

}  // namespace cli::model::options
