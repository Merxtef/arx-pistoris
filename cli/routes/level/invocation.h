// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "pipeline/execution_context.h"
#include "pipeline/options.h"
#include "routes/descriptor.h"
#include "routes/level/options.h"
#include "routes/level/state.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace cli::level {

struct OutputConverterDescriptor;

enum class TextureLookupMode : std::uint8_t {
  kGameResources,
  kFlatFolder,
};

struct TextureInput {
  TextureLookupMode mode = TextureLookupMode::kGameResources;
  PathLocation folder;
};

struct NativeOutput {
  OutputTarget fts;
  OutputTarget llf;
  OutputTarget dlf;
  PathLocation texture_folder;
  std::string texture_resource_directory;
  std::string dlf_scene_path;
  std::string runtime_fts_path;
  bool detached_layout = false;
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
  NativeOutput native_output;
  JsonOutput json_output;
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
