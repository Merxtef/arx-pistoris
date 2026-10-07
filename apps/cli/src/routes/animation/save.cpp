// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/save.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "pipeline/execution_context.h"
#include "resources/resource_output.h"
#include "resources/selector.h"
#include "resources/sound_io.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"

#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cli::animation {
namespace {

template <class Result>
bool outputFailure(const char* what, const Result& result) {
  return conversionOutputFailure(DiagnosticCode::kAnimationOutputFailed, what, result);
}

bool writeNativeFile(const pistoris::Tea& animation, const ExecutionContext& execution, const Invocation& invocation,
                     bool json, pistoris::NativeTextMode text_mode, std::span<const pistoris::SoundFile> sound_files) {
  std::vector<std::uint8_t> primary;
  if (json) {
    auto text = pistoris::toTeaJson(animation, invocation.format.pretty, text_mode);
    if (!text) return outputFailure("Animation JSON output", text);
    primary.assign(text->begin(), text->end());
  } else {
    auto bytes = pistoris::writeTea(animation);
    if (!bytes) return outputFailure("TEA output", bytes);
    primary = std::move(*bytes);
  }

  ResourceOutputPlan resource_outputs;
  const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kAnimation, invocation.output.path);
  resource_outputs.addPrimaryOwned(invocation.output, std::move(primary), asset);
  if (!sound_files.empty()) {
    if (!addSoundFileOutputs(resource_outputs,
                             execution.io(),
                             invocation.sound_output,
                             sound_files,
                             asset,
                             DiagnosticCode::kAnimationOutputFailed,
                             "Animation"))
      return false;
  }
  if (!execution.resourceOutputs().resolve(resource_outputs)) return false;
  return execution.resourceOutputs().write(resource_outputs);
}

bool writeTeaNative(NativeAnimation& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (invocation.sound_options.export_files && invocation.sound_options.input_folder_specified)
    loadNativeSoundFiles(
        source.animation, source.text_mode, execution.io(), invocation.sound_input, source.sound_files);
  const std::span<const pistoris::SoundFile> sounds =
      invocation.sound_options.export_files ? source.sound_files : std::span<const pistoris::SoundFile>{};
  return writeNativeFile(source.animation, execution, invocation, false, source.text_mode, sounds);
}

bool writeJsonNative(NativeAnimation& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (invocation.sound_options.export_files && invocation.sound_options.input_folder_specified)
    loadNativeSoundFiles(
        source.animation, source.text_mode, execution.io(), invocation.sound_input, source.sound_files);
  const std::span<const pistoris::SoundFile> sounds =
      invocation.sound_options.export_files ? source.sound_files : std::span<const pistoris::SoundFile>{};
  return writeNativeFile(source.animation, execution, invocation, true, source.text_mode, sounds);
}

bool bakeIntermediate(IntermediateAnimation& source, const Invocation& invocation, NativeAnimation& out) {
  const auto compacted = source.animation.compactSounds();
  if (!compacted) return outputFailure("Animation sound compaction", compacted);
  if (invocation.sound_rebase.enabled) {
    const auto rebased = source.animation.rebaseSoundPaths(invocation.sound_rebase.directory);
    if (!rebased) return outputFailure("Animation sound rebasing", rebased);
  }
  out.text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  auto bundle = source.animation.bakeNativeBundle(
      {.include_sound_files = invocation.sound_options.export_files, .text_mode = out.text_mode});
  if (!bundle) return outputFailure("Animation native output", bundle);
  out.animation = std::move(bundle->tea);
  out.sound_files = std::move(bundle->sound_files);
  return true;
}

bool writeTeaIntermediate(IntermediateAnimation& source, const ExecutionContext& execution,
                          const Invocation& invocation) {
  NativeAnimation native;
  return bakeIntermediate(source, invocation, native) &&
         writeNativeFile(native.animation, execution, invocation, false, native.text_mode, native.sound_files);
}

bool writeJsonIntermediate(IntermediateAnimation& source, const ExecutionContext& execution,
                           const Invocation& invocation) {
  NativeAnimation native;
  return bakeIntermediate(source, invocation, native) &&
         writeNativeFile(native.animation, execution, invocation, true, native.text_mode, native.sound_files);
}

}  // namespace

const OutputConverterDescriptor* outputConverterDescriptor(Format output) {
  static constexpr OutputConverterDescriptor kTea{writeTeaNative, writeTeaIntermediate};
  static constexpr OutputConverterDescriptor kJson{writeJsonNative, writeJsonIntermediate};
  switch (output) {
    case Format::kTea:
      return &kTea;
    case Format::kJson:
      return &kJson;
    default:
      return nullptr;
  }
}

bool writeNativeOutput(NativeAnimation& animation, const ExecutionContext& execution, const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_native) {
    diagnostic(DiagnosticCode::kAnimationUnsupportedOutput, "Animation output converter requires intermediate data");
    return false;
  }
  return invocation.output_converter->write_native(animation, execution, invocation);
}

bool writeIntermediateOutput(IntermediateAnimation& animation, const ExecutionContext& execution,
                             const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_intermediate) {
    diagnostic(DiagnosticCode::kAnimationUnsupportedOutput, "Animation output converter is unavailable");
    return false;
  }
  return invocation.output_converter->write_intermediate(animation, execution, invocation);
}

}  // namespace cli::animation
