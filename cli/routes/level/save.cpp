// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/save.h"

#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level_diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/paths.h"
#include "io/service.h"
#include "modules/module.h"
#include "pipeline/execution_context.h"
#include "resources/output.h"
#include "resources/selector.h"
#include "routes/level/invocation.h"
#include "routes/level/native_input.h"
#include "routes/level/operations.h"
#include "routes/level/options/modules.h"
#include "routes/level/state.h"

#include <concepts>
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

template <typename Native>
bool writeNativeJson(IoService& io, const OutputTarget& target, const Native& native, bool pretty,
                     const char* description, std::string_view signer = {}) {
  std::string json;
  ArxReturnCode rc = ARX_OK;
  if constexpr (std::same_as<Native, pistoris::Dlf> || std::same_as<Native, pistoris::Llf>) {
    rc = pistoris::exportJson(native, json, pretty, signer);
  } else {
    rc = pistoris::exportJson(native, json, pretty);
  }
  if (rc != ARX_OK) return outputFailure(description, rc);
  return writeText(io, target, json);
}

bool writeTextureFiles(IoService& io, const NativeOutput& output, std::span<const pistoris::NativeTextureFile> files) {
  for (const pistoris::NativeTextureFile& file : files) {
    PathLocation location;
    std::string error;
    const char* filename = pathFilename(file.resource_path.c_str());
    if (!io.appendPathLocation(output.texture_folder, filename, location, error)) {
      diagnostic(DiagnosticCode::kLevelOutputFailed,
                 "Cannot resolve texture output '%s': %s",
                 file.resource_path.c_str(),
                 error.c_str());
      return false;
    }
    OutputTarget target;
    static_cast<PathLocation&>(target) = std::move(location);
    if (!writeOutput(io, target, file.encoded_image.data(), file.encoded_image.size())) return false;
  }
  return true;
}

void logDetachedLayout(const NativeOutput& output) {
  if (!output.detached_layout) return;
  log(ARX_LOG_INFO,
      "loose Level output uses physical FTS '%s'; DLF runtime FTS reference is '%s'",
      output.fts.path.c_str(),
      output.runtime_fts_path.c_str());
}

std::string jsonLevelName(const Invocation& invocation) {
  return "level" + std::to_string(invocation.json_output.level);
}

bool nativeBake(IntermediateLevel& source, const Invocation& invocation, NativeEncoding encoding,
                bool include_texture_files, NativeLevelFiles& out) {
  if (invocation.options.dlf_only) {
    const std::string level_name = encoding == NativeEncoding::kJson ? jsonLevelName(invocation) : std::string{};
    pistoris::Level::NativeDlfBakeOptions options;
    options.level_name = level_name;
    options.target_fts_offset = source.source_fts_offset;
    if (encoding == NativeEncoding::kBinary || invocation.options.fts_scene_directory_specified)
      options.dlf_scene_path = invocation.native_output.dlf_scene_path;

    pistoris::Dlf dlf;
    const ArxReturnCode rc = source.level.bakeNativeDlf(options, dlf);
    if (rc != ARX_OK) return outputFailure("Level DLF output", rc);
    out.dlf = std::move(dlf);
    return true;
  }

  const std::string level_name = encoding == NativeEncoding::kJson ? jsonLevelName(invocation) : std::string{};
  pistoris::Level::NativeBakeOptions options;
  options.level_name = level_name;
  options.reconstruct_quads = invocation.options.reconstruct_quads;
  options.include_texture_files = include_texture_files;
  if (encoding == NativeEncoding::kBinary || invocation.options.output_texture_folder_specified) {
    options.texture_folder = invocation.native_output.texture_resource_directory;
    options.texture_path_mode = pistoris::NativeTexturePathMode::kRebase;
  }
  if (encoding == NativeEncoding::kBinary || invocation.options.fts_scene_directory_specified)
    options.dlf_scene_path = invocation.native_output.dlf_scene_path;

  pistoris::NativeLevelBundle bundle;
  const ArxReturnCode rc = source.level.bakeNativeBundle(options, bundle);
  if (rc != ARX_OK) return outputFailure("Level native bundle output", rc);
  out.fts = std::move(bundle.fts);
  out.llf = std::move(bundle.llf);
  out.dlf = std::move(bundle.dlf);
  out.texture_files = std::move(bundle.texture_files);
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
  if (!writeTextureFiles(io, invocation.native_output, files.texture_files)) return false;
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
  return !files.dlf || writeNativeJson(io,
                                       invocation.json_output.dlf,
                                       *files.dlf,
                                       invocation.format.pretty,
                                       "DLF JSON output",
                                       invocation.options.signer);
}

bool writeNativeBinaryIntermediate(IntermediateLevel& source, const ExecutionContext& execution,
                                   const Invocation& invocation, const operations::OperationDiagnostics&) {
  const bool include_texture_files = invocation.options.export_textures && !invocation.options.dlf_only;

  NativeLevelFiles files;
  if (!nativeBake(source, invocation, NativeEncoding::kBinary, include_texture_files, files)) return false;
  return writeBinaryFiles(files, execution, invocation);
}

bool writeJsonNative(NativeLevelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  return writeJsonFiles(files, execution, invocation);
}

bool writeJsonIntermediate(IntermediateLevel& source, const ExecutionContext& execution, const Invocation& invocation,
                           const operations::OperationDiagnostics&) {
  NativeLevelFiles files;
  if (!nativeBake(source, invocation, NativeEncoding::kJson, false, files)) return false;
  return writeJsonFiles(files, execution, invocation);
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
  options.include_texture_files = false;
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
  std::vector<std::uint8_t> out;
  const ArxReturnCode rc = source.level.exportGlb(out, invocation.options.glb_export);
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  return writeOutput(execution.io(), invocation.output, out.data(), out.size());
}

}  // namespace

const OutputConverterDescriptor* outputConverterDescriptor(Format output, const Module* module) {
  static constexpr OutputConverterDescriptor kNativeBinary{
      .write_intermediate = writeNativeBinaryIntermediate,
      .requires_texture_images = true,
  };
  static constexpr OutputConverterDescriptor kJson{
      .write_native = writeJsonNative,
      .write_intermediate = writeJsonIntermediate,
  };
  static constexpr OutputConverterDescriptor kGlb{
      .write_intermediate = writeGlbIntermediate,
      .requires_texture_images = true,
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
