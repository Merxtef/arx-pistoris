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
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/format.h"
#include "io/service.h"
#include "modules/module.h"
#include "pipeline/execution_context.h"
#include "resources/layout.h"
#include "resources/level_image_io.h"
#include "resources/output.h"
#include "resources/resource_output.h"
#include "resources/selector.h"
#include "resources/texture_io.h"
#include "routes/level/invocation.h"
#include "routes/level/native_carriers.h"
#include "routes/level/operations.h"
#include "routes/level/options/modules.h"
#include "routes/level/state.h"

#include <concepts>
#include <cstddef>
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
  diagnostic(DiagnosticCode::kLevelOutputFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool writeBytes(IoService& io, const OutputTarget& target, const std::vector<std::uint8_t>& data) {
  return writeOutput(io, target, data.data(), data.size());
}

bool writeText(IoService& io, const OutputTarget& target, const std::string& data) {
  return writeOutput(io, target, data.data(), data.size());
}

void reserveLevelOutputs(ResourceOutputPlan& plan, const Invocation& invocation) {
  switch (invocation.output.format) {
    case Format::kFts:
    case Format::kDlf:
      plan.reserveOutput(invocation.native_output.fts);
      plan.reserveOutput(invocation.native_output.llf);
      plan.reserveOutput(invocation.native_output.dlf);
      break;
    case Format::kJson:
      plan.reserveOutput(invocation.json_output.fts);
      plan.reserveOutput(invocation.json_output.llf);
      plan.reserveOutput(invocation.json_output.dlf);
      break;
    default:
      plan.reserveOutput(invocation.output);
      break;
  }
}

bool prepareRawResourceOutputs(ResourceOutputPlan& plan, std::span<const pistoris::NativeTextureFile> texture_files,
                               GeneratedLevelImages& generated, const ExecutionContext& execution,
                               const Invocation& invocation) {
  reserveLevelOutputs(plan, invocation);
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
      !addDirectLevelImageOutputs(
          plan, invocation.image_input, invocation.image_output, invocation.loaded_images, generated, asset))
    return false;
  return execution.resourceOutputs().resolve(plan);
}

bool prepareProjectedResourceOutputs(ResourceOutputPlan& plan,
                                     std::span<const pistoris::NativeTextureFile> texture_files,
                                     const pistoris::Level& level, GeneratedLevelImages& generated,
                                     const ExecutionContext& execution, const Invocation& invocation) {
  reserveLevelOutputs(plan, invocation);
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
  return execution.resourceOutputs().resolve(plan);
}

template <typename Native>
bool writeNativeJson(IoService& io, const OutputTarget& target, const Native& native, bool pretty,
                     const char* description, std::string_view signer = {}) {
  std::string json;
  ArxReturnCode rc = ARX_OK;
  if constexpr (std::same_as<Native, pistoris::Dlf> || std::same_as<Native, pistoris::Llf>) {
    rc = pistoris::toJson(native, json, pretty, signer);
  } else {
    rc = pistoris::toJson(native, json, pretty);
  }
  if (rc != ARX_OK) return outputFailure(description, rc);
  return writeText(io, target, json);
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
  std::size_t removed = 0;
  ArxReturnCode rc = source.level.compactTextures(&removed);
  if (rc != ARX_OK) return outputFailure("Level texture compaction", rc);
  if (removed != 0) log(ARX_LOG_INFO, "removed %zu unused Level texture(s)", removed);

  if (invocation.rebase_textures) {
    rc = source.level.rebaseTexturePaths(invocation.texture_rebase_directory);
    if (rc != ARX_OK) return outputFailure("Level texture rebasing", rc);
  }
  return true;
}

