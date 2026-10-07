// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/save.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/format.h"
#include "modules/module.h"
#include "pipeline/execution_context.h"
#include "resources/layout.h"
#include "resources/level_image_io.h"
#include "resources/resource_output.h"
#include "resources/selector.h"
#include "resources/texture_io.h"
#include "routes/conversion_failure.h"
#include "routes/level/invocation.h"
#include "routes/level/native_carriers.h"
#include "routes/level/operations.h"
#include "routes/level/options/modules.h"
#include "routes/level/state.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::level {
namespace {

template <typename Diagnostics>
operations::OperationDiagnostics makeDiagnostics() {
  return operations::OperationDiagnostics{std::in_place_type<Diagnostics>};
}

enum class NativeEncoding : std::uint8_t {
  kBinary,
  kJson,
};

bool outputFailure(const char* what, ArxReturnCode rc) {
  return conversionOutputFailure(DiagnosticCode::kLevelOutputFailed, what, rc);
}

template <class Result>
bool outputFailure(const char* what, const Result& result) {
  return conversionOutputFailure(DiagnosticCode::kLevelOutputFailed, what, result);
}

bool sameTarget(const OutputTarget& left, const OutputTarget& right) noexcept {
  return left.address == right.address && left.path == right.path;
}

void addLevelData(ResourceOutputPlan& plan, const OutputTarget& target, std::vector<std::uint8_t> data,
                  ResourceAssetId asset, const Invocation& invocation) {
  if (sameTarget(target, invocation.output))
    plan.addPrimaryOwned(target, std::move(data), asset);
  else
    plan.addOwned(ResourceFileKind::kData, target, std::move(data), asset);
}

bool writeSingleLevelOutput(std::vector<std::uint8_t> data, const ExecutionContext& execution,
                            const Invocation& invocation) {
  ResourceOutputPlan plan;
  const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kLevel, invocation.output.path);
  plan.addPrimaryOwned(invocation.output, std::move(data), asset);
  return execution.resourceOutputs().resolve(plan) && execution.resourceOutputs().write(plan);
}

bool prepareRawResourceOutputs(ResourceOutputPlan& plan, std::span<const pistoris::NativeTextureFile> texture_files,
                               const LoadedLevelImages& images, GeneratedLevelImages& generated,
                               const ExecutionContext& execution, const Invocation& invocation) {
  const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kLevel, invocation.output.path);
  if (!addNativeTextureFileOutputs(plan,
                                   execution.io(),
                                   invocation.native_output.textures,
                                   texture_files,
                                   asset,
                                   DiagnosticCode::kLevelOutputFailed,
                                   "Level")) {
    return false;
  }
  if (!invocation.options.dlf_only &&
      !addDirectLevelImageOutputs(plan, invocation.image_input, invocation.image_output, images, generated, asset))
    return false;
  return true;
}

bool prepareProjectedResourceOutputs(ResourceOutputPlan& plan,
                                     std::span<const pistoris::NativeTextureFile> texture_files,
                                     const pistoris::Level& level, GeneratedLevelImages& generated,
                                     const ExecutionContext& execution, const Invocation& invocation) {
  const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kLevel, invocation.output.path);
  if (!addNativeTextureFileOutputs(plan,
                                   execution.io(),
                                   invocation.native_output.textures,
                                   texture_files,
                                   asset,
                                   DiagnosticCode::kLevelOutputFailed,
                                   "Level")) {
    return false;
  }
  if (!invocation.options.dlf_only &&
      !addIntermediateLevelImageOutputs(plan, invocation.image_output, level, generated, asset)) {
    return false;
  }
  return true;
}

void logDetachedLayout(const NativeOutput& output) {
  if (output.fts.layout != ResourceLayout::kLoose) return;
  log(ARX_LOG_INFO,
      "loose Level output uses physical FTS '%s'; DLF runtime FTS reference is '%s'",
      output.fts.path.c_str(),
      output.runtime_fts_path.c_str());
}

std::string jsonLevelName(const Invocation& invocation) {
  return "level" + std::to_string(invocation.json_output.level);
}

bool prepareFullOutput(IntermediateLevel& source, const Invocation& invocation) {
  const auto compacted = source.level.compactTextures();
  if (!compacted) return outputFailure("Level texture compaction", compacted);
  if (*compacted != 0) log(ARX_LOG_INFO, "removed %zu unused Level texture(s)", *compacted);

  if (invocation.texture_rebase.enabled) {
    const auto rebased = source.level.rebaseTexturePaths(invocation.texture_rebase.directory);
    if (!rebased) return outputFailure("Level texture rebasing", rebased);
  }
  return true;
}

