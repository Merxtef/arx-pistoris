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
    return {HelpSection::kOptions, "--dlf-only", "Write only DLF output after applying Level operations."};
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

class FtsSceneDirectoryModule final : public NativeBakeModifierModule<kBinaryNativeLevelBakeOutputs> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--dlf-scene-directory"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--dlf-scene-directory <RESOURCE-DIRECTORY>",
            "Set the DLF scene directory used to locate game/<directory>/fast.fts."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--dlf-scene-directory: expected argument");
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

const Module& ftsSceneDirectoryModule() { return moduleInstance<FtsSceneDirectoryModule>(); }

}  // namespace cli::level::options
