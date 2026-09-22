// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/save.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
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
#include "resources/selector.h"
#include "resources/sound_io.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/native_text.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cli::animation {
namespace {

bool outputFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kAnimationOutputFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool writeNativeFile(const pistoris::Tea& animation, const ExecutionContext& execution, const Invocation& invocation,
                     bool json, pistoris::NativeTextMode text_mode, std::span<const pistoris::SoundFile> sound_files) {
  ResourceOutputPlan resource_outputs;
  resource_outputs.reserveOutput(invocation.output);
  if (!sound_files.empty()) {
    const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kAnimation, invocation.output.path);
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

  bool success = false;
  if (json) {
    std::string text;
    const ArxReturnCode rc = pistoris::toJson(animation, text, invocation.format.pretty, text_mode);
    if (rc != ARX_OK) {
      outputFailure("Animation JSON output", rc);
    } else {
      success = writeOutput(execution.io(), invocation.output, text.data(), text.size());
    }
  } else {
    std::vector<std::uint8_t> bytes;
    const ArxReturnCode rc = pistoris::writeTea(animation, bytes);
    if (rc != ARX_OK) {
      outputFailure("TEA output", rc);
    } else {
      success = writeOutput(execution.io(), invocation.output, bytes.data(), bytes.size());
    }
  }
  if (!success) return false;
  return execution.resourceOutputs().write(resource_outputs);
}

bool writeTeaNative(NativeAnimation& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (invocation.sound_options.export_files)
    loadNativeSoundFiles(
        source.animation, source.text_mode, execution.io(), invocation.sound_input, source.sound_files);
  return writeNativeFile(source.animation, execution, invocation, false, source.text_mode, source.sound_files);
}

bool writeJsonNative(NativeAnimation& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (invocation.sound_options.export_files)
    loadNativeSoundFiles(
        source.animation, source.text_mode, execution.io(), invocation.sound_input, source.sound_files);
  return writeNativeFile(source.animation, execution, invocation, true, source.text_mode, source.sound_files);
}

bool bakeIntermediate(IntermediateAnimation& source, const Invocation& invocation, NativeAnimation& out) {
  std::size_t removed = 0;
  ArxReturnCode rc = source.animation.compactSounds(&removed);
  if (rc != ARX_OK) return outputFailure("Animation sound compaction", rc);
  if (invocation.sound_rebase.enabled) {
    rc = source.animation.rebaseSoundPaths(invocation.sound_rebase.directory);
    if (rc != ARX_OK) return outputFailure("Animation sound rebasing", rc);
  }
  out.text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  pistoris::NativeAnimationBundle bundle;
  rc = source.animation.bakeNativeBundle(
      {.include_sound_files = invocation.sound_options.export_files, .text_mode = out.text_mode}, bundle);
  if (rc != ARX_OK) return outputFailure("Animation native output", rc);
  out.animation = std::move(bundle.tea);
  out.sound_files = std::move(bundle.sound_files);
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
