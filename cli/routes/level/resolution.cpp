// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/resolution.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "modules/module.h"
#include "resources/input.h"
#include "resources/level_json.h"
#include "resources/selector.h"
#include "routes/descriptor.h"
#include "routes/level/invocation.h"
#include "routes/level/load.h"
#include "routes/level/native_input.h"
#include "routes/level/options.h"
#include "routes/level/save.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli::level {
namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

OutputTarget targetFromLocation(PathLocation location, Format format) {
  OutputTarget target;
  static_cast<PathLocation&>(target) = std::move(location);
  target.format = format;
  return target;
}

OutputTarget resourceTarget(std::string path, Format format) {
  return targetFromLocation({.path = std::move(path), .address = PathAddress::kMountRelative}, format);
}

PathLocation resourceLocation(std::string path) {
  return {.path = std::move(path), .address = PathAddress::kMountRelative};
}

bool readRequiredResource(IoService& io, std::string_view path, std::vector<std::uint8_t>& out) {
  const ResourceReadResult result = io.readResource(path, out);
  switch (result) {
    case ResourceReadResult::kSuccess:
      return true;
    case ResourceReadResult::kNotFound:
      diagnostic(DiagnosticCode::kResourceNotFound,
                 "Mounted resource not found: %.*s",
                 static_cast<int>(path.size()),
                 path.data());
      return false;
    case ResourceReadResult::kInvalidPath:
      diagnostic(DiagnosticCode::kResourceSelectorInvalid,
                 "Invalid mounted resource path: %.*s",
                 static_cast<int>(path.size()),
                 path.data());
      return false;
    case ResourceReadResult::kReadFailed:
      diagnostic(DiagnosticCode::kResourceReadFailed,
                 "Mounted resource cannot be read: %.*s",
                 static_cast<int>(path.size()),
                 path.data());
      return false;
  }
  return false;
}

bool discoverDlfCompanions(Invocation& invocation, std::vector<ClassifiedPath>& inputs, IoService& io) {
  if (invocation.input == kNoClassifiedPath || inputs[invocation.input].facts.format != Format::kDlf) return true;

  const std::size_t dlf_index = invocation.input;
  const ClassifiedPath& dlf_input = inputs[dlf_index];
  if (dlf_input.location.address == PathAddress::kAbsolute) {
    diagnostic(DiagnosticCode::kResourceSelectorInvalid,
               "DLF Level input must be mount-relative; absolute paths cannot define a game resource layout: %s",
               dlf_input.path.c_str());
    return false;
  }

  pistoris::Dlf dlf;
  const ArxReturnCode rc = decodeDlf(dlf_input, dlf);
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kLevelInputFailed,
               "DLF Level input failed: %s (code %d)",
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }

  std::string fts_path;
  if (!pistoris::paths::ftsFromDlfScene(dlf.scene_path, fts_path)) {
    diagnostic(DiagnosticCode::kLevelInputFailed, "DLF Level scene path is invalid: %s", dlf.scene_path.c_str());
    return false;
  }
  std::vector<std::uint8_t> fts_buffer;
  if (!readRequiredResource(io, fts_path, fts_buffer)) return false;

  PathLocation parent;
  PathLocation llf_location;
  std::string error;
  if (!io.parentPathLocation(dlf_input.location, parent, error) ||
      !io.appendPathLocation(parent, resourceStem(dlf_input.path, false) + ".llf", llf_location, error)) {
    diagnostic(DiagnosticCode::kResourceSelectorInvalid,
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

  const std::size_t positional_index = dlf_input.positional_index;
  const ArxResourceKind resource_kind = dlf_input.resource_kind;
  invocation.dlf = dlf_index;
  invocation.input = inputs.size();
  if (!appendClassifiedInput(
          fts_path, resourceLocation(fts_path), std::move(fts_buffer), positional_index, resource_kind, inputs)) {
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
                               inputs)) {
      return false;
    }
  }
  return true;
}

