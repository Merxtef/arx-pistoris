// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "conversion/options.h"
#include "formats/options.h"
#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "resources/inventory_icon_io.h"
#include "resources/model_input_io.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/descriptor.h"
#include "routes/model/options.h"
#include "routes/model/state.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cli::model {

struct OutputConverterDescriptor;

struct AnimationSidecarOutput {
  std::size_t source = 0;
  OutputTarget target;
};

enum class AnimationSoundRebaseMode : std::uint8_t {
  kPreserve,
  kExplicit,
  kAutomatic,
};

struct Invocation final : RouteInvocation {
  std::size_t input = kNoClassifiedPath;
  OutputTarget output;
  std::vector<std::size_t> extras;
  ModelOptions options;
  SharedConversionOptions conversion;
  FormatOptions format;
  TextureIoOptions texture_options;
  TextureInput textures;
  TextureOutput texture_output;
  InventoryIconInput inventory_icon_input;
  InventoryIconOutput inventory_icon_output;
  SoundIoOptions sound_options;
  std::vector<SoundInput> sound_inputs;
  SoundOutput sound_output;
  ResolvedSidecarRebase texture_rebase;
  AnimationSoundRebaseMode sound_rebase_mode = AnimationSoundRebaseMode::kPreserve;
  ResolvedSidecarRebase sound_rebase;
  std::string animation_fallback_type;
  std::string preview_asset_name;
  std::vector<ModelMaterialLibraryInput> obj_material_libraries;
  OutputTarget obj_mtl_output;
  std::vector<AnimationSidecarOutput> animation_outputs;
  const OutputConverterDescriptor* output_converter = nullptr;
  ModelInput state;
};

struct ResolvedModelInvocation {
  const ExecutionContext& common;
  Invocation& invocation;
};

}  // namespace cli::model
