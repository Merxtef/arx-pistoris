// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/resolution.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "conversion/options.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "modules/module.h"
#include "pipeline/options.h"
#include "resources/input.h"
#include "resources/layout.h"
#include "resources/level_image_io.h"
#include "resources/level_json.h"
#include "resources/read_diagnostics.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/texture_io.h"
#include "routes/conversion_path.h"
#include "routes/descriptor.h"
#include "routes/level/invocation.h"
#include "routes/level/load.h"
#include "routes/level/native_carriers.h"
#include "routes/level/options.h"
#include "routes/level/save.h"
#include "routes/level/state.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::level {
namespace {

OutputTarget targetFromLocation(PathLocation location, Format format, ResourceLayout layout) {
  OutputTarget target;
  static_cast<PathLocation&>(target) = std::move(location);
  target.format = format;
  target.layout = layout;
  return target;
}

OutputTarget resourceTarget(std::string path, Format format) {
  return targetFromLocation(
      {.path = std::move(path), .address = PathAddress::kMountRelative}, format, ResourceLayout::kGame);
}

bool loadMinimapSamplerImage(IoService& io, std::string_view role, const std::string& requested,
                             media::PreparedImage& prepared, ArxEncodedImageView& view) {
  prepared = {};
  view = {};
  if (requested.empty()) return true;

  PathLocation location;
  std::string error;
  if (!io.resolvePathLocation(requested, location, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid,
               "Invalid minimap %.*s image path '%s': %s",
               static_cast<int>(role.size()),
               role.data(),
               requested.c_str(),
               error.c_str());
    return false;
  }

  std::vector<std::uint8_t> encoded;
  const ResourceReadResult result = io.readPath(location, encoded);
  if (result != ResourceReadResult::kSuccess) {
    std::string description = "Minimap ";
    description += role;
    description += " image";
    reportRequiredReadFailure(result, description, requested);
    return false;
  }

  const ArxReturnCode rc = media::prepareImage(std::move(encoded), prepared);
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kResourceInputInvalid,
               "Minimap %.*s image is invalid (%s): %s (code %d)",
               static_cast<int>(role.size()),
               role.data(),
               requested.c_str(),
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }
  view = {prepared.encoded.data(), prepared.encoded.size()};
  return true;
}

bool loadMinimapSamplerImages(Invocation& invocation, IoService& io) {
  if (!invocation.options.generate_minimap) return true;
  const MinimapSamplerPaths& paths = invocation.options.minimap_sampler_paths;
  pistoris::Level::MinimapGenerationOptions& generation = invocation.options.minimap_generation;
  return loadMinimapSamplerImage(
             io, "foreground", paths.foreground, invocation.minimap_foreground, generation.foreground.image) &&
         loadMinimapSamplerImage(
             io, "background", paths.background, invocation.minimap_background, generation.background.image) &&
         loadMinimapSamplerImage(io, "water", paths.water, invocation.minimap_water, generation.water.image) &&
         loadMinimapSamplerImage(io, "lava", paths.lava, invocation.minimap_lava, generation.lava.image);
}

