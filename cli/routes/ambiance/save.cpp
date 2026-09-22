// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/save.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "pipeline/execution_context.h"
#include "resources/output.h"
#include "resources/resource_output.h"
#include "resources/sound_io.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/state.h"
#include "routes/native_text.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cli::ambiance {
namespace {

bool outputFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kAmbianceOutputFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool prepareIntermediateSounds(IntermediateAmbiance& source, const Invocation& invocation) {
  if (!invocation.sound_rebase.enabled) return true;
  const ArxReturnCode rc = source.ambiance.rebaseSoundPaths(invocation.sound_rebase.directory);
  if (rc != ARX_OK) return outputFailure("Ambiance sound rebasing", rc);
  return true;
}

bool writeAmbianceFiles(const void* primary_data, std::size_t primary_size, std::span<const pistoris::SoundFile> sounds,
                        const ExecutionContext& execution, const Invocation& invocation) {
  ResourceOutputPlan resource_outputs;
  resource_outputs.reserveOutput(invocation.output);
  if (!sounds.empty()) {
    const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kAmbiance, invocation.output.path);
    if (!addSoundFileOutputs(resource_outputs,
                             execution.io(),
                             invocation.sound_output,
                             sounds,
                             asset,
                             DiagnosticCode::kAmbianceOutputFailed,
                             "Ambiance"))
      return false;
  }
  if (!execution.resourceOutputs().resolve(resource_outputs)) return false;
  return writeOutput(execution.io(), invocation.output, primary_data, primary_size) &&
         execution.resourceOutputs().write(resource_outputs);
}

bool writeAmbFile(const pistoris::Amb& ambiance, std::span<const pistoris::SoundFile> sounds,
                  const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<std::uint8_t> bytes;
  const ArxReturnCode rc = pistoris::writeAmb(ambiance, bytes);
  if (rc != ARX_OK) return outputFailure("AMB output", rc);
  return writeAmbianceFiles(bytes.data(), bytes.size(), sounds, execution, invocation);
}

bool writeAmbNative(NativeAmbiance& source, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::SoundFile> sounds;
  if (invocation.sound_options.export_files)
    loadNativeSoundFiles(source.ambiance, source.text_mode, execution.io(), invocation.sounds, sounds);
  return writeAmbFile(source.ambiance, sounds, execution, invocation);
}

bool writeJsonFile(const pistoris::Amb& ambiance, std::span<const pistoris::SoundFile> sounds,
                   pistoris::NativeTextMode text_mode, const ExecutionContext& execution,
                   const Invocation& invocation) {
  std::string text;
  const ArxReturnCode rc = pistoris::toJson(ambiance, text, invocation.format.pretty, text_mode);
  if (rc != ARX_OK) return outputFailure("AMB JSON output", rc);
  return writeAmbianceFiles(text.data(), text.size(), sounds, execution, invocation);
}

bool writeJsonNative(NativeAmbiance& source, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::SoundFile> sounds;
  if (invocation.sound_options.export_files)
    loadNativeSoundFiles(source.ambiance, source.text_mode, execution.io(), invocation.sounds, sounds);
  return writeJsonFile(source.ambiance, sounds, source.text_mode, execution, invocation);
}

bool writeAmbIntermediate(IntermediateAmbiance& source, const ExecutionContext& execution,
                          const Invocation& invocation) {
  const pistoris::NativeTextMode text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  pistoris::NativeAmbianceBundle bundle;
  const ArxReturnCode rc = source.ambiance.bakeNativeBundle(
      {.include_sound_files = invocation.sound_options.export_files, .text_mode = text_mode}, bundle);
  if (rc != ARX_OK) return outputFailure("Ambiance native output", rc);
  return writeAmbFile(bundle.amb, bundle.sound_files, execution, invocation);
}

bool writeJsonIntermediate(IntermediateAmbiance& source, const ExecutionContext& execution,
                           const Invocation& invocation) {
  const pistoris::NativeTextMode text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  pistoris::NativeAmbianceBundle bundle;
  const ArxReturnCode rc = source.ambiance.bakeNativeBundle(
      {.include_sound_files = invocation.sound_options.export_files, .text_mode = text_mode}, bundle);
  if (rc != ARX_OK) return outputFailure("Ambiance native output", rc);
  return writeJsonFile(bundle.amb, bundle.sound_files, text_mode, execution, invocation);
}

bool writeGlbIntermediate(IntermediateAmbiance& source, const ExecutionContext& execution,
                          const Invocation& invocation) {
  if (!invocation.sound_options.export_files) {
    std::vector<std::uint8_t> bytes;
    const ArxReturnCode rc =
        source.ambiance.exportGlb(bytes, invocation.options.glb_export, invocation.reference_model.get());
    if (rc != ARX_OK) return outputFailure("Ambiance GLB output", rc);
    return writeAmbianceFiles(bytes.data(), bytes.size(), {}, execution, invocation);
  }

  pistoris::AmbianceGlbBundle bundle;
  const ArxReturnCode rc =
      source.ambiance.exportGlbBundle(invocation.options.glb_export, invocation.reference_model.get(), bundle);
  if (rc != ARX_OK) return outputFailure("Ambiance GLB output", rc);
  return writeAmbianceFiles(bundle.glb.data(), bundle.glb.size(), bundle.sound_files, execution, invocation);
}

}  // namespace

const OutputConverterDescriptor* outputConverterDescriptor(Format output) {
  static constexpr OutputConverterDescriptor kAmb{writeAmbNative, writeAmbIntermediate};
  static constexpr OutputConverterDescriptor kJson{writeJsonNative, writeJsonIntermediate};
  static constexpr OutputConverterDescriptor kGlb{nullptr, writeGlbIntermediate};
  switch (output) {
    case Format::kAmb:
      return &kAmb;
    case Format::kJson:
      return &kJson;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool writeNativeOutput(NativeAmbiance& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_native) {
    diagnostic(DiagnosticCode::kAmbianceUnsupportedOutput, "Ambiance output converter requires intermediate data");
    return false;
  }
  return invocation.output_converter->write_native(source, execution, invocation);
}

bool writeIntermediateOutput(IntermediateAmbiance& source, const ExecutionContext& execution,
                             const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_intermediate) {
    diagnostic(DiagnosticCode::kAmbianceUnsupportedOutput, "Ambiance output converter is unavailable");
    return false;
  }
  if (!prepareIntermediateSounds(source, invocation)) return false;
  return invocation.output_converter->write_intermediate(source, execution, invocation);
}

}  // namespace cli::ambiance