bool siblingTarget(IoService& io, const OutputTarget& primary, std::string_view filename, Format format,
                   OutputTarget& out) {
  PathLocation parent;
  PathLocation sibling;
  std::string error;
  if (!io.parentPathLocation(primary, parent, error) || !io.appendPathLocation(parent, filename, sibling, error)) {
    diagnostic(DiagnosticCode::kIoCreateFailed,
               "Cannot resolve sibling output '%.*s': %s",
               static_cast<int>(filename.size()),
               filename.data(),
               error.c_str());
    return false;
  }
  out = targetFromLocation(std::move(sibling), format);
  return true;
}

bool resolveScenePath(const LevelOptions& options, std::string_view level_name, IoService& io, std::string& out) {
  if (!options.fts_scene_directory_specified) {
    if (pistoris::paths::dlfSceneFromLevelName(level_name, out)) {
      out.push_back('/');
      return true;
    }
    diagnostic(DiagnosticCode::kLevelOutputFailed,
               "Cannot derive DLF scene directory from Level name '%.*s'",
               static_cast<int>(level_name.size()),
               level_name.data());
    return false;
  }

  std::string error;
  if (!io.normalizeResourcePath(options.fts_scene_directory, out, error)) {
    diagnostic(DiagnosticCode::kLevelOutputFailed,
               "Invalid --fts-scene-directory '%s': %s",
               options.fts_scene_directory.c_str(),
               error.c_str());
    return false;
  }
  out.push_back('/');
  return true;
}

bool resolveInputTextures(Invocation& invocation, const std::vector<ClassifiedPath>& inputs,
                          const LevelOptions& options, IoService& io) {
  if (options.input_texture_folder_specified) {
    std::string error;
    if (!io.resolvePathLocation(options.input_texture_folder, invocation.textures.folder, error)) {
      diagnostic(DiagnosticCode::kResourceSelectorInvalid,
                 "Invalid input texture folder '%s': %s",
                 options.input_texture_folder.c_str(),
                 error.c_str());
      return false;
    }
    invocation.textures.mode = TextureLookupMode::kFlatFolder;
    return true;
  }

  const bool dlf_layout = inputs[invocation.input].facts.format != Format::kJson &&
                          invocation.dlf != kNoClassifiedPath &&
                          inputs[invocation.dlf].positional_index == inputs[invocation.input].positional_index;
  if (dlf_layout || inputs[invocation.input].facts.format == Format::kGlb) {
    invocation.textures.mode = TextureLookupMode::kGameResources;
    return true;
  }

  PathLocation parent;
  std::string error;
  if (!io.parentPathLocation(inputs[invocation.input].location, parent, error) ||
      !io.appendPathLocation(parent, "textures", invocation.textures.folder, error)) {
    diagnostic(
        DiagnosticCode::kResourceSelectorInvalid, "Cannot resolve default input texture folder: %s", error.c_str());
    return false;
  }
  invocation.textures.mode = TextureLookupMode::kFlatFolder;
  return true;
}

bool resolveTextureOutput(NativeOutput& output, bool dlf_layout, const LevelOptions& options, IoService& io) {
  std::string directory = dlf_layout ? "graph/obj3d/textures" : "textures";
  if (options.output_texture_folder_specified) {
    std::string error;
    if (!io.normalizeResourcePath(options.output_texture_folder, directory, error)) {
      diagnostic(DiagnosticCode::kIoCreateFailed,
                 "Invalid output texture resource folder '%s': %s",
                 options.output_texture_folder.c_str(),
                 error.c_str());
      return false;
    }
  }
  for (char& value : directory) value = lowerAscii(value);

  PathLocation base;
  std::string error;
  if (!dlf_layout && !io.parentPathLocation(output.fts, base, error)) {
    diagnostic(DiagnosticCode::kIoCreateFailed, "Cannot resolve loose Level output directory: %s", error.c_str());
    return false;
  }
  if (!io.appendPathLocation(base, directory, output.texture_folder, error)) {
    diagnostic(DiagnosticCode::kIoCreateFailed,
               "Cannot resolve texture output folder '%s': %s",
               directory.c_str(),
               error.c_str());
    return false;
  }
  output.texture_resource_directory = std::move(directory);
  return true;
}

