// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class SnapBoneOriginsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--snap-bone-origins"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--snap-bone-origins", "Copy exact bone-origin positions from the reference Model."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().reference.snap_bone_origins = true;
    return {};
  }
};

class CopyBoneSelectionsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--copy-bone-selections"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--copy-bone-selections",
            "Replace bone-origin selection memberships from the reference Model."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {inferBoneSelectionsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().reference.copy_bone_origin_selections = true;
    return {};
  }
};

class CopyActionSelectionsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--copy-action-selections"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--copy-action-selections",
            "Replace action-point selection memberships from the reference Model."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().reference.copy_action_point_selections = true;
    return {};
  }
};

}  // namespace

const Module& snapBoneOriginsModule() { return moduleInstance<SnapBoneOriginsModule>(); }

const Module& copyBoneSelectionsModule() { return moduleInstance<CopyBoneSelectionsModule>(); }

const Module& copyActionSelectionsModule() { return moduleInstance<CopyActionSelectionsModule>(); }

}  // namespace cli::model::options