bool discoverDlfCompanions(Invocation& invocation, std::vector<ClassifiedPath>& inputs, IoService& io,
                           std::optional<InputConverterDescriptor::DecodedDlfInput>& decoded_dlf) {
  if (invocation.input == kNoClassifiedPath || inputs[invocation.input].facts.format != Format::kDlf) return true;

  const std::size_t dlf_index = invocation.input;
  const ClassifiedPath& dlf_input = inputs[dlf_index];
  if (dlf_input.location.address == PathAddress::kAbsolute) {
    diagnostic(DiagnosticCode::kLevelInputFailed,
               "DLF Level input must be mount-relative; absolute paths cannot define a game resource layout: %s",
               dlf_input.path.c_str());
    return false;
  }

  PathLocation parent;
  PathLocation llf_location;
  std::string error;
  if (!io.parentPathLocation(dlf_input.location, parent, error) ||
      !io.appendPathLocation(parent, resourceFormatStem(dlf_input.path) + ".llf", llf_location, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid,
               "Cannot resolve sibling LLF path for '%s': %s",
               dlf_input.path.c_str(),
               error.c_str());
    return false;
  }

  std::vector<std::uint8_t> llf_buffer;
  const ResourceReadResult llf_result = io.readPath(llf_location, llf_buffer);
  if (llf_result == ResourceReadResult::kInvalidPath || llf_result == ResourceReadResult::kReadFailed) {
    diagnostic(DiagnosticCode::kResourceReadFailed, "Optional Level LLF cannot be read: %s", llf_location.path.c_str());
    return false;
  }

  InputConverterDescriptor::DecodedDlfInput prepared;
  std::optional<pistoris::Llf>* embedded_output =
      llf_result == ResourceReadResult::kSuccess ? nullptr : &prepared.embedded_lighting;
  const ArxReturnCode rc = decodeDlf(dlf_input, prepared.dlf, embedded_output);
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kLevelInputFailed,
               "DLF Level input failed: %s (code %d)",
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }

  std::string fts_path;
  if (!pistoris::paths::ftsFromDlfScene(prepared.dlf.scene_path, fts_path)) {
    diagnostic(
        DiagnosticCode::kLevelInputFailed, "DLF Level scene path is invalid: %s", prepared.dlf.scene_path.c_str());
    return false;
  }
  std::vector<std::uint8_t> fts_buffer;
  if (!readRequiredResource(io, fts_path, fts_buffer)) return false;

  const std::size_t positional_index = dlf_input.positional_index;
  const ArxResourceKind resource_kind = dlf_input.resource_kind;
  invocation.dlf = dlf_index;
  invocation.input = inputs.size();
  if (!appendClassifiedInput(fts_path,
                             {.path = fts_path, .address = PathAddress::kMountRelative},
                             std::move(fts_buffer),
                             positional_index,
                             resource_kind,
                             ResourceLayout::kGame,
                             inputs)) {
    return false;
  }
  if (llf_result == ResourceReadResult::kSuccess) {
    invocation.llf = inputs.size();
    std::string llf_path = llf_location.path;
    if (!appendClassifiedInput(std::move(llf_path),
                               std::move(llf_location),
                               std::move(llf_buffer),
                               positional_index,
                               resource_kind,
                               ResourceLayout::kGame,
                               inputs)) {
      return false;
    }
  }
  decoded_dlf.emplace(std::move(prepared));
  return true;
}

bool siblingTarget(IoService& io, const OutputTarget& primary, std::string_view filename, Format format,
                   OutputTarget& out) {
  PathLocation parent;
  PathLocation sibling;
  std::string error;
  if (!io.parentPathLocation(primary, parent, error) || !io.appendPathLocation(parent, filename, sibling, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid,
               "Cannot resolve sibling output '%.*s': %s",
               static_cast<int>(filename.size()),
               filename.data(),
               error.c_str());
    return false;
  }
  out = targetFromLocation(std::move(sibling), format, primary.layout);
  return true;
}

bool resolveScenePath(const LevelOptions& options, std::string_view level_name, IoService& io, std::string& out) {
  if (!options.fts_scene_directory_specified) {
    if (pistoris::paths::dlfSceneFromLevelName(level_name, out)) {
      out.push_back('/');
      return true;
    }
    diagnostic(DiagnosticCode::kResourcePathInvalid,
               "Cannot derive DLF scene directory from Level name '%.*s'",
               static_cast<int>(level_name.size()),
               level_name.data());
    return false;
  }

  std::string error;
  if (!io.normalizeResourcePath(options.fts_scene_directory, out, error)) {
    diagnostic(DiagnosticCode::kResourcePathInvalid,
               "Invalid --dlf-scene-directory '%s': %s",
               options.fts_scene_directory.c_str(),
               error.c_str());
    return false;
  }
  out.push_back('/');
  return true;
}

