// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "modules/sounds/modules.h"
#include "pipeline/options.h"
#include "routes/descriptor.h"
#include "routes/types.h"

#include <span>

namespace cli::modules::sounds {
namespace {

inline constexpr FormatMask kSoundFileOutputs = formatBit(Format::kAmb) | formatBit(Format::kTea) |
                                                formatBit(Format::kFtl) | formatBit(Format::kJson) |
                                                formatBit(Format::kGlb) | formatBit(Format::kCin);

class SkipSoundExportModule final : public OutputFormatModule<kSoundFileOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--skip-sound-export"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor* route) const noexcept override {
    return {HelpSection::kOptions,
            "--skip-sound-export",
            route && route->kind == RouteKind::kCinematic ? "Do not read or write referenced audio files."
                                                          : "Keep sound references without writing audio files."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.options.sounds.export_files = false;
    return {};
  }
};

class InputSoundFolderModule final : public InputModifierModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--input-sound-folder"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--input-sound-folder <PATH>",
            "Resolve format-referenced sound paths from this folder."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--input-sound-folder: expected argument");
      return {.ok = false};
    }
    ctx.options.sounds.input_folder_specified = true;
    ctx.options.sounds.input_folder = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& skipSoundExportModule() { return moduleInstance<SkipSoundExportModule>(); }

const Module& inputSoundFolderModule() { return moduleInstance<InputSoundFolderModule>(); }

std::span<const ModuleRef> rootModules() {
  static constexpr ModuleRef kModules[] = {skipSoundExportModule, inputSoundFolderModule};
  return kModules;
}

}  // namespace cli::modules::sounds
