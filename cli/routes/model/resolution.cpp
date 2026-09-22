// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/resolution.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "pipeline/options.h"
#include "resources/inventory_icon_io.h"
#include "resources/layout.h"
#include "resources/model_input_io.h"
#include "resources/read_diagnostics.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/conversion_path.h"
#include "routes/descriptor.h"
#include "routes/model/animation_sidecars.h"
#include "routes/model/invocation.h"
#include "routes/model/load.h"
#include "routes/model/options.h"
#include "routes/model/save.h"
#include "routes/model/state.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::model {
namespace {

bool resolveTextureOutput(const RouteResolveContext& context, Invocation& invocation) {
  if (context.route.output != Format::kObj && context.route.output != Format::kFtl &&
      context.route.output != Format::kJson)
    return true;
  invocation.texture_output.layout = invocation.output.layout;
  if (invocation.output.layout == ResourceLayout::kGame) return true;
  const char* owner = context.route.output == Format::kObj ? "OBJ" : "Model";
  return resolveSidecarOutputBase(invocation.output, owner, "texture", context.io, invocation.texture_output.base);
}

bool resolveSoundInput(const ClassifiedPath& input, const SoundIoOptions& options, IoService& io, SoundInput& out) {
  out.use_format_sources = input.layout == ResourceLayout::kLoose;
  return resolveSidecarInputBase(input,
                                 out.use_format_sources,
                                 options.input_folder_specified,
                                 options.input_folder,
                                 "Model",
                                 "sound",
                                 io,
                                 out.source_base);
}

bool resolveSounds(const RouteResolveContext& context, Invocation& invocation) {
  invocation.sound_inputs.resize(context.inputs.size());
  if (context.inputs[invocation.input].facts.format == Format::kGlb &&
      !resolveSoundInput(context.inputs[invocation.input],
                         invocation.sound_options,
                         context.io,
                         invocation.sound_inputs[invocation.input]))
    return false;
  for (const std::size_t index : invocation.extras)
    if (!resolveSoundInput(context.inputs[index], invocation.sound_options, context.io, invocation.sound_inputs[index]))
      return false;

  invocation.sound_output.layout = invocation.output.layout;
  if (invocation.output.layout == ResourceLayout::kLoose && invocation.output_converter->uses_sound_files) {
    if (!resolveSidecarOutputBase(invocation.output, "Model", "sound", context.io, invocation.sound_output.base))
      return false;
  }
  return true;
}

bool resolveTextureRebase(const RouteResolveContext& context, Invocation& invocation, bool native) {
  const SidecarRebaseDirection automatic =
      native ? SidecarRebaseDirection::kNone
             : automaticSidecarRebase(sidecarEndpoint(context.inputs[invocation.input], ARX_RESOURCE_KIND_MODEL),
                                      sidecarEndpoint(invocation.output, ARX_RESOURCE_KIND_MODEL));
  return resolveSidecarRebase({.explicit_requested = context.conversion.textures.requested,
                               .explicit_directory = context.conversion.textures.directory,
                               .automatic = automatic,
                               .to_loose_directory = kLooseTextureDirectory,
                               .to_game_directory = pistoris::paths::textureDirectory()},
                              "texture",
                              context.io,
                              invocation.texture_rebase);
}

bool resolveSoundRebase(const RouteResolveContext& context, Invocation& invocation, bool native) {
  const SidecarRebaseDirection automatic =
      native ? SidecarRebaseDirection::kNone
             : automaticSidecarRebaseTarget(sidecarEndpoint(invocation.output, ARX_RESOURCE_KIND_MODEL));
  if (!resolveSidecarRebase({.explicit_requested = context.conversion.sounds.requested,
                             .explicit_directory = context.conversion.sounds.directory,
                             .automatic = automatic,
                             .to_loose_directory = kLooseSoundDirectory,
                             .to_game_directory = pistoris::paths::soundDirectory()},
                            "sound",
                            context.io,
                            invocation.sound_rebase)) {
    return false;
  }
  if (context.conversion.sounds.requested)
    invocation.sound_rebase_mode = AnimationSoundRebaseMode::kExplicit;
  else if (automatic != SidecarRebaseDirection::kNone)
    invocation.sound_rebase_mode = AnimationSoundRebaseMode::kAutomatic;
  return true;
}

bool resolveAnimationFallbackType(const RouteResolveContext& context, Invocation& invocation) {
  if (context.route.output != Format::kFtl || invocation.output.layout != ResourceLayout::kGame) return true;
  std::string_view model_type;
  pistoris::paths::ModelPathView parsed;
  if (invocation.output.selector.kind == ARX_RESOURCE_KIND_MODEL) {
    model_type = invocation.output.selector.type;
  } else if (pistoris::paths::modelFromFtl(invocation.output.path, parsed)) {
    model_type = parsed.type;
  }
  if (!model_type.empty()) {
    invocation.animation_fallback_type = model_type == "npc" ? "npc" : "fix_inter";
    return true;
  }

  bool npc = false;
  std::string error;
  if (!context.io.hasPathComponent(invocation.output, "npc", npc, error)) {
    diagnostic(DiagnosticCode::kModelOutputFailed,
               "Cannot inspect Model output path for Animation placement: %s",
               error.c_str());
    return false;
  }
  invocation.animation_fallback_type = npc ? "npc" : "fix_inter";
  return true;
}

std::string previewAssetName(std::string_view path) {
  std::string result(path);
  for (;;) {
    std::string stripped = resourceFormatStem(result);
    if (stripped == result) break;
    result = std::move(stripped);
  }
  constexpr std::string_view kBaseSuffix = "_base";
  if (result.size() > kBaseSuffix.size() && result.ends_with(kBaseSuffix)) result.resize(result.size() - 5U);
  return result.empty() ? "asset" : result;
}

bool loadAnimationSounds(IntermediateModel& intermediate, const RouteResolveContext& context,
                         const Invocation& invocation) {
  if (intermediate.animations.size() != intermediate.sound_sources.size() ||
      intermediate.animations.size() != intermediate.animation_sources.size()) {
    diagnostic(DiagnosticCode::kModelInputFailed, "Model Animation source metadata is inconsistent");
    return false;
  }
  const auto load = [&](std::size_t index) {
    if (intermediate.animation_sources[index].input >= invocation.sound_inputs.size()) {
      diagnostic(DiagnosticCode::kModelInputFailed, "Model Animation has an invalid source index");
      return false;
    }
    return !intermediate.animations[index] ||
           loadSoundData(*intermediate.animations[index],
                         context.io,
                         invocation.sound_inputs[intermediate.animation_sources[index].input],
                         intermediate.sound_sources[index]);
  };

  if (context.route.output == Format::kFtl || context.route.output == Format::kJson) {
    for (const AnimationSidecarOutput& output : invocation.animation_outputs)
      if (!load(output.source)) return false;
    return true;
  }
  for (std::size_t index = 0; index < intermediate.animations.size(); ++index)
    if (!load(index)) return false;
  return true;
}

bool reportInventoryIconError(const InventoryIconInput& input, const LoadedInventoryIcon& loaded, ArxReturnCode rc,
                              ArxReturnCode bad_image) {
  std::string_view path = loaded.resolved_path;
  if (path.empty()) path = input.exact ? std::string_view(input.location.path) : std::string_view(input.path);
  if (rc == bad_image) {
    if (!input.required) {
      log(ARX_LOG_WARN, "Invalid Model inventory icon was skipped: %.*s", static_cast<int>(path.size()), path.data());
      return true;
    }
    diagnostic(DiagnosticCode::kModelInputFailed,
               "Inventory icon is invalid: %.*s",
               static_cast<int>(path.size()),
               path.data());
    return false;
  }
  diagnostic(DiagnosticCode::kModelInputFailed,
             "Inventory icon input failed (%.*s): %s (code %d)",
             static_cast<int>(path.size()),
             path.data(),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool loadInventoryIcon(Invocation& invocation, IoService& io) {
  LoadedInventoryIcon loaded;
  if (!cli::readInventoryIcon(io, invocation.inventory_icon_input, loaded)) return false;
  if (loaded.encoded.empty()) return true;
  if (NativeModelFiles* native = std::get_if<NativeModelFiles>(&invocation.state)) {
    media::PreparedImage prepared;
    const ArxReturnCode rc = media::prepareImage(std::move(loaded.encoded), prepared);
    if (rc != ARX_OK) return reportInventoryIconError(invocation.inventory_icon_input, loaded, rc, ARX_IMAGE_BAD_DATA);
    native->inventory_icon = std::move(prepared.encoded);
    native->inventory_icon_format = prepared.info.format;
    return true;
  }
  IntermediateModel* intermediate = std::get_if<IntermediateModel>(&invocation.state);
  if (!intermediate) return true;
  const ArxReturnCode rc = intermediate->model.setInventoryIcon({loaded.encoded.data(), loaded.encoded.size()},
                                                                invocation.options.inventory_icon_set);
  if (rc == ARX_OK) return true;
  return reportInventoryIconError(invocation.inventory_icon_input, loaded, rc, ARX_MODEL_BAD_INVENTORY_ICON);
}

}  // namespace

bool resolveInvocation(const RouteResolveContext& context, Invocation& invocation) {
  if (context.route_options) invocation.options = static_cast<const ModelOptions&>(*context.route_options);
  invocation.conversion = context.conversion;
  invocation.format = context.format_options;
  invocation.texture_options = context.texture_options;
  invocation.sound_options = context.sound_options;
  applyFormatModifiers(invocation.options, context.format_modifiers);
  if (context.format_modifiers.glb.arx_units_per_unit) {
    invocation.options.level_preview_glb.arx_units_per_glb_unit = *context.format_modifiers.glb.arx_units_per_unit;
  }

  const InputConverterDescriptor* input_converter = inputConverterDescriptor(context.route);
  invocation.output_converter = outputConverterDescriptor(context.route.output, context.output_converter.module);
  if (!input_converter) {
    diagnostic(DiagnosticCode::kModelUnsupportedInput, "Unsupported Model input converter");
    return false;
  }
  if (!invocation.output_converter) {
    diagnostic(DiagnosticCode::kModelUnsupportedOutput, "Unsupported Model output converter");
    return false;
  }
  if (!resolveModelTextureInput(
          context.inputs[invocation.input], invocation.texture_options, context.io, "Model", invocation.textures) ||
      !resolveTextureOutput(context, invocation) || !resolveAnimationFallbackType(context, invocation)) {
    return false;
  }
  if (!resolveSounds(context, invocation)) return false;
  if (!resolveInventoryIconInput(context.inputs[invocation.input],
                                 invocation.options.input_icon ? invocation.options.input_icon : "",
                                 context.io,
                                 invocation.inventory_icon_input) ||
      !resolveInventoryIconOutput(invocation.output, context.io, invocation.inventory_icon_output))
    return false;
  invocation.preview_asset_name = previewAssetName(context.inputs[invocation.input].path);

  if (!readModelMaterialLibraries(context.inputs[invocation.input],
                                  context.io,
                                  DiagnosticCode::kModelInputFailed,
                                  "Model",
                                  invocation.obj_material_libraries))
    return false;

  if (context.route.output == Format::kObj) {
    PathLocation parent;
    PathLocation mtl;
    std::string error;
    if (!context.io.parentPathLocation(invocation.output, parent, error) ||
        !context.io.appendPathLocation(parent, resourceStem(invocation.output.path) + ".mtl", mtl, error)) {
      diagnostic(DiagnosticCode::kIoPathInvalid, "Cannot resolve OBJ material output: %s", error.c_str());
      return false;
    }
    static_cast<PathLocation&>(invocation.obj_mtl_output) = std::move(mtl);
  }

  const bool native = selectConversionPath(input_converter->load_native != nullptr,
                                           invocation.output_converter->write_native != nullptr,
                                           context.requires_intermediate) == ConversionPath::kNative;
  if (!resolveTextureRebase(context, invocation, native) || !resolveSoundRebase(context, invocation, native))
    return false;
  if (!loadInput(*input_converter, context.inputs, invocation, invocation.options, native, invocation.state))
    return false;
  if (!loadInventoryIcon(invocation, context.io)) return false;
  if (!resolveAnimationSidecarOutputs(invocation)) return false;
  if (invocation.texture_options.export_files) {
    if (IntermediateModel* intermediate = std::get_if<IntermediateModel>(&invocation.state)) {
      if (!loadTextureImages(intermediate->model, context.io, invocation.textures, intermediate->texture_source_paths))
        return false;
      intermediate->texture_source_paths.clear();
    }
  }
  if (invocation.sound_options.export_files && invocation.output_converter->uses_sound_files) {
    if (IntermediateModel* intermediate = std::get_if<IntermediateModel>(&invocation.state)) {
      if (!loadAnimationSounds(*intermediate, context, invocation)) return false;
      intermediate->sound_sources.clear();
    }
  }

  if (invocation.options.reference_ftl) {
    IntermediateModel* intermediate = std::get_if<IntermediateModel>(&invocation.state);
    if (!intermediate) {
      diagnostic(DiagnosticCode::kModelModuleInvalid, "--ftl-reference requires intermediate Model data");
      return false;
    }
    std::vector<std::uint8_t> reference;
    std::string resolved_path;
    const ResourceReadResult result = context.io.readPath(invocation.options.reference_ftl, reference, &resolved_path);
    if (result != ResourceReadResult::kSuccess) {
      reportRequiredReadFailure(result, "Reference FTL", invocation.options.reference_ftl);
      return false;
    }
    if (!loadReferenceModel(reference, resolved_path, invocation.native_text_mode, *intermediate)) return false;
  }
  return true;
}

}  // namespace cli::model