bool resolveInputTextures(Invocation& invocation, const std::vector<ClassifiedPath>& inputs,
                          const TextureIoOptions& options, IoService& io) {
  const bool native_input =
      inputs[invocation.input].facts.format == Format::kFts || inputs[invocation.input].facts.format == Format::kJson;
  invocation.textures.use_format_sources = inputs[invocation.input].layout == ResourceLayout::kLoose;
  invocation.textures.source_lookup = native_input ? ImageLookupMode::kGamePriority : ImageLookupMode::kExact;
  return resolveSidecarInputBase(inputs[invocation.input],
                                 invocation.textures.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 "Level",
                                 "texture",
                                 io,
                                 invocation.textures.source_base);
}

bool resolveTextureRebase(Invocation& invocation, const SharedConversionOptions& options,
                          SidecarRebaseDirection automatic, IoService& io) {
  return resolveSidecarRebase({.explicit_requested = options.rebase_textures,
                               .explicit_directory = options.texture_directory,
                               .automatic = automatic,
                               .to_loose_directory = kLooseTextureDirectory,
                               .to_game_directory = pistoris::paths::textureDirectory()},
                              "texture",
                              io,
                              invocation.rebase_textures,
                              invocation.texture_rebase_directory);
}

bool resolveTextureOutput(Invocation& invocation, IoService& io) {
  NativeOutput& output = invocation.native_output;
  output.textures.layout = invocation.output.layout;
  if (invocation.output.layout == ResourceLayout::kGame) return true;
  return resolveSidecarOutputBase(invocation.output, "Level", "texture", io, output.textures.base);
}

