// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class SnapBonePositionsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--snap-bone-positions"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--snap-bone-positions", "Copy exact bone positions from the reference Model."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().reference.snap_bone_positions = true;
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
            "Replace bone selection memberships from the reference Model."};
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {inferBoneSelectionsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<ModelOptions>().reference.copy_bone_selection_memberships = true;
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

const Module& snapBonePositionsModule() { return moduleInstance<SnapBonePositionsModule>(); }

const Module& copyBoneSelectionsModule() { return moduleInstance<CopyBoneSelectionsModule>(); }

const Module& copyActionSelectionsModule() { return moduleInstance<CopyActionSelectionsModule>(); }

}  // namespace cli::model::options
