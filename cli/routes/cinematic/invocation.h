// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/options.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/cinematic/options.h"
#include "routes/cinematic/state.h"
#include "routes/descriptor.h"

#include <cstddef>

namespace cli::cinematic {

struct OutputConverterDescriptor;

struct Invocation final : RouteInvocation {
  std::size_t input = kNoClassifiedPath;
  OutputTarget output;
  CinematicOptions options;
  const OutputConverterDescriptor* output_converter = nullptr;
  CinematicInput state;
  TextureIoOptions texture_options;
  SoundIoOptions sound_options;
  TextureInput textures;
  TextureOutput texture_output;
  SoundInput sounds;
  SoundOutput sound_output;
  ResolvedSidecarRebase texture_rebase;
  ResolvedSidecarRebase effect_rebase;
  ResolvedSidecarRebase speech_rebase;
};

}  // namespace cli::cinematic
