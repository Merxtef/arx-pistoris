// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/resolution.h"

#include "arx_pistoris/model.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "io/service.h"
#include "pipeline/options.h"
#include "resources/input.h"
#include "resources/layout.h"
#include "resources/model_input.h"
#include "resources/model_input_io.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/load.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/save.h"
#include "routes/ambiance/state.h"
#include "routes/conversion_path.h"
#include "routes/descriptor.h"

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cli::ambiance {
namespace {

bool resolveSoundInput(const ClassifiedPath& input, const SoundIoOptions& options, IoService& io, SoundInput& out) {
  out.use_format_sources = input.layout == ResourceLayout::kLoose;
  return resolveSidecarInputBase(input,
                                 out.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 "Ambiance",
                                 "sound",
                                 io,
                                 out.source_base);
}

bool resolveSoundOutput(const RouteResolveContext& context, Invocation& invocation) {
  invocation.sound_output.layout = invocation.output.layout;
  if (invocation.output.layout == ResourceLayout::kGame) return true;
  return resolveSidecarOutputBase(invocation.output, "Ambiance", "sound", context.io, invocation.sound_output.base);
}

bool resolveSoundRebase(const RouteResolveContext& context, Invocation& invocation, SidecarRebaseDirection automatic) {
  return resolveSidecarRebase({.explicit_requested = context.conversion.rebase_sounds,
                               .explicit_directory = context.conversion.sound_directory,
                               .automatic = automatic,
                               .to_loose_directory = kLooseSoundDirectory,
                               .to_game_directory = pistoris::paths::ambianceSoundDirectory()},
                              "sound",
                              context.io,
                              invocation.rebase_sounds,
                              invocation.sound_rebase_directory);
}

bool loadReferenceModel(const std::string& argument, const TextureIoOptions& texture_options, IoService& io,
                        Invocation& invocation) {
  const std::array<const char*, 1> arguments = {argument.c_str()};
  std::vector<ClassifiedPath> inputs;
  if (!loadClassifiedInputs(arguments, io, inputs)) return false;
  if (inputs.size() != 1 || !isModelInput(inputs.front().facts)) {
    diagnostic(
        DiagnosticCode::kAmbianceInputFailed, "Reference Model input format is unsupported: %s", argument.c_str());
    return false;
  }

  const ClassifiedPath& input = inputs.front();
  std::vector<ModelMaterialLibraryInput> material_libraries;
  if (!readModelMaterialLibraries(
          input, io, DiagnosticCode::kAmbianceInputFailed, "reference Model", material_libraries))
    return false;

  ConvertedModelInput converted;
  pistoris::Model::GlbImportOptions glb_options;
  glb_options.arx_units_per_glb_unit = invocation.options.glb_import.arx_units_per_glb_unit;
  if (!convertModelInput(
          input, material_libraries, glb_options, DiagnosticCode::kAmbianceInputFailed, "Reference Model", converted)) {
    return false;
  }

  TextureInput textures;
  if (!resolveModelTextureInput(input, texture_options, io, "reference Model", textures)) return false;
  if (!loadTextureImages(converted.model, io, textures, converted.texture_source_paths)) return false;
  if (!converted.animations.empty()) {
    log(ARX_LOG_WARN, "Reference Model input ignored %zu Animation sidecar(s)", converted.animations.size());
  }

  auto reference = std::make_unique<pistoris::Model>();
  reference->swap(converted.model);
  invocation.reference_model = std::move(reference);
  return true;
}

}  // namespace

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation) {
  if (context.route_options) invocation.options = static_cast<const AmbianceOptions&>(*context.route_options);
  invocation.format = context.format_options;
  invocation.sound_options = context.sound_options;
  applyFormatModifiers(invocation.options, context.format_modifiers);

  const InputConverterDescriptor* input_converter = inputConverterDescriptor(context.route);
  invocation.output_converter = outputConverterDescriptor(context.route.output);
  if (!input_converter) {
    diagnostic(DiagnosticCode::kAmbianceUnsupportedInput, "Unsupported Ambiance input converter");
    return false;
  }
  if (!invocation.output_converter) {
    diagnostic(DiagnosticCode::kAmbianceUnsupportedOutput, "Unsupported Ambiance output converter");
    return false;
  }

  if (!resolveSoundInput(context.inputs[invocation.input], invocation.sound_options, context.io, invocation.sounds) ||
      !resolveSoundOutput(context, invocation))
    return false;

  const bool native = selectConversionPath(input_converter->load_native != nullptr,
                                           invocation.output_converter->write_native != nullptr,
                                           context.requires_intermediate) == ConversionPath::kNative;
  const SidecarRebaseDirection automatic =
      native ? SidecarRebaseDirection::kNone
             : automaticSidecarRebase(sidecarEndpoint(context.inputs[invocation.input], ARX_RESOURCE_KIND_AMBIANCE),
                                      sidecarEndpoint(invocation.output, ARX_RESOURCE_KIND_AMBIANCE));
  if (!resolveSoundRebase(context, invocation, automatic)) return false;
  if (!loadInput(*input_converter, context.inputs, invocation, native, invocation.state)) return false;
  if (IntermediateAmbiance* intermediate = std::get_if<IntermediateAmbiance>(&invocation.state)) {
    const bool needs_sound_data = invocation.sound_options.export_files || (invocation.options.trim_tracks_to_master &&
                                                                            intermediate->ambiance.trackCount() > 1U);
    if (needs_sound_data &&
        !loadSoundData(intermediate->ambiance, context.io, invocation.sounds, intermediate->sound_sources))
      return false;
    intermediate->sound_sources.clear();
  }
  return !invocation.options.reference_model ||
         loadReferenceModel(*invocation.options.reference_model, context.texture_options, context.io, invocation);
}

}  // namespace cli::ambiance
