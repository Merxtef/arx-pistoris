// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "formats/classification.h"
#include "formats/options.h"
#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "resources/sound_io.h"
#include "routes/animation/state.h"
#include "routes/descriptor.h"

#include <cstddef>
#include <string>

namespace cli::animation {

struct OutputConverterDescriptor;

struct Invocation final : RouteInvocation {
  std::size_t input = kNoClassifiedPath;
  OutputTarget output;
  SharedConversionOptions conversion;
  FormatOptions format;
  SoundIoOptions sound_options;
  SoundInput sound_input;
  SoundOutput sound_output;
  bool rebase_sounds = false;
  std::string sound_rebase_directory;
  const OutputConverterDescriptor* output_converter = nullptr;
  AnimationInput state;
};

struct ResolvedAnimationInvocation {
  const ExecutionContext& common;
  Invocation& invocation;
};

}  // namespace cli::animation
