// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/save.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "pipeline/execution_context.h"
#include "resources/output.h"
#include "resources/resource_output.h"
#include "resources/sound_io.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace cli::ambiance {
namespace {

template <class Result>
bool outputFailure(const char* what, const Result& result) {
  return conversionOutputFailure(DiagnosticCode::kAmbianceOutputFailed, what, result);
}

bool prepareIntermediateSounds(IntermediateAmbiance& source, const Invocation& invocation) {
  if (!invocation.sound_rebase.enabled) return true;
  const auto result = source.ambiance.rebaseSoundPaths(invocation.sound_rebase.directory);
  return result || outputFailure("Ambiance sound rebasing", result);
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
  auto bytes = pistoris::writeAmb(ambiance);
  if (!bytes) return outputFailure("AMB output", bytes);
  return writeAmbianceFiles(bytes->data(), bytes->size(), sounds, execution, invocation);
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
  auto text = pistoris::toAmbJson(ambiance, invocation.format.pretty, text_mode);
  if (!text) return outputFailure("AMB JSON output", text);
  return writeAmbianceFiles(text->data(), text->size(), sounds, execution, invocation);
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
  auto bundle = source.ambiance.bakeNativeBundle(
      {.include_sound_files = invocation.sound_options.export_files, .text_mode = text_mode});
  if (!bundle) return outputFailure("Ambiance native output", bundle);
  return writeAmbFile(bundle->amb, bundle->sound_files, execution, invocation);
}

bool writeJsonIntermediate(IntermediateAmbiance& source, const ExecutionContext& execution,
                           const Invocation& invocation) {
  const pistoris::NativeTextMode text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  auto bundle = source.ambiance.bakeNativeBundle(
      {.include_sound_files = invocation.sound_options.export_files, .text_mode = text_mode});
  if (!bundle) return outputFailure("Ambiance native output", bundle);
  return writeJsonFile(bundle->amb, bundle->sound_files, text_mode, execution, invocation);
}

bool writeGlbIntermediate(IntermediateAmbiance& source, const ExecutionContext& execution,
                          const Invocation& invocation) {
  if (!invocation.sound_options.export_files) {
    auto bytes = source.ambiance.exportGlb(invocation.options.glb_export, invocation.reference_model.get());
    if (!bytes) return outputFailure("Ambiance GLB output", bytes);
    return writeAmbianceFiles(bytes->data(), bytes->size(), {}, execution, invocation);
  }

  auto bundle = source.ambiance.exportGlbBundle(invocation.options.glb_export, invocation.reference_model.get());
  if (!bundle) return outputFailure("Ambiance GLB output", bundle);
  return writeAmbianceFiles(bundle->glb.data(), bundle->glb.size(), bundle->sound_files, execution, invocation);
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