bool bakeDlf(IntermediateLevel& source, const Invocation& invocation, NativeEncoding encoding, NativeLevelFiles& out) {
  const std::string level_name = encoding == NativeEncoding::kJson ? jsonLevelName(invocation) : std::string{};
  out.text_mode = encoding == NativeEncoding::kJson ? pistoris::NativeTextMode::kUtf8 : invocation.native_text_mode;
  pistoris::Level::DlfBakeOptions options;
  options.level_name = level_name;
  options.target_fts_offset = source.source_fts_offset;
  options.text_mode = out.text_mode;
  if (encoding == NativeEncoding::kBinary || invocation.options.fts_scene_directory_specified)
    options.dlf_scene_path = invocation.native_output.dlf_scene_path;

  auto dlf = source.level.bakeDlf(options);
  if (!dlf) return outputFailure("Level DLF output", dlf);
  out.dlf = std::move(*dlf);
  return true;
}

bool bakeNativeBundle(IntermediateLevel& source, const Invocation& invocation, NativeEncoding encoding,
                      bool include_texture_files, NativeLevelFiles& out,
                      std::vector<pistoris::NativeTextureFile>& texture_files) {
  const std::string level_name = encoding == NativeEncoding::kJson ? jsonLevelName(invocation) : std::string{};
  out.text_mode = encoding == NativeEncoding::kJson ? pistoris::NativeTextMode::kUtf8 : invocation.native_text_mode;
  pistoris::Level::NativeBakeOptions options;
  options.level_name = level_name;
  options.reconstruct_quads = invocation.options.reconstruct_quads;
  options.include_texture_files = include_texture_files;
  options.text_mode = out.text_mode;
  if (encoding == NativeEncoding::kBinary || invocation.options.fts_scene_directory_specified)
    options.dlf_scene_path = invocation.native_output.dlf_scene_path;

  auto bundle = source.level.bakeNativeBundle(options);
  if (!bundle) return outputFailure("Level native bundle output", bundle);
  out.fts = std::move(bundle->fts);
  out.llf = std::move(bundle->llf);
  out.dlf = std::move(bundle->dlf);
  texture_files = std::move(bundle->texture_files);
  return true;
}

bool addBinaryFiles(ResourceOutputPlan& plan, NativeLevelFiles& files, const Invocation& invocation) {
  const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kLevel, invocation.output.path);
  if (files.fts) {
    auto bytes = pistoris::writeFts(*files.fts, invocation.format.compress);
    if (!bytes) return outputFailure("FTS output", bytes);
    addLevelData(plan, invocation.native_output.fts, std::move(*bytes), asset, invocation);
  }
  if (files.llf) {
    const pistoris::LlfWriteOptions options{invocation.options.signer};
    auto bytes = pistoris::writeLlf(*files.llf, options, invocation.format.compress);
    if (!bytes) return outputFailure("LLF output", bytes);
    addLevelData(plan, invocation.native_output.llf, std::move(*bytes), asset, invocation);
  }
  if (files.dlf) {
    const pistoris::DlfWriteOptions options{nullptr, invocation.options.signer};
    auto bytes = pistoris::writeDlf(*files.dlf, options, invocation.format.compress);
    if (!bytes) return outputFailure("DLF output", bytes);
    addLevelData(plan, invocation.native_output.dlf, std::move(*bytes), asset, invocation);
  }
  if (files.fts) logDetachedLayout(invocation.native_output);
  return true;
}

bool addJsonFiles(ResourceOutputPlan& plan, NativeLevelFiles& files, const Invocation& invocation) {
  if (files.dlf) applyLevelNumber(invocation.json_output.level, *files.dlf);

  const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kLevel, invocation.output.path);
  if (files.fts) {
    auto json =
        pistoris::toFtsJson(*files.fts, invocation.json_output.level, invocation.format.pretty, files.text_mode);
    if (!json) return outputFailure("FTS JSON output", json);
    addLevelData(
        plan, invocation.json_output.fts, std::vector<std::uint8_t>(json->begin(), json->end()), asset, invocation);
  }
  if (files.llf) {
    auto json = pistoris::toLlfJson(*files.llf, invocation.format.pretty, invocation.options.signer);
    if (!json) return outputFailure("LLF JSON output", json);
    addLevelData(
        plan, invocation.json_output.llf, std::vector<std::uint8_t>(json->begin(), json->end()), asset, invocation);
  }
  if (files.dlf) {
    auto json = pistoris::toDlfJson(*files.dlf, invocation.format.pretty, invocation.options.signer, files.text_mode);
    if (!json) return outputFailure("DLF JSON output", json);
    addLevelData(
        plan, invocation.json_output.dlf, std::vector<std::uint8_t>(json->begin(), json->end()), asset, invocation);
  }
  return true;
}

