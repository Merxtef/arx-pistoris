// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/model.hpp"

#include "formats/options.h"
#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "resources/selector.h"
#include "resources/sound_io.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/state.h"
#include "routes/descriptor.h"

#include <cstddef>
#include <memory>
#include <string>

namespace cli::ambiance {

struct OutputConverterDescriptor;

struct Invocation final : RouteInvocation {
  std::size_t input = kNoClassifiedPath;
  OutputTarget output;
  AmbianceOptions options;
  FormatOptions format;
  const OutputConverterDescriptor* output_converter = nullptr;
  AmbianceInput state;
  std::unique_ptr<pistoris::Model> reference_model;
  SoundIoOptions sound_options;
  SoundInput sounds;
  SoundOutput sound_output;
  bool rebase_sounds = false;
  std::string sound_rebase_directory;
};

struct ResolvedAmbianceInvocation {
  const ExecutionContext& common;
  Invocation& invocation;
};

}  // namespace cli::ambiance
