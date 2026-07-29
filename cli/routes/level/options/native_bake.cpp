// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "console/diagnostics.h"
#include "formats/format.h"
#include "modules/module.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {
namespace {

inline constexpr FormatMask kNativeLevelBakeOutputs =
    formatBit(Format::kFts) | formatBit(Format::kDlf) | formatBit(Format::kJson);
inline constexpr FormatMask kBinaryNativeLevelBakeOutputs = formatBit(Format::kFts) | formatBit(Format::kDlf);

class DlfOnlyModule final : public NativeBakeModifierModule<kNativeLevelBakeOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--dlf-only"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--dlf-only", "Write only the DLF carrier after applying Level operations."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().dlf_only = true;
    return {};
  }
};

class NoQuadReconstructionModule final : public NativeBakeModifierModule<kNativeLevelBakeOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--no-quad-reconstruction"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {
        HelpSection::kOptions, "--no-quad-reconstruction", "Disable quad reconstruction for FTS or Level JSON output."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().reconstruct_quads = false;
    return {};
  }
};

class SkipTextureExportModule final : public NativeBakeModifierModule<kBinaryNativeLevelBakeOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--skip-texture-export"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--skip-texture-export",
            "Keep native texture references but do not resolve or write texture image files."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().export_textures = false;
    return {};
  }
};

class OutputTextureFolderModule final : public NativeBakeModifierModule<kBinaryNativeLevelBakeOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--output-texture-folder"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--output-texture-folder <RESOURCE-DIRECTORY>",
            "Rebase Level texture files into this resource folder."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--output-texture-folder: expected argument");
      return {.ok = false};
    }
    LevelOptions& options = ctx.routeOptions<LevelOptions>();
    options.output_texture_folder_specified = true;
    options.output_texture_folder = ctx.argv[++ctx.index];
    return {};
  }
};

class InputTextureFolderModule final : public InputModifierModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--input-texture-folder"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--input-texture-folder <PATH>",
            "Resolve Level texture filenames from this flat input folder."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--input-texture-folder: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<LevelOptions>().input_texture_folder_specified = true;
    ctx.routeOptions<LevelOptions>().input_texture_folder = ctx.argv[++ctx.index];
    return {};
  }
};

class FtsSceneDirectoryModule final : public NativeBakeModifierModule<kBinaryNativeLevelBakeOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--fts-scene-directory"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--fts-scene-directory <RESOURCE-DIRECTORY>",
            "Set the DLF scene directory used to locate game/<directory>/fast.fts."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--fts-scene-directory: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<LevelOptions>().fts_scene_directory_specified = true;
    ctx.routeOptions<LevelOptions>().fts_scene_directory = ctx.argv[++ctx.index];
    return {};
  }
};

}  // namespace

const Module& dlfOnlyModule() { return moduleInstance<DlfOnlyModule>(); }

const Module& noQuadReconstructionModule() { return moduleInstance<NoQuadReconstructionModule>(); }

const Module& skipTextureExportModule() { return moduleInstance<SkipTextureExportModule>(); }

const Module& outputTextureFolderModule() { return moduleInstance<OutputTextureFolderModule>(); }

const Module& inputTextureFolderModule() { return moduleInstance<InputTextureFolderModule>(); }

const Module& ftsSceneDirectoryModule() { return moduleInstance<FtsSceneDirectoryModule>(); }

}  // namespace cli::level::options