void prepareNativeTextureOutput(const NativeLevelFiles& files, const ExecutionContext& execution,
                                const Invocation& invocation, std::vector<pistoris::NativeTextureFile>& texture_files) {
  if (!invocation.texture_options.export_files || !files.fts) return;
  texture_files = files.texture_files;
  if (invocation.texture_options.input_folder_specified)
    loadNativeTextureFiles(*files.fts, files.text_mode, execution.io(), invocation.textures, texture_files);
}

bool writeNativeBinaryNative(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> texture_files;
  prepareNativeTextureOutput(files, execution, invocation, texture_files);
  GeneratedLevelImages generated;
  ResourceOutputPlan resources;
  if (!prepareRawResourceOutputs(resources, texture_files, files.images, generated, execution, invocation) ||
      !addBinaryFiles(resources, files, invocation) || !execution.resourceOutputs().resolve(resources))
    return false;
  return execution.resourceOutputs().write(resources);
}

bool writeNativeBinaryIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                   const Invocation& invocation, const operations::OperationDiagnostics&) {
  NativeLevelFiles files;
  std::vector<pistoris::NativeTextureFile> texture_files;
  if (invocation.options.dlf_only) {
    if (!bakeDlf(source, invocation, NativeEncoding::kBinary, files)) return false;
  } else {
    const bool include_texture_files = invocation.texture_options.export_files;
    if (!prepareFullOutput(source, invocation)) return false;
    if (!bakeNativeBundle(source, invocation, NativeEncoding::kBinary, include_texture_files, files, texture_files))
      return false;
  }
  GeneratedLevelImages projected;
  ResourceOutputPlan resources;
  if (!prepareProjectedResourceOutputs(resources, texture_files, source.level, projected, execution, invocation))
    return false;
  if (!addBinaryFiles(resources, files, invocation) || !execution.resourceOutputs().resolve(resources)) return false;
  return execution.resourceOutputs().write(resources);
}

bool writeJsonNative(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> texture_files;
  prepareNativeTextureOutput(files, execution, invocation, texture_files);
  GeneratedLevelImages generated;
  ResourceOutputPlan resources;
  if (!prepareRawResourceOutputs(resources, texture_files, files.images, generated, execution, invocation) ||
      !addJsonFiles(resources, files, invocation) || !execution.resourceOutputs().resolve(resources))
    return false;
  return execution.resourceOutputs().write(resources);
}

bool writeJsonIntermediate(IntermediateLevel& source, const ExecutionContext& execution, const Invocation& invocation,
                           const operations::OperationDiagnostics&) {
  NativeLevelFiles files;
  std::vector<pistoris::NativeTextureFile> texture_files;
  if (invocation.options.dlf_only) {
    if (!bakeDlf(source, invocation, NativeEncoding::kJson, files)) return false;
  } else {
    const bool include_texture_files = invocation.texture_options.export_files;
    if (!prepareFullOutput(source, invocation)) return false;
    if (!bakeNativeBundle(source, invocation, NativeEncoding::kJson, include_texture_files, files, texture_files))
      return false;
  }
  GeneratedLevelImages projected;
  ResourceOutputPlan resources;
  if (!prepareProjectedResourceOutputs(resources, texture_files, source.level, projected, execution, invocation))
    return false;
  if (!addJsonFiles(resources, files, invocation) || !execution.resourceOutputs().resolve(resources)) return false;
  return execution.resourceOutputs().write(resources);
}