bool bakeDlf(IntermediateLevel& source, const Invocation& invocation, NativeEncoding encoding, NativeLevelFiles& out) {
  const std::string level_name = encoding == NativeEncoding::kJson ? jsonLevelName(invocation) : std::string{};
  pistoris::Level::DlfBakeOptions options;
  options.level_name = level_name;
  options.target_fts_offset = source.source_fts_offset;
  if (encoding == NativeEncoding::kBinary || invocation.options.fts_scene_directory_specified)
    options.dlf_scene_path = invocation.native_output.dlf_scene_path;

  pistoris::Dlf dlf;
  const ArxReturnCode rc = source.level.bakeDlf(options, dlf);
  if (rc != ARX_OK) return outputFailure("Level DLF output", rc);
  out.dlf = std::move(dlf);
  return true;
}

bool bakeNativeBundle(IntermediateLevel& source, const Invocation& invocation, NativeEncoding encoding,
                      bool include_texture_files, NativeLevelFiles& out,
                      std::vector<pistoris::NativeTextureFile>& texture_files) {
  const std::string level_name = encoding == NativeEncoding::kJson ? jsonLevelName(invocation) : std::string{};
  pistoris::Level::NativeBakeOptions options;
  options.level_name = level_name;
  options.reconstruct_quads = invocation.options.reconstruct_quads;
  options.textures.include_files = include_texture_files;
  if (encoding == NativeEncoding::kBinary || invocation.options.fts_scene_directory_specified)
    options.dlf_scene_path = invocation.native_output.dlf_scene_path;

  pistoris::NativeLevelBundle bundle;
  const ArxReturnCode rc = source.level.bakeNativeBundle(options, bundle);
  if (rc != ARX_OK) return outputFailure("Level native bundle output", rc);
  out.fts = std::move(bundle.fts);
  out.llf = std::move(bundle.llf);
  out.dlf = std::move(bundle.dlf);
  texture_files = std::move(bundle.texture_files);
  return true;
}

bool writeBinaryFiles(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  IoService& io = execution.io();
  if (files.fts) {
    std::vector<std::uint8_t> bytes;
    const ArxReturnCode rc = pistoris::writeFts(*files.fts, bytes, invocation.format.compress);
    if (rc != ARX_OK) return outputFailure("FTS output", rc);
    if (!writeBytes(io, invocation.native_output.fts, bytes)) return false;
  }
  if (files.llf) {
    std::vector<std::uint8_t> bytes;
    const pistoris::LlfWriteOptions options{invocation.options.signer};
    const ArxReturnCode rc = pistoris::writeLlf(*files.llf, options, bytes, invocation.format.compress);
    if (rc != ARX_OK) return outputFailure("LLF output", rc);
    if (!writeBytes(io, invocation.native_output.llf, bytes)) return false;
  }
  if (files.dlf) {
    std::vector<std::uint8_t> bytes;
    const pistoris::DlfWriteOptions options{nullptr, invocation.options.signer};
    const ArxReturnCode rc = pistoris::writeDlf(*files.dlf, options, bytes, invocation.format.compress);
    if (rc != ARX_OK) return outputFailure("DLF output", rc);
    if (!writeBytes(io, invocation.native_output.dlf, bytes)) return false;
  }
  if (files.fts) logDetachedLayout(invocation.native_output);
  return true;
}

bool writeJsonFiles(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  if (files.fts) {
    applyLevelNumber(invocation.json_output.level, *files.fts, files.dlf ? &*files.dlf : nullptr);
  } else if (files.dlf) {
    applyLevelNumber(invocation.json_output.level, *files.dlf);
  }

  IoService& io = execution.io();
  if (files.fts &&
      !writeNativeJson(io, invocation.json_output.fts, *files.fts, invocation.format.pretty, "FTS JSON output")) {
    return false;
  }
  if (files.llf && !writeNativeJson(io,
                                    invocation.json_output.llf,
                                    *files.llf,
                                    invocation.format.pretty,
                                    "LLF JSON output",
                                    invocation.options.signer)) {
    return false;
  }
  if (files.dlf && !writeNativeJson(io,
                                    invocation.json_output.dlf,
                                    *files.dlf,
                                    invocation.format.pretty,
                                    "DLF JSON output",
                                    invocation.options.signer)) {
    return false;
  }
  return true;
}

