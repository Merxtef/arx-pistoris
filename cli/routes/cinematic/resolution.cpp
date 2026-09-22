// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/cinematic/resolution.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "conversion/options.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "pipeline/options.h"
#include "resources/cinematic_sound_io.h"
#include "resources/layout.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/load.h"
#include "routes/cinematic/options.h"
#include "routes/cinematic/save.h"
#include "routes/cinematic/state.h"
#include "routes/conversion_path.h"
#include "routes/descriptor.h"

#include <string_view>
#include <variant>

namespace cli::cinematic {
namespace {

constexpr std::string_view kLooseSpeechDirectory = "speech";

bool resolveTextureInput(const ClassifiedPath& input, const TextureIoOptions& options, IoService& io,
                         TextureInput& out) {
  out.use_format_sources = input.layout == ResourceLayout::kLoose;
  out.source_lookup = input.facts.format == Format::kGlb ? ImageLookupMode::kExact : ImageLookupMode::kGamePriority;
  return resolveSidecarInputBase(input,
                                 out.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 "Cinematic",
                                 "illustration",
                                 io,
                                 out.source_base);
}

bool resolveSoundInput(const ClassifiedPath& input, const SoundIoOptions& options, IoService& io, SoundInput& out) {
  out.use_format_sources = input.layout == ResourceLayout::kLoose;
  return resolveSidecarInputBase(input,
                                 out.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 "Cinematic",
                                 "sound",
                                 io,
                                 out.source_base);
}

bool resolveOutputs(const RouteResolveContext& context, Invocation& invocation) {
  invocation.texture_output.layout = invocation.output.layout;
  invocation.sound_output.layout = invocation.output.layout;
  if (invocation.output.layout == ResourceLayout::kGame) return true;
  if (context.route.output == Format::kCin && invocation.texture_options.export_files &&
      !resolveSidecarOutputBase(
          invocation.output, "Cinematic", "illustration", context.io, invocation.texture_output.base))
    return false;
  if (invocation.sound_options.export_files &&
      !resolveSidecarOutputBase(invocation.output, "Cinematic", "sound", context.io, invocation.sound_output.base))
    return false;
  return true;
}

SidecarRebaseDirection automaticRebase(const RouteResolveContext& context) {
  if (!((context.route.input == Format::kCin && context.route.output == Format::kGlb) ||
        (context.route.input == Format::kGlb && context.route.output == Format::kCin)))
    return SidecarRebaseDirection::kNone;
  return automaticSidecarRebase(sidecarEndpoint(context.inputs.front(), ARX_RESOURCE_KIND_CINEMATIC),
                                sidecarEndpoint(context.output, ARX_RESOURCE_KIND_CINEMATIC));
}

bool resolveRebase(const DirectoryRebaseRequest& explicit_request, SidecarRebaseDirection automatic,
                   std::string_view loose_directory, std::string_view game_directory, std::string_view resource,
                   IoService& io, ResolvedSidecarRebase& out) {
  return resolveSidecarRebase({.explicit_requested = explicit_request.requested,
                               .explicit_directory = explicit_request.directory,
                               .automatic = automatic,
                               .to_loose_directory = loose_directory,
                               .to_game_directory = game_directory},
                              resource,
                              io,
                              out);
}

DirectoryRebaseRequest effectiveRequest(const DirectoryRebaseRequest& targeted,
                                        const DirectoryRebaseRequest& aggregate) {
  return targeted.requested ? targeted : aggregate;
}

}  // namespace

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation) {
  if (context.route_options) invocation.options = static_cast<const CinematicOptions&>(*context.route_options);
  invocation.texture_options = context.texture_options;
  invocation.sound_options = context.sound_options;
  invocation.output_converter = outputConverterDescriptor(context.route.output);
  const InputConverterDescriptor* input_converter = inputConverterDescriptor(context.route);
  if (!input_converter) {
    diagnostic(DiagnosticCode::kCinematicUnsupportedInput, "Unsupported Cinematic input converter");
    return false;
  }
  if (!invocation.output_converter) {
    diagnostic(DiagnosticCode::kCinematicUnsupportedOutput, "Unsupported Cinematic output converter");
    return false;
  }

  const ClassifiedPath& input = context.inputs[invocation.input];
  if (!resolveTextureInput(input, invocation.texture_options, context.io, invocation.textures) ||
      !resolveSoundInput(input, invocation.sound_options, context.io, invocation.sounds) ||
      !resolveOutputs(context, invocation))
    return false;

  const SidecarRebaseDirection automatic = automaticRebase(context);
  if (!resolveRebase(context.conversion.textures,
                     automatic,
                     kLooseTextureDirectory,
                     pistoris::paths::cinematicIllustrationDirectory(),
                     "illustration",
                     context.io,
                     invocation.texture_rebase) ||
      !resolveRebase(effectiveRequest(invocation.options.effects, context.conversion.sounds),
                     automatic,
                     kLooseSoundDirectory,
                     {},
                     "sound effect",
                     context.io,
                     invocation.effect_rebase) ||
      !resolveRebase(effectiveRequest(invocation.options.speech, context.conversion.sounds),
                     automatic,
                     kLooseSpeechDirectory,
                     {},
                     "speech",
                     context.io,
                     invocation.speech_rebase))
    return false;

  const bool requires_game_sidecar_bake =
      input.layout == ResourceLayout::kLoose && invocation.output.layout == ResourceLayout::kGame &&
      (invocation.texture_options.export_files || invocation.sound_options.export_files);
  const bool native =
      selectConversionPath(input_converter->load_native != nullptr,
                           invocation.output_converter->write_native != nullptr,
                           context.requires_intermediate || requires_game_sidecar_bake) == ConversionPath::kNative;
  if (!loadInput(*input_converter, context.inputs, invocation, native, invocation.state)) return false;
  IntermediateCinematic* intermediate = std::get_if<IntermediateCinematic>(&invocation.state);
  if (!intermediate) return true;
  if (invocation.texture_options.export_files &&
      !loadTextureImages(intermediate->cinematic, context.io, invocation.textures, intermediate->illustration_sources))
    return false;
  intermediate->illustration_sources.clear();
  if (!prepareCinematicSounds(intermediate->cinematic,
                              context.io,
                              invocation.sounds,
                              intermediate->sound_sources,
                              intermediate->sound_source_format,
                              invocation.sound_options.export_files))
    return false;
  intermediate->sound_sources.clear();
  return true;
}

}  // namespace cli::cinematic
