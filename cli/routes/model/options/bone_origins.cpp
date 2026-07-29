// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <cstring>
#include <span>

namespace cli::model::options {
namespace {

class SnapBoneOriginsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--snap-bone-origins-to-reference"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--snap-bone-origins-to-reference <snap-origins|delta-deform|hierarchy-deform> [N]",
            "Repair bone origins using the reference model."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument,
                 "--snap-bone-origins-to-reference: expected mode snap-origins, delta-deform, or "
                 "hierarchy-deform [N]");
      return {.ok = false};
    }

    const char* mode = ctx.argv[++ctx.index];
    if (std::strcmp(mode, "snap-origins") == 0) {
      ctx.routeOptions<ModelOptions>().bone_origin_reference_mode = BoneOriginReferenceMode::kSnapOrigins;
    } else if (std::strcmp(mode, "delta-deform") == 0) {
      ctx.routeOptions<ModelOptions>().bone_origin_reference_mode = BoneOriginReferenceMode::kDeltaDeform;
    } else if (std::strcmp(mode, "hierarchy-deform") == 0) {
      ctx.routeOptions<ModelOptions>().bone_origin_reference_mode = BoneOriginReferenceMode::kHierarchyDeform;
      if (ctx.index + 1 < ctx.argc) {
        std::size_t limit = 0;
        if (modules::parseSize(ctx.argv[ctx.index + 1], limit)) {
          ctx.routeOptions<ModelOptions>().hierarchy_deform_step_limit = limit;
          ++ctx.index;
        }
      }
    } else {
      diagnostic(DiagnosticCode::kInvalidMode, "--snap-bone-origins-to-reference: unknown mode '%s'", mode);
      return {.ok = false};
    }
    return {};
  }
};

}  // namespace

const Module& snapBoneOriginsModule() { return moduleInstance<SnapBoneOriginsModule>(); }

}  // namespace cli::model::options