bool resolveNativeOutput(Invocation& invocation, Format output_format, const LevelOptions& options, IoService& io) {
  if (output_format != Format::kFts && output_format != Format::kDlf) return true;

  NativeOutput& output = invocation.native_output;
  const bool dlf_output = output_format == Format::kDlf;
  const OutputTarget& primary = invocation.output;
  const std::string level_name =
      primary.selector.kind == ARX_RESOURCE_KIND_LEVEL ? primary.selector.name : resourceFormatStem(primary.path);
  if (level_name.empty()) {
    diagnostic(DiagnosticCode::kLevelOutputFailed, "Native Level output requires a non-empty filename stem");
    return false;
  }
  if (!resolveScenePath(options, level_name, io, output.dlf_scene_path)) return false;
  if (!pistoris::paths::ftsFromDlfScene(output.dlf_scene_path, output.runtime_fts_path)) {
    diagnostic(DiagnosticCode::kResourcePathInvalid,
               "Cannot resolve runtime FTS path for DLF scene '%s'",
               output.dlf_scene_path.c_str());
    return false;
  }

  if (!dlf_output) {
    output.fts = primary;
    if (!siblingTarget(io, primary, level_name + ".llf", Format::kLlf, output.llf) ||
        !siblingTarget(io, primary, level_name + ".dlf", Format::kDlf, output.dlf)) {
      return false;
    }
    return true;
  }

  bool absolute = false;
  std::string error;
  if (!io.isAbsolutePath(primary.path, absolute, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Invalid DLF output path '%s': %s", primary.path.c_str(), error.c_str());
    return false;
  }
  if (absolute) {
    diagnostic(DiagnosticCode::kLevelOutputFailed,
               "DLF Level output must be mount-relative; absolute paths cannot define a game resource layout: %s",
               primary.path.c_str());
    return false;
  }

  output.dlf = primary;
  if (primary.selector.kind == ARX_RESOURCE_KIND_LEVEL) {
    output.llf = resourceTarget(pistoris::paths::levelLlf(primary.selector.level), Format::kLlf);
  } else if (!siblingTarget(io, primary, level_name + ".llf", Format::kLlf, output.llf)) {
    return false;
  }
  output.fts = resourceTarget(output.runtime_fts_path, Format::kFts);
  return true;
}

bool resolveJsonOutput(Invocation& invocation, Format output_format, IoService& io) {
  if (output_format != Format::kJson) return true;
  LevelJsonPath path;
  if (!parseLevelFtsJsonPath(invocation.output.path, path)) {
    diagnostic(DiagnosticCode::kLevelOutputFailed,
               "Level JSON output must use the central name level<N>.fts.json: %s",
               invocation.output.path.c_str());
    return false;
  }

  JsonOutput& output = invocation.json_output;
  output.fts = invocation.output;
  output.level = path.level;
  return siblingTarget(io, invocation.output, levelLlfJsonFilename(path), Format::kJson, output.llf) &&
         siblingTarget(io, invocation.output, levelDlfJsonFilename(path), Format::kJson, output.dlf);
}

void logPreviewFailure(std::string_view class_path, const char* reason) {
  log(ARX_LOG_WARN,
      "Level Model preview skipped (%.*s): %s",
      static_cast<int>(class_path.size()),
      class_path.data(),
      reason);
}

void logPreviewFailure(std::string_view class_path, const char* reason, ArxReturnCode rc) {
  log(ARX_LOG_WARN,
      "Level Model preview skipped (%.*s): %s: %s (code %d)",
      static_cast<int>(class_path.size()),
      class_path.data(),
      reason,
      pistoris::errorString(rc),
      static_cast<int>(rc));
}

bool loadModelPreviews(Invocation& invocation, IoService& io) {
  if (!invocation.options.load_previews) return true;
  IntermediateLevel* intermediate = std::get_if<IntermediateLevel>(&invocation.state);
  if (!intermediate) {
    diagnostic(DiagnosticCode::kLevelOutputFailed, "--load-previews requires intermediate Level data");
    return false;
  }

  std::vector<ArxLevelEntity> entities(intermediate->level.entityCount());
  const ArxReturnCode copy_rc = intermediate->level.copyEntities(0, entities.size(), entities.data());
  if (copy_rc != ARX_OK) {
    diagnostic(DiagnosticCode::kLevelOutputFailed,
               "Cannot inspect Level entities for Model previews: %s (code %d)",
               pistoris::errorString(copy_rc),
               static_cast<int>(copy_rc));
    return false;
  }

  std::vector<std::string_view> class_paths;
  class_paths.reserve(entities.size());
  for (const ArxLevelEntity& entity : entities)
    class_paths.emplace_back(entity.class_path.data, entity.class_path.size);
  std::ranges::sort(class_paths);
  class_paths.erase(std::ranges::unique(class_paths).begin(), class_paths.end());

  const TextureInput textures;
  for (std::string_view class_path : class_paths) {
    pistoris::paths::ModelPathView model_path;
    std::string ftl_path;
    if (!pistoris::paths::modelFromEntityClass(class_path, model_path) ||
        !pistoris::paths::modelFtl(model_path, ftl_path)) {
      logPreviewFailure(class_path, "class path does not map to a canonical FTL");
      continue;
    }

    std::vector<std::uint8_t> bytes;
    const ResourceReadResult read = io.readResource(ftl_path, bytes);
    if (read != ResourceReadResult::kSuccess) {
      logPreviewFailure(class_path, "FTL was not found or could not be read through active mounts");
      continue;
    }
    pistoris::Ftl native;
    ArxReturnCode rc = pistoris::readFtl(bytes, native);
    if (rc != ARX_OK) {
      logPreviewFailure(class_path, "FTL decoding failed", rc);
      continue;
    }
    auto model = std::make_unique<pistoris::Model>();
    rc = pistoris::Model::importNative(*model, native);
    if (rc == ARX_OK) rc = model->setResourcePath(ftl_path);
    if (rc == ARX_OK) rc = model->compactTextures();
    if (rc != ARX_OK) {
      logPreviewFailure(class_path, "Model preparation failed", rc);
      continue;
    }
    if (!loadTextureImages(*model, io, textures)) return false;
    invocation.model_previews.push_back(std::move(model));
  }
  if (!invocation.model_previews.empty())
    log(ARX_LOG_INFO, "loaded %zu Model preview(s)", invocation.model_previews.size());
  return true;
}

}  // namespace

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation) {
  invocation.output = context.output;
  if (context.route_options) invocation.options = static_cast<const LevelOptions&>(*context.route_options);
  invocation.format = context.format_options;
  invocation.texture_options = context.texture_options;
  applyFormatModifiers(invocation.options, context.format_modifiers);

  std::optional<InputConverterDescriptor::DecodedDlfInput> decoded_dlf;
  if (!discoverDlfCompanions(invocation, context.inputs, context.io, decoded_dlf)) return false;

  const InputConverterDescriptor* input_converter = inputConverterDescriptor(context.route);
  invocation.output_converter = outputConverterDescriptor(context.route.output, context.output_converter.module);
  if (!input_converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedInput, "Unsupported Level input format");
    return false;
  }
  if (!invocation.output_converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Unsupported Level output converter");
    return false;
  }
  if (invocation.options.load_previews && !invocation.output_converter->supports_model_previews) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput,
               "--load-previews is supported only by the default Level GLB exporter");
    return false;
  }

  const std::size_t image_input =
      invocation.dlf != kNoClassifiedPath && context.inputs[invocation.dlf].facts.format == Format::kDlf
          ? invocation.dlf
          : invocation.input;
  if (!resolveInputTextures(invocation, context.inputs, invocation.texture_options, context.io) ||
      !resolveLevelImageInput(context.inputs[image_input], context.io, invocation.image_input) ||
      !resolveLevelImageOutput(invocation.output, context.io, invocation.image_output) ||
      !resolveJsonOutput(invocation, context.route.output, context.io) ||
      !resolveNativeOutput(invocation, context.route.output, invocation.options, context.io) ||
      ((context.route.output == Format::kFts || context.route.output == Format::kDlf ||
        context.route.output == Format::kJson) &&
       !resolveTextureOutput(invocation, context.io))) {
    return false;
  }
  if (!loadMinimapSamplerImages(invocation, context.io)) return false;
  invocation.image_output.minimap_border_color = effectiveMinimapBorderColor(invocation.options);

  loadLevelImages(context.io, invocation.image_input, invocation.loaded_images);
  const bool native = selectConversionPath(input_converter->load_native != nullptr,
                                           invocation.output_converter->write_native != nullptr,
                                           context.requires_intermediate) == ConversionPath::kNative;
  const SidecarRebaseDirection automatic =
      native ? SidecarRebaseDirection::kNone
             : automaticSidecarRebase(sidecarEndpoint(context.inputs[invocation.input], ARX_RESOURCE_KIND_LEVEL),
                                      sidecarEndpoint(invocation.output, ARX_RESOURCE_KIND_LEVEL));
  if (!resolveTextureRebase(invocation, context.conversion, automatic, context.io)) return false;
  if (!loadInput(*input_converter,
                 context.inputs,
                 invocation,
                 invocation.options,
                 decoded_dlf ? &*decoded_dlf : nullptr,
                 native,
                 invocation.state))
    return false;
  if (IntermediateLevel* intermediate = std::get_if<IntermediateLevel>(&invocation.state))
    applyLevelImages(intermediate->level, invocation.loaded_images);
  if (invocation.texture_options.export_files) {
    if (IntermediateLevel* intermediate = std::get_if<IntermediateLevel>(&invocation.state)) {
      if (!loadTextureImages(intermediate->level, context.io, invocation.textures, intermediate->texture_source_paths))
        return false;
      intermediate->texture_source_paths.clear();
    }
  }
  return loadModelPreviews(invocation, context.io);
}

}  // namespace cli::level
