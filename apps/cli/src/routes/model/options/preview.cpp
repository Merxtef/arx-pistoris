// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <span>

namespace cli::model::options {
namespace {

class AsLevelPreviewModule final : public OutputConverterModule<formatBit(Format::kGlb), Format::kGlb> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--as-level-preview"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--as-level-preview",
            "Export a static entity preview using the Level GLB scale (100 Arx units per GLB unit by default)."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {previewClassPathModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext&) const override { return {}; }
};

class PreviewClassPathModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--preview-class-path"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--preview-class-path <CLASS-PATH|MODEL-SELECTOR>",
            "Set the entity class path represented by a Model Level preview."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--preview-class-path: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<ModelOptions>().preview_class_path = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& asLevelPreviewModule() { return moduleInstance<AsLevelPreviewModule>(); }

const Module& previewClassPathModule() { return moduleInstance<PreviewClassPathModule>(); }

}  // namespace cli::model::options