bool resolveNativeOutput(Invocation& invocation, Format output_format, const LevelOptions& options, IoService& io) {
  if (output_format != Format::kFts && output_format != Format::kDlf) return true;

  NativeOutput& output = invocation.native_output;
  const bool dlf_layout = output_format == Format::kDlf;
  const OutputTarget& primary = invocation.output;
  const std::string level_name =
      primary.selector.kind == ARX_RESOURCE_KIND_LEVEL ? primary.selector.name : resourceStem(primary.path, false);
  if (level_name.empty()) {
    diagnostic(DiagnosticCode::kLevelOutputFailed, "Native Level output requires a non-empty filename stem");
    return false;
  }
  if (!resolveScenePath(options, level_name, io, output.dlf_scene_path) ||
      !pistoris::paths::ftsFromDlfScene(output.dlf_scene_path, output.runtime_fts_path)) {
    diagnostic(DiagnosticCode::kLevelOutputFailed,
               "Cannot resolve runtime FTS path for DLF scene '%s'",
               output.dlf_scene_path.c_str());
    return false;
  }

  if (!dlf_layout) {
    output.fts = primary;
    if (!siblingTarget(io, primary, level_name + ".llf", Format::kLlf, output.llf) ||
        !siblingTarget(io, primary, level_name + ".dlf", Format::kDlf, output.dlf)) {
      return false;
    }
    output.detached_layout = true;
    return resolveTextureOutput(output, false, options, io);
  }

  bool absolute = false;
  std::string error;
  if (!io.isAbsolutePath(primary.path, absolute, error)) {
    diagnostic(
        DiagnosticCode::kIoCreateFailed, "Invalid DLF output path '%s': %s", primary.path.c_str(), error.c_str());
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
  return resolveTextureOutput(output, true, options, io);
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

bool requiresIntermediate(std::span<const ModuleInvocation> modules) {
  for (const ModuleInvocation& invocation : modules) {
    if (!invocation.module) continue;
    switch (invocation.module->category()) {
      case ModuleCategory::kRoute:
      case ModuleCategory::kSharedConversion:
      case ModuleCategory::kNativeBakeModifier:
        return true;
      default:
        break;
    }
  }
  return false;
}

}  // namespace

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation) {
  invocation.output = context.output;
  if (context.route_options) invocation.options = static_cast<const LevelOptions&>(*context.route_options);
  invocation.format = context.options.format;
  applyFormatModifiers(invocation.options, context.options.format_modifiers);

  if (!discoverDlfCompanions(invocation, context.inputs, context.io)) return false;

  const InputConverterDescriptor* input_converter = inputConverterDescriptor(context.route);
  invocation.output_converter = outputConverterDescriptor(context.output_format, context.output_converter.module);
  if (!input_converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedInput, "Unsupported Level input format");
    return false;
  }
  if (!invocation.output_converter) {
    diagnostic(DiagnosticCode::kLevelUnsupportedOutput, "Unsupported Level output converter");
    return false;
  }

  if (!resolveInputTextures(invocation, context.inputs, invocation.options, context.io) ||
      !resolveJsonOutput(invocation, context.output_format, context.io) ||
      !resolveNativeOutput(invocation, context.output_format, invocation.options, context.io)) {
    return false;
  }

  const bool native = input_converter->load_native && invocation.output_converter->write_native &&
                      !requiresIntermediate(context.modules);
  return loadInput(*input_converter, context.inputs, invocation, invocation.options, native, invocation.state);
}

}  // namespace cli::level
