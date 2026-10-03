// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "media/encoded.h"
#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "resources/level_image_io.h"
#include "resources/sidecar_io.h"
#include "resources/texture_io.h"
#include "routes/descriptor.h"
#include "routes/level/options.h"
#include "routes/level/state.h"

#include <arx_pistoris/model.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cli::level {

struct OutputConverterDescriptor;

struct NativeOutput {
  OutputTarget fts;
  OutputTarget llf;
  OutputTarget dlf;
  TextureOutput textures;
  std::string dlf_scene_path;
  std::string runtime_fts_path;
};

struct JsonOutput {
  OutputTarget fts;
  OutputTarget llf;
  OutputTarget dlf;
  std::uint32_t level = 0;
};

struct Invocation final : RouteInvocation {
  std::size_t input = kNoClassifiedPath;
  std::size_t llf = kNoClassifiedPath;
  std::size_t dlf = kNoClassifiedPath;
  OutputTarget output;
  TextureInput textures;
  TextureIoOptions texture_options;
  ResolvedSidecarRebase texture_rebase;
  NativeOutput native_output;
  JsonOutput json_output;
  std::vector<std::unique_ptr<pistoris::Model>> model_previews;
  LevelImageInput image_input;
  LevelImageOutput image_output;
  LoadedLevelImages loaded_images;
  media::PreparedImage minimap_foreground;
  media::PreparedImage minimap_background;
  media::PreparedImage minimap_water;
  media::PreparedImage minimap_lava;
  LevelOptions options;
  FormatOptions format;
  const OutputConverterDescriptor* output_converter = nullptr;
  LevelInput state;
};

struct ResolvedLevelInvocation {
  const ExecutionContext& common;
  Invocation& invocation;
};

}  // namespace cli::level