void prepareNativeTextureOutput(const NativeLevelFiles& files, const ExecutionContext& execution,
                                const Invocation& invocation, std::vector<pistoris::NativeTextureFile>& texture_files) {
  if (invocation.texture_options.export_files && files.fts)
    loadNativeTextureFiles(*files.fts, execution.io(), invocation.textures, texture_files);
}

bool writeNativeBinaryNative(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> texture_files;
  prepareNativeTextureOutput(files, execution, invocation, texture_files);
  GeneratedLevelImages generated;
  ResourceOutputPlan resources;
  if (!prepareRawResourceOutputs(resources, texture_files, generated, execution, invocation)) return false;
  return writeBinaryFiles(files, execution, invocation) && execution.resourceOutputs().write(resources);
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
  return writeBinaryFiles(files, execution, invocation) && execution.resourceOutputs().write(resources);
}

bool writeJsonNative(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> texture_files;
  prepareNativeTextureOutput(files, execution, invocation, texture_files);
  GeneratedLevelImages generated;
  ResourceOutputPlan resources;
  if (!prepareRawResourceOutputs(resources, texture_files, generated, execution, invocation)) return false;
  return writeJsonFiles(files, execution, invocation) && execution.resourceOutputs().write(resources);
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
  return writeJsonFiles(files, execution, invocation) && execution.resourceOutputs().write(resources);
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
  return writeOutput(execution.io(), invocation.output, out.data(), out.size());
}

bool writeDebugCellsIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                 const Invocation& invocation, const operations::OperationDiagnostics&) {
  pistoris::Level::NativeBakeOptions options;
  options.level_name = "debug";
  options.reconstruct_quads = false;
  options.textures.include_files = false;
  pistoris::NativeLevelBundle bundle;
  ArxReturnCode rc = source.level.bakeNativeBundle(options, bundle);
  if (rc != ARX_OK) return outputFailure("Level native bundle output", rc);

  std::vector<std::uint8_t> out;
  rc = pistoris::level_debug::exportFtsCellsDebugGlb(bundle.fts, out, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeOutput(execution.io(), invocation.output, out.data(), out.size());
}

bool writeNavigationIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                 const Invocation& invocation, const operations::OperationDiagnostics& diagnostics) {
  std::vector<std::uint8_t> out;
  const auto* navigation = std::get_if<pistoris::level_debug::NavigationDiagnostics>(&diagnostics);
  const ArxReturnCode rc =
      pistoris::level_debug::exportNavigationDebugGlb(source.level, out, navigation, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeOutput(execution.io(), invocation.output, out.data(), out.size());
}

bool writeRoomDistancesIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                    const Invocation& invocation, const operations::OperationDiagnostics& diagnostics) {
  std::vector<std::uint8_t> out;
  const auto* room_distances = std::get_if<pistoris::level_debug::RoomDistanceGenDiagnostics>(&diagnostics);
  const ArxReturnCode rc = pistoris::level_debug::exportRoomDistanceDebugGlb(
      source.level, out, room_distances, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeOutput(execution.io(), invocation.output, out.data(), out.size());
}

bool writeGlbIntermediate(IntermediateLevel& source, const ExecutionContext& execution, const Invocation& invocation,
                          const operations::OperationDiagnostics&) {
  if (!prepareFullOutput(source, invocation)) return false;
  std::vector<const pistoris::Model*> previews;
  previews.reserve(invocation.model_previews.size());
  for (const auto& preview : invocation.model_previews) previews.push_back(preview.get());
  std::vector<std::uint8_t> out;
  ArxLevelModelPreviewReport report{};
  const ArxReturnCode rc = source.level.exportGlb(out, previews, invocation.options.glb_export, &report);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  if (report.previewed_entities != 0)
    log(ARX_LOG_INFO, "attached Model previews to %zu Level entity instance(s)", report.previewed_entities);
  GeneratedLevelImages projected;
  ResourceOutputPlan resources;
  if (!prepareProjectedResourceOutputs(resources, {}, source.level, projected, execution, invocation)) return false;
  return writeOutput(execution.io(), invocation.output, out.data(), out.size()) &&
         execution.resourceOutputs().write(resources);
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
