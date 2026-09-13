// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/resolution.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "io/service.h"
#include "pipeline/options.h"
#include "resources/layout.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "routes/animation/invocation.h"
#include "routes/animation/load.h"
#include "routes/animation/save.h"
#include "routes/animation/state.h"
#include "routes/conversion_path.h"
#include "routes/descriptor.h"

#include <variant>

namespace cli::animation {
namespace {

bool resolveSoundInput(const ClassifiedPath& input, const SoundIoOptions& options, IoService& io, SoundInput& out) {
  out.use_format_sources = input.layout == ResourceLayout::kLoose;
  return resolveSidecarInputBase(input,
                                 out.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 "Animation",
                                 "sound",
                                 io,
                                 out.source_base);
}

bool resolveSounds(const RouteResolveContext& context, Invocation& invocation, bool native) {
  if (!resolveSoundInput(
          context.inputs[invocation.input], invocation.sound_options, context.io, invocation.sound_input))
    return false;
  invocation.sound_output.layout = invocation.output.layout;
  if (invocation.output.layout == ResourceLayout::kLoose &&
      !resolveSidecarOutputBase(invocation.output, "Animation", "sound", context.io, invocation.sound_output.base))
    return false;
  const SidecarRebaseDirection automatic =
      native ? SidecarRebaseDirection::kNone
             : automaticSidecarRebase(sidecarEndpoint(context.inputs[invocation.input], ARX_RESOURCE_KIND_ANIMATION),
                                      sidecarEndpoint(invocation.output, ARX_RESOURCE_KIND_ANIMATION));
  return resolveSidecarRebase({.explicit_requested = context.conversion.rebase_sounds,
                               .explicit_directory = context.conversion.sound_directory,
                               .automatic = automatic,
                               .to_loose_directory = kLooseSoundDirectory,
                               .to_game_directory = pistoris::paths::soundDirectory()},
                              "sound",
                              context.io,
                              invocation.rebase_sounds,
                              invocation.sound_rebase_directory);
}

}  // namespace

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation) {
  invocation.conversion = context.conversion;
  invocation.format = context.format_options;
  invocation.sound_options = context.sound_options;
  const InputConverterDescriptor* input_converter = inputConverterDescriptor(context.route);
  invocation.output_converter = outputConverterDescriptor(context.route.output);
  if (!input_converter) {
    diagnostic(DiagnosticCode::kAnimationUnsupportedInput, "Unsupported Animation input converter");
    return false;
  }
  if (!invocation.output_converter) {
    diagnostic(DiagnosticCode::kAnimationUnsupportedOutput, "Unsupported Animation output converter");
    return false;
  }
  const bool native = selectConversionPath(input_converter->load_native != nullptr,
                                           invocation.output_converter->write_native != nullptr,
                                           context.requires_intermediate) == ConversionPath::kNative;
  if (!resolveSounds(context, invocation, native)) return false;
  if (!loadInput(*input_converter, context.inputs, invocation, native, invocation.state)) return false;
  if (invocation.sound_options.export_files) {
    if (IntermediateAnimation* intermediate = std::get_if<IntermediateAnimation>(&invocation.state)) {
      if (!loadSoundData(intermediate->animation, context.io, invocation.sound_input, intermediate->sound_sources))
        return false;
      intermediate->sound_sources.clear();
    }
  }
  return true;
}

}  // namespace cli::animation