bool writeDebugCellsNative(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  if (!files.fts) {
    diagnostic(DiagnosticCode::kLevelOutputFailed, "Debug-cell output requires FTS data");
    return false;
  }
  std::vector<std::uint8_t> out;
  const ArxReturnCode rc =
      pistoris::level_debug::exportFtsCellsDebugGlb(*files.fts, out, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeSingleLevelOutput(std::move(out), execution, invocation);
}

bool writeDebugCellsIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                 const Invocation& invocation, const operations::OperationDiagnostics&) {
  pistoris::Level::NativeBakeOptions options;
  options.level_name = "debug";
  options.reconstruct_quads = false;
  options.include_texture_files = false;
  options.text_mode = pistoris::NativeTextMode::kUtf8;
  auto bundle = source.level.bakeNativeBundle(options);
  if (!bundle) return outputFailure("Level native bundle output", bundle);

  std::vector<std::uint8_t> out;
  const ArxReturnCode rc =
      pistoris::level_debug::exportFtsCellsDebugGlb(bundle->fts, out, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeSingleLevelOutput(std::move(out), execution, invocation);
}

bool writeNavigationIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                 const Invocation& invocation, const operations::OperationDiagnostics& diagnostics) {
  std::vector<std::uint8_t> out;
  const auto* navigation = std::get_if<pistoris::level_debug::NavigationDiagnostics>(&diagnostics);
  const ArxReturnCode rc =
      pistoris::level_debug::exportNavigationDebugGlb(source.level, out, navigation, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeSingleLevelOutput(std::move(out), execution, invocation);
}

bool writeRoomDistancesIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                    const Invocation& invocation, const operations::OperationDiagnostics& diagnostics) {
  std::vector<std::uint8_t> out;
  const auto* room_distances = std::get_if<pistoris::level_debug::RoomDistanceGenDiagnostics>(&diagnostics);
  const ArxReturnCode rc = pistoris::level_debug::exportRoomDistanceDebugGlb(
      source.level, out, room_distances, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeSingleLevelOutput(std::move(out), execution, invocation);
}

bool writeGlbIntermediate(IntermediateLevel& source, const ExecutionContext& execution, const Invocation& invocation,
                          const operations::OperationDiagnostics&) {
  if (!prepareFullOutput(source, invocation)) return false;
  std::vector<const pistoris::Model*> previews;
  previews.reserve(invocation.model_previews.size());
  for (const auto& preview : invocation.model_previews) previews.push_back(preview.get());
  ArxLevelModelPreviewReport report{};
  auto out = source.level.exportGlb(previews, invocation.options.glb_export, &report);
  if (!out) return outputFailure("GLB output", out);
  if (report.previewed_entities != 0)
    log(ARX_LOG_INFO, "attached Model previews to %zu Level entity instance(s)", report.previewed_entities);
  GeneratedLevelImages projected;
  ResourceOutputPlan resources;
  if (!prepareProjectedResourceOutputs(resources, {}, source.level, projected, execution, invocation)) return false;
  const ResourceAssetId asset = resources.addAsset(ResourceAssetKind::kLevel, invocation.output.path);
  resources.addPrimaryOwned(invocation.output, std::move(*out), asset);
  if (!execution.resourceOutputs().resolve(resources)) return false;
  return execution.resourceOutputs().write(resources);
}

}  // namespace

const OutputConverterDescriptor* outputConverterDescriptor(Format output, const Module* module) {
  static constexpr OutputConverterDescriptor kNativeBinary{
      .write_native = writeNativeBinaryNative,
      .write_intermediate = writeNativeBinaryIntermediate,
  };
  static constexpr OutputConverterDescriptor kJson{
      .write_native = writeJsonNative,
      .write_intermediate = writeJsonIntermediate,
  };
  static constexpr OutputConverterDescriptor kGlb{
      .write_intermediate = writeGlbIntermediate,
      .supports_model_previews = true,
  };
  static constexpr OutputConverterDescriptor kDebugCells{
      .write_native = writeDebugCellsNative,
      .write_intermediate = writeDebugCellsIntermediate,
  };
  static constexpr OutputConverterDescriptor kDebugNavigation{
      .write_intermediate = writeNavigationIntermediate,
      .create_diagnostics = makeDiagnostics<pistoris::level_debug::NavigationDiagnostics>,
  };
  static constexpr OutputConverterDescriptor kDebugRoomDistances{
      .write_intermediate = writeRoomDistancesIntermediate,
      .create_diagnostics = makeDiagnostics<pistoris::level_debug::RoomDistanceGenDiagnostics>,
  };

  if (!module) {
    switch (output) {
      case Format::kFts:
      case Format::kDlf:
        return &kNativeBinary;
      case Format::kJson:
        return &kJson;
      case Format::kGlb:
        return &kGlb;
      default:
        return nullptr;
    }
  }

  if (module == &options::debugCellsModule()) return output == Format::kGlb ? &kDebugCells : nullptr;
  if (module == &options::debugNavigationModule()) return output == Format::kGlb ? &kDebugNavigation : nullptr;
  if (module == &options::debugRoomDistancesModule()) {
    return output == Format::kGlb ? &kDebugRoomDistances : nullptr;
  }
  return nullptr;
}

bool writeNativeOutput(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  const OutputConverterDescriptor* converter = invocation.output_converter;
  if (!converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Unsupported Level output converter");
    return false;
  }
  if (!converter->write_native) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Level output converter requires intermediate data");
    return false;
  }
  return converter->write_native(files, execution, invocation);
}

bool writeIntermediateOutput(IntermediateLevel& level, const ExecutionContext& execution, const Invocation& invocation,
                             const operations::OperationDiagnostics& diagnostics) {
  const OutputConverterDescriptor* converter = invocation.output_converter;
  if (!converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Unsupported Level output converter");
    return false;
  }
  if (!converter->write_intermediate) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Level output converter requires native data");
    return false;
  }
  return converter->write_intermediate(level, execution, invocation, diagnostics);
}

}  // namespace cli::level
