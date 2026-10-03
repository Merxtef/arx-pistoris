// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/cinematic/save.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/cinematic/glb.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "pipeline/execution_context.h"
#include "resources/cinematic_sound_io.h"
#include "resources/layout.h"
#include "resources/output.h"
#include "resources/resource_output.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/state.h"
#include "routes/conversion_failure.h"

#include <cstddef>
#include <span>
#include <vector>

namespace cli::cinematic {
namespace {

template <class Result>
bool outputFailure(const char* what, const Result& result) {
  return conversionOutputFailure(DiagnosticCode::kCinematicOutputFailed, what, result);
}

bool prepareIntermediate(IntermediateCinematic& source, const Invocation& invocation) {
  if (invocation.texture_rebase.enabled) {
    const auto result = source.cinematic.rebaseTexturePaths(invocation.texture_rebase.directory);
    if (!result) return outputFailure("Cinematic illustration rebasing", result);
  }
  if (invocation.effect_rebase.enabled) {
    const auto result =
        source.cinematic.rebaseSoundPaths(pistoris::SoundKind::kEffect, invocation.effect_rebase.directory);
    if (!result) return outputFailure("Cinematic sound-effect rebasing", result);
  }
  if (invocation.speech_rebase.enabled) {
    const auto result =
        source.cinematic.rebaseSoundPaths(pistoris::SoundKind::kSpeech, invocation.speech_rebase.directory);
    if (!result) return outputFailure("Cinematic speech rebasing", result);
  }
  return true;
}

template <typename SoundFile>
bool writeCinematicFilesImpl(const void* primary_data, std::size_t primary_size,
                             std::span<const pistoris::NativeTextureFile> illustrations,
                             std::span<const SoundFile> sounds, const ExecutionContext& execution,
                             const Invocation& invocation) {
  ResourceOutputPlan outputs;
  outputs.reserveOutput(invocation.output);
  if (!illustrations.empty() || !sounds.empty()) {
    const ResourceAssetId asset = outputs.addAsset(ResourceAssetKind::kCinematic, invocation.output.path);
    if (!addNativeTextureFileOutputs(outputs,
                                     execution.io(),
                                     invocation.texture_output,
                                     illustrations,
                                     asset,
                                     DiagnosticCode::kCinematicOutputFailed,
                                     "Cinematic") ||
        !addSoundFileOutputs(outputs,
                             execution.io(),
                             invocation.sound_output,
                             sounds,
                             asset,
                             DiagnosticCode::kCinematicOutputFailed,
                             "Cinematic"))
      return false;
  }
  if (!execution.resourceOutputs().resolve(outputs)) return false;
  return writeOutput(execution.io(), invocation.output, primary_data, primary_size) &&
         execution.resourceOutputs().write(outputs);
}

bool writeCinematicFiles(const void* primary_data, std::size_t primary_size,
                         std::span<const pistoris::NativeTextureFile> illustrations,
                         std::span<const pistoris::CinematicSoundFile> sounds, const ExecutionContext& execution,
                         const Invocation& invocation) {
  return writeCinematicFilesImpl(primary_data, primary_size, illustrations, sounds, execution, invocation);
}

bool writeCinematicFiles(const void* primary_data, std::size_t primary_size,
                         std::span<const pistoris::NativeTextureFile> illustrations,
                         std::span<const pistoris::SoundFile> sounds, const ExecutionContext& execution,
                         const Invocation& invocation) {
  return writeCinematicFilesImpl(primary_data, primary_size, illustrations, sounds, execution, invocation);
}

template <typename SoundFile>
bool writeCinFileImpl(const pistoris::Cin& cinematic, std::span<const pistoris::NativeTextureFile> illustrations,
                      std::span<const SoundFile> sounds, const ExecutionContext& execution,
                      const Invocation& invocation) {
  auto bytes = pistoris::writeCin(cinematic);
  if (!bytes) return outputFailure("CIN output", bytes);
  return writeCinematicFiles(bytes->data(), bytes->size(), illustrations, sounds, execution, invocation);
}

bool writeCinFile(const pistoris::Cin& cinematic, std::span<const pistoris::NativeTextureFile> illustrations,
                  std::span<const pistoris::CinematicSoundFile> sounds, const ExecutionContext& execution,
                  const Invocation& invocation) {
  return writeCinFileImpl(cinematic, illustrations, sounds, execution, invocation);
}

bool writeCinFile(const pistoris::Cin& cinematic, std::span<const pistoris::NativeTextureFile> illustrations,
                  std::span<const pistoris::SoundFile> sounds, const ExecutionContext& execution,
                  const Invocation& invocation) {
  return writeCinFileImpl(cinematic, illustrations, sounds, execution, invocation);
}

bool writeCinNative(NativeCinematic& source, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> illustrations;
  if (invocation.texture_options.export_files)
    loadNativeTextureFiles(source.cinematic, source.text_mode, execution.io(), invocation.textures, illustrations);
  std::vector<pistoris::SoundFile> sounds;
  if (invocation.sound_options.export_files)
    loadNativeCinematicSoundFiles(source.cinematic, source.text_mode, execution.io(), invocation.sounds, sounds);
  return writeCinFile(source.cinematic, illustrations, sounds, execution, invocation);
}

bool writeCinIntermediate(IntermediateCinematic& source, const ExecutionContext& execution,
                          const Invocation& invocation) {
  auto bundle = source.cinematic.bakeNativeBundle(
      {.include_illustration_files = invocation.texture_options.export_files,
       .include_sound_files = invocation.sound_options.export_files,
       .illustration_format = static_cast<ArxImageFormat>(
           invocation.texture_output.layout == ResourceLayout::kGame ? ARX_IMAGE_FORMAT_BMP : ARX_IMAGE_FORMAT_UNKNOWN),
       .text_mode = invocation.native_text_mode});
  if (!bundle) return outputFailure("Cinematic native output", bundle);
  return writeCinFile(bundle->cin, bundle->illustration_files, bundle->sound_files, execution, invocation);
}

bool writeGlbIntermediate(IntermediateCinematic& source, const ExecutionContext& execution,
                          const Invocation& invocation) {
  if (!invocation.sound_options.export_files) {
    auto bytes = source.cinematic.exportGlb();
    if (!bytes) return outputFailure("Cinematic GLB output", bytes);
    return writeCinematicFiles(
        bytes->data(), bytes->size(), {}, std::span<const pistoris::CinematicSoundFile>{}, execution, invocation);
  }

  auto bundle = source.cinematic.exportGlbBundle();
  if (!bundle) return outputFailure("Cinematic GLB output", bundle);
  return writeCinematicFiles(bundle->glb.data(), bundle->glb.size(), {}, bundle->sound_files, execution, invocation);
}

}  // namespace

const OutputConverterDescriptor* outputConverterDescriptor(Format output) {
  static constexpr OutputConverterDescriptor kCin{writeCinNative, writeCinIntermediate};
  static constexpr OutputConverterDescriptor kGlb{nullptr, writeGlbIntermediate};
  switch (output) {
    case Format::kCin:
      return &kCin;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool writeNativeOutput(NativeCinematic& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_native) {
    diagnostic(DiagnosticCode::kCinematicUnsupportedOutput, "Cinematic output converter requires intermediate data");
    return false;
  }
  return invocation.output_converter->write_native(source, execution, invocation);
}

bool writeIntermediateOutput(IntermediateCinematic& source, const ExecutionContext& execution,
                             const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_intermediate) {
    diagnostic(DiagnosticCode::kCinematicUnsupportedOutput, "Cinematic output converter is unavailable");
    return false;
  }
  if (!prepareIntermediate(source, invocation)) return false;
  return invocation.output_converter->write_intermediate(source, execution, invocation);
}

}  // namespace cli::cinematic
