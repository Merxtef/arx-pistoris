// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/textures/modules.h"
#include "pipeline/options.h"
#include "routes/descriptor.h"
#include "routes/types.h"

#include <span>

namespace cli::modules::textures {
namespace {

inline constexpr FormatMask kTextureFileOutputs = formatBit(Format::kFtl) | formatBit(Format::kFts) |
                                                  formatBit(Format::kDlf) | formatBit(Format::kJson) |
                                                  formatBit(Format::kObj) | formatBit(Format::kCin);

class SkipTextureExportModule final : public OutputFormatModule<kTextureFileOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--skip-texture-export"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--skip-texture-export",
            "Keep texture references but do not resolve or write texture image files."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.textures.export_files = false;
    return {};
  }
};

class InputTextureFolderModule final : public InputModifierModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--input-texture-folder"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor* route) const noexcept override {
    const char* description = route && route->kind == RouteKind::kAmbiance
                                  ? "Search this folder for textures referenced by a loose --reference-model input."
                                  : "Search this folder for texture files referenced by loose input.";
    return {HelpSection::kOptions, "--input-texture-folder <PATH>", description};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--input-texture-folder: expected argument");
      return {.ok = false};
    }
    ctx.options.textures.input_folder_specified = true;
    ctx.options.textures.input_folder = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& skipTextureExportModule() { return moduleInstance<SkipTextureExportModule>(); }

const Module& inputTextureFolderModule() { return moduleInstance<InputTextureFolderModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {skipTextureExportModule, inputTextureFolderModule};
  return kModules;
}

}  // namespace cli::modules::textures
