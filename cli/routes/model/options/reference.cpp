// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model.hpp"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class FtlReferenceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--ftl-reference"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--ftl-reference <PATH>", "Load a reference Model for requested copy operations."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {
        snapBoneOriginsModule,
        copyBoneSelectionsModule,
        copyActionSelectionsModule,
    };
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--ftl-reference: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<ModelOptions>().reference_ftl = ctx.argv[++ctx.index];
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Model::ReferenceOptions& options = context.routeOptions<ModelOptions>().reference;
    if (options.snap_bone_origins || options.copy_bone_origin_selections || options.copy_action_point_selections)
      return true;
    diagnostic(DiagnosticCode::kInvalidModuleValue, "--ftl-reference requires a reference operation");
    return false;
  }
};

}  // namespace

const Module& ftlReferenceModule() { return moduleInstance<FtlReferenceModule>(); }

}  // namespace cli::model::options
