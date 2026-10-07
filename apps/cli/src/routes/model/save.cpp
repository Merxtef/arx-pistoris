// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/save.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/glb.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/format.h"
#include "modules/module.h"
#include "pipeline/execution_context.h"
#include "resources/inventory_icon_io.h"
#include "resources/resource_output.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/conversion_failure.h"
#include "routes/model/invocation.h"
#include "routes/model/options/modules.h"
#include "routes/model/state.h"
#include "routes/native_text.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace cli::model {
namespace {

struct AnimationWriteEntry {
  NativeAnimationFile* file = nullptr;
  const OutputTarget* target = nullptr;
};

template <class Result>
bool outputFailure(const char* what, const Result& result) {
  return conversionOutputFailure(DiagnosticCode::kModelOutputFailed, what, result);
}

bool addAnimationOutputs(ResourceOutputPlan& plan, std::span<const AnimationWriteEntry> animations,
                         const ExecutionContext& execution, const Invocation& invocation, bool json) {
  for (const AnimationWriteEntry& output : animations) {
    const NativeAnimationFile& animation = *output.file;
    const std::string& identity = animation.resource_path.empty() ? output.target->path : animation.resource_path;
    const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kAnimation, identity);
    if (json) {
      auto text = pistoris::toTeaJson(animation.tea, invocation.format.pretty, animation.text_mode);
      if (!text) return outputFailure("TEA JSON output", text);
      plan.addPrimaryOwned(*output.target, std::vector<std::uint8_t>(text->begin(), text->end()), asset);
    } else {
      auto bytes = pistoris::writeTea(animation.tea);
      if (!bytes) return outputFailure("TEA output", bytes);
      plan.addPrimaryOwned(*output.target, std::move(*bytes), asset);
    }
    if (!addSoundFileOutputs(plan,
                             execution.io(),
                             invocation.sound_output,
                             animation.sound_files,
                             asset,
                             DiagnosticCode::kModelOutputFailed,
                             "Model Animation"))
      return false;
  }
  return true;
}

bool resolveResourceOutputs(ResourceOutputPlan& plan, const ExecutionContext& execution) {
  return execution.resourceOutputs().resolve(plan);
}

bool compactAnimationSounds(pistoris::Animation& animation) {
  const auto result = animation.compactSounds();
  return result || outputFailure("Animation sound compaction", result);
}

bool rebaseAnimationSounds(pistoris::Animation& animation, const Invocation& invocation) {
  const auto result = animation.rebaseSoundPaths(invocation.sound_rebase.directory);
  return result || outputFailure("Animation sound rebasing", result);
}

struct AutomaticSoundRebaseScan {
  bool eligible = false;
  bool preserved = false;
};

void inspectAutomaticSoundRebase(const pistoris::Animation& animation, const AnimationSource& source,
                                 const Invocation& invocation, AutomaticSoundRebaseScan& scan) {
  if (animation.soundCount() == 0) return;
  const SidecarEndpoint output = sidecarEndpoint(invocation.output, ARX_RESOURCE_KIND_MODEL);
  const SidecarRebaseDirection expected = automaticSidecarRebaseTarget(output);
  if (expected != SidecarRebaseDirection::kNone && automaticSidecarRebase(source.endpoint, output) == expected)
    scan.eligible = true;
  else
    scan.preserved = true;
}

bool shouldRebaseAnimationSounds(const Invocation& invocation, const AutomaticSoundRebaseScan& scan) {
  if (invocation.sound_rebase_mode == AnimationSoundRebaseMode::kExplicit) return true;
  if (invocation.sound_rebase_mode != AnimationSoundRebaseMode::kAutomatic) return false;
  if (scan.eligible && scan.preserved)
    log(ARX_LOG_INFO, "mixed Animation sound sources do not share an automatic rebase; preserving Sound paths");
  return scan.eligible && !scan.preserved;
}

bool prepareIntermediateSounds(IntermediateModel& source, const Invocation& invocation) {
  if (source.animations.size() != source.animation_sources.size()) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "Model Animation source metadata is inconsistent");
    return false;
  }
  for (pistoris::Animation& animation : source.animations)
    if (!compactAnimationSounds(animation)) return false;
  AutomaticSoundRebaseScan scan;
  if (invocation.sound_rebase_mode == AnimationSoundRebaseMode::kAutomatic)
    for (std::size_t index = 0; index < source.animations.size(); ++index)
      inspectAutomaticSoundRebase(source.animations[index], source.animation_sources[index], invocation, scan);
  if (shouldRebaseAnimationSounds(invocation, scan))
    for (pistoris::Animation& animation : source.animations)
      if (!rebaseAnimationSounds(animation, invocation)) return false;
  return true;
}

bool prepareSelectedIntermediateSounds(IntermediateModel& source, const Invocation& invocation) {
  if (source.animations.size() != source.animation_sources.size()) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "Model Animation source metadata is inconsistent");
    return false;
  }
  for (const AnimationSidecarOutput& output : invocation.animation_outputs) {
    if (output.source >= source.animations.size() || output.source >= source.animation_sources.size()) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Animation output has an invalid source index");
      return false;
    }
    pistoris::Animation& animation = source.animations[output.source];
    if (!compactAnimationSounds(animation)) return false;
  }
  AutomaticSoundRebaseScan scan;
  if (invocation.sound_rebase_mode == AnimationSoundRebaseMode::kAutomatic)
    for (const AnimationSidecarOutput& output : invocation.animation_outputs)
      inspectAutomaticSoundRebase(
          source.animations[output.source], source.animation_sources[output.source], invocation, scan);
  if (shouldRebaseAnimationSounds(invocation, scan))
    for (const AnimationSidecarOutput& output : invocation.animation_outputs)
      if (!rebaseAnimationSounds(source.animations[output.source], invocation)) return false;
  return true;
}

bool prepareIntermediateTextures(IntermediateModel& source, const Invocation& invocation) {
  const auto compacted = source.model.compactTextures();
  if (!compacted) return outputFailure("Model texture compaction", compacted);
  if (*compacted != 0) log(ARX_LOG_INFO, "removed %zu unused Model texture(s)", *compacted);
  if (invocation.texture_rebase.enabled) {
    const auto rebased = source.model.rebaseTexturePaths(invocation.texture_rebase.directory);
    if (!rebased) return outputFailure("Model texture rebasing", rebased);
  }
  return true;
}

bool bakeIntermediate(IntermediateModel& source, bool include_texture_files, bool include_sound_files,
                      const Invocation& invocation, NativeModelFiles& out,
                      std::vector<pistoris::NativeTextureFile>& texture_files) {
  out.text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  const pistoris::NativeModelBakeOptions options{.include_texture_files = include_texture_files,
                                                 .text_mode = out.text_mode};
  auto bundle = source.model.bakeNativeBundle(options);
  if (!bundle) return outputFailure("Model native output", bundle);
  out.ftl = std::move(bundle->ftl);
  texture_files = std::move(bundle->texture_files);

  out.animations.reserve(invocation.animation_outputs.size());
  for (const AnimationSidecarOutput& output : invocation.animation_outputs) {
    if (output.source >= source.animations.size()) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Animation output has an invalid source index");
      return false;
    }
    const pistoris::Animation& animation = source.animations[output.source];
    auto native = animation.bakeNativeBundle({.include_sound_files = include_sound_files, .text_mode = out.text_mode});
    if (!native) return outputFailure("Animation native output", native);
    NativeAnimationFile file;
    file.tea = std::move(native->tea);
    file.resource_path = std::string(animation.resourcePath());
    file.sound_files = std::move(native->sound_files);
    file.text_mode = out.text_mode;
    out.animations.push_back(std::move(file));
  }
  return true;
}

bool selectNativeAnimationOutputs(NativeModelFiles& files, const Invocation& invocation,
                                  std::vector<AnimationWriteEntry>& out) {
  out.clear();
  out.reserve(invocation.animation_outputs.size());
  for (const AnimationSidecarOutput& output : invocation.animation_outputs) {
    if (output.source >= files.animations.size()) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Animation output has an invalid source index");
      return false;
    }
    out.push_back({&files.animations[output.source], &output.target});
  }
  return true;
}

bool pairBakedAnimationOutputs(NativeModelFiles& files, const Invocation& invocation,
                               std::vector<AnimationWriteEntry>& out) {
  if (files.animations.size() != invocation.animation_outputs.size()) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "Baked Animation count does not match output target count");
    return false;
  }
  out.clear();
  out.reserve(files.animations.size());
  for (std::size_t index = 0; index < files.animations.size(); ++index)
    out.push_back({&files.animations[index], &invocation.animation_outputs[index].target});
  return true;
}

bool writeFtlFiles(const pistoris::Ftl& ftl, std::span<const AnimationWriteEntry> animations,
                   std::span<const pistoris::NativeTextureFile> texture_files,
                   std::span<const std::uint8_t> inventory_icon, ArxImageFormat inventory_icon_format,
                   const ExecutionContext& execution, const Invocation& invocation) {
  auto bytes = pistoris::writeFtl(ftl, invocation.format.compress);
  if (!bytes) return outputFailure("FTL output", bytes);
  ResourceOutputPlan resource_outputs;
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, invocation.output.path);
  resource_outputs.addPrimaryOwned(invocation.output, std::move(*bytes), model_asset);
  if (!addNativeTextureFileOutputs(resource_outputs,
                                   execution.io(),
                                   invocation.texture_output,
                                   texture_files,
                                   model_asset,
                                   DiagnosticCode::kModelOutputFailed,
                                   "Model") ||
      !addInventoryIconOutput(
          resource_outputs, invocation.inventory_icon_output, inventory_icon, inventory_icon_format, model_asset) ||
      !addAnimationOutputs(resource_outputs, animations, execution, invocation, false) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  return execution.resourceOutputs().write(resource_outputs);
}

void prepareNativeFtlOutput(const NativeModelFiles& files, const ExecutionContext& execution,
                            const Invocation& invocation, std::vector<pistoris::NativeTextureFile>& texture_files) {
  if (!invocation.texture_options.export_files) return;
  texture_files = files.texture_files;
  if (invocation.texture_options.input_folder_specified)
    loadNativeTextureFiles(files.ftl, files.text_mode, execution.io(), invocation.textures, texture_files);
}

void prepareNativeSoundOutput(std::span<AnimationWriteEntry> animations, const ExecutionContext& execution,
                              const Invocation& invocation) {
  if (!invocation.sound_options.export_files) {
    for (const AnimationWriteEntry& output : animations) output.file->sound_files.clear();
    return;
  }
  for (const AnimationWriteEntry& output : animations) {
    NativeAnimationFile& animation = *output.file;
    if (invocation.sound_options.input_folder_specified)
      loadNativeSoundFiles(animation.tea,
                           animation.text_mode,
                           execution.io(),
                           invocation.sound_inputs[animation.input],
                           animation.sound_files);
  }
}

bool writeFtlNative(NativeModelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> texture_files;
  std::vector<AnimationWriteEntry> animations;
  if (!selectNativeAnimationOutputs(files, invocation, animations)) return false;
  prepareNativeFtlOutput(files, execution, invocation, texture_files);
  prepareNativeSoundOutput(animations, execution, invocation);
  return writeFtlFiles(
      files.ftl, animations, texture_files, files.inventory_icon, files.inventory_icon_format, execution, invocation);
}

bool writeFtlIntermediate(IntermediateModel& source, const ExecutionContext& execution, const Invocation& invocation) {
  NativeModelFiles files;
  std::vector<pistoris::NativeTextureFile> texture_files;
  const bool include_texture_files = invocation.texture_options.export_files;
  const bool include_sound_files = invocation.sound_options.export_files;
  if (!prepareIntermediateTextures(source, invocation) || !prepareSelectedIntermediateSounds(source, invocation) ||
      !bakeIntermediate(source, include_texture_files, include_sound_files, invocation, files, texture_files)) {
    return false;
  }
  std::vector<AnimationWriteEntry> animations;
  std::vector<std::uint8_t> rendered_icon;
  return projectInventoryIcon(
             source.model, invocation.options.inventory_icon_render, pistoris::ImageFormat::kBmp, rendered_icon) &&
         pairBakedAnimationOutputs(files, invocation, animations) &&
         writeFtlFiles(
             files.ftl, animations, texture_files, rendered_icon, ARX_IMAGE_FORMAT_BMP, execution, invocation);
}

bool writeJsonNative(NativeModelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  std::vector<pistoris::NativeTextureFile> texture_files;
  std::vector<AnimationWriteEntry> animations;
  if (!selectNativeAnimationOutputs(files, invocation, animations)) return false;
  prepareNativeFtlOutput(files, execution, invocation, texture_files);
  prepareNativeSoundOutput(animations, execution, invocation);
  auto text = pistoris::toFtlJson(files.ftl, invocation.format.pretty, files.text_mode);
  if (!text) return outputFailure("FTL JSON output", text);
  ResourceOutputPlan resource_outputs;
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, invocation.output.path);
  resource_outputs.addPrimaryOwned(
      invocation.output, std::vector<std::uint8_t>(text->begin(), text->end()), model_asset);
  if (!addNativeTextureFileOutputs(resource_outputs,
                                   execution.io(),
                                   invocation.texture_output,
                                   texture_files,
                                   model_asset,
                                   DiagnosticCode::kModelOutputFailed,
                                   "Model") ||
      !addInventoryIconOutput(resource_outputs,
                              invocation.inventory_icon_output,
                              files.inventory_icon,
                              files.inventory_icon_format,
                              model_asset) ||
      !addAnimationOutputs(resource_outputs, animations, execution, invocation, true) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  return execution.resourceOutputs().write(resource_outputs);
}

bool writeJsonIntermediate(IntermediateModel& source, const ExecutionContext& execution, const Invocation& invocation) {
  NativeModelFiles files;
  std::vector<pistoris::NativeTextureFile> texture_files;
  const bool include_texture_files = invocation.texture_options.export_files;
  const bool include_sound_files = invocation.sound_options.export_files;
  if (!prepareIntermediateTextures(source, invocation) || !prepareSelectedIntermediateSounds(source, invocation) ||
      !bakeIntermediate(source, include_texture_files, include_sound_files, invocation, files, texture_files)) {
    return false;
  }
  std::vector<AnimationWriteEntry> animations;
  if (!pairBakedAnimationOutputs(files, invocation, animations)) return false;
  auto text = pistoris::toFtlJson(files.ftl, invocation.format.pretty, files.text_mode);
  if (!text) return outputFailure("FTL JSON output", text);
  ResourceOutputPlan resource_outputs;
  std::string identity(source.model.resourcePath());
  if (identity.empty()) identity = invocation.output.path;
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(identity));
  resource_outputs.addPrimaryOwned(
      invocation.output, std::vector<std::uint8_t>(text->begin(), text->end()), model_asset);
  std::vector<std::uint8_t> rendered_icon;
  if (!addNativeTextureFileOutputs(resource_outputs,
                                   execution.io(),
                                   invocation.texture_output,
                                   texture_files,
                                   model_asset,
                                   DiagnosticCode::kModelOutputFailed,
                                   "Model") ||
      !addInventoryIconOutput(resource_outputs,
                              invocation.inventory_icon_output,
                              source.model,
                              invocation.options.inventory_icon_render,
                              rendered_icon,
                              model_asset) ||
      !addAnimationOutputs(resource_outputs, animations, execution, invocation, true) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  return execution.resourceOutputs().write(resource_outputs);
}

bool writeObjIntermediate(IntermediateModel& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (!source.animations.empty()) {
    log(ARX_LOG_WARN, "OBJ output ignores %zu Animation sidecar(s)", source.animations.size());
  }

  if (!prepareIntermediateTextures(source, invocation)) return false;
  const std::string object_stem = resourceStem(invocation.output.path);
  const pistoris::ObjExportOptions options{.include_files = invocation.texture_options.export_files};
  auto object = source.model.exportObj(object_stem, options);
  if (!object) return outputFailure("OBJ output", object);

  ResourceOutputPlan resource_outputs;
  std::string identity(source.model.resourcePath());
  if (identity.empty()) identity = invocation.output.path;
  const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(identity));
  resource_outputs.addPrimary(invocation.output, object->text.data(), object->text.size(), asset);
  if (!object->mtl.empty())
    resource_outputs.add(
        ResourceFileKind::kData, invocation.obj_mtl_output, object->mtl.data(), object->mtl.size(), asset);
  std::vector<std::uint8_t> rendered_icon;
  if (!addInventoryIconOutput(resource_outputs,
                              invocation.inventory_icon_output,
                              source.model,
                              invocation.options.inventory_icon_render,
                              rendered_icon,
                              asset) ||
      !addObjTextureFileOutputs(resource_outputs,
                                execution.io(),
                                invocation.texture_output,
                                object->texture_files,
                                asset,
                                DiagnosticCode::kModelOutputFailed,
                                "Model") ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;

  return execution.resourceOutputs().write(resource_outputs);
}

bool writeGlbIntermediate(IntermediateModel& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (!prepareIntermediateTextures(source, invocation) || !prepareIntermediateSounds(source, invocation)) return false;
  std::vector<const pistoris::Animation*> animations;
  animations.reserve(source.animations.size());
  for (const pistoris::Animation& animation : source.animations) animations.push_back(&animation);

  pistoris::ModelGlbBundle bundle;
  if (invocation.sound_options.export_files) {
    auto exported = source.model.exportGlbBundle(animations, invocation.options.glb_export);
    if (!exported) return outputFailure("GLB output", exported);
    bundle = std::move(*exported);
  } else {
    auto exported = source.model.exportGlb(animations, invocation.options.glb_export);
    if (!exported) return outputFailure("GLB output", exported);
    bundle.glb = std::move(*exported);
  }
  ResourceOutputPlan resource_outputs;
  std::string model_identity(source.model.resourcePath());
  if (model_identity.empty()) model_identity = invocation.output.path;
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(model_identity));
  resource_outputs.addPrimary(invocation.output, bundle.glb.data(), bundle.glb.size(), model_asset);
  std::vector<std::uint8_t> rendered_icon;
  if (!addInventoryIconOutput(resource_outputs,
                              invocation.inventory_icon_output,
                              source.model,
                              invocation.options.inventory_icon_render,
                              rendered_icon,
                              model_asset))
    return false;
  std::vector<ResourceAssetId> assets;
  assets.reserve(source.animations.size());
  for (std::size_t index = 0; index < source.animations.size(); ++index) {
    const pistoris::Animation& animation = source.animations[index];
    std::string identity(animation.resourcePath());
    if (identity.empty()) identity = std::string(animation.name());
    if (identity.empty()) identity = "<unnamed " + std::to_string(index + 1U) + ">";
    assets.push_back(resource_outputs.addAsset(ResourceAssetKind::kAnimation, std::move(identity)));
  }
  for (const pistoris::AnimationSoundFile& file : bundle.sound_files) {
    if (file.animation_index >= assets.size()) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "GLB sound sidecar has an invalid Animation index");
      return false;
    }
    if (!addSoundFileOutputs(resource_outputs,
                             execution.io(),
                             invocation.sound_output,
                             std::span<const pistoris::SoundFile>(&file.file, 1),
                             assets[file.animation_index],
                             DiagnosticCode::kModelOutputFailed,
                             "Model Animation"))
      return false;
  }
  if (!resolveResourceOutputs(resource_outputs, execution)) return false;
  return execution.resourceOutputs().write(resource_outputs);
}

bool writeLevelPreviewGlbIntermediate(IntermediateModel& source, const ExecutionContext& execution,
                                      const Invocation& invocation) {
  if (!source.animations.empty()) {
    log(ARX_LOG_WARN, "Model Level preview ignores %zu Animation sidecar(s)", source.animations.size());
  }
  if (!prepareIntermediateTextures(source, invocation)) return false;

  pistoris::Model::LevelPreviewGlbOptions options = invocation.options.level_preview_glb;
  std::string inferred_class_path;
  if (!invocation.options.preview_class_path) {
    pistoris::paths::ModelPathView model_path;
    options.class_path = pistoris::paths::modelFromFtl(source.model.resourcePath(), model_path) &&
                                 pistoris::paths::baseEntityClassFromModel(model_path, inferred_class_path)
                             ? inferred_class_path
                             : std::string_view{};
  } else {
    options.class_path = *invocation.options.preview_class_path;
  }
  options.asset_name = invocation.preview_asset_name;
  auto bytes = source.model.exportLevelPreviewGlb(options);
  if (!bytes) return outputFailure("Level preview GLB output", bytes);
  ResourceOutputPlan resource_outputs;
  std::string identity(source.model.resourcePath());
  if (identity.empty()) identity = invocation.output.path;
  const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(identity));
  resource_outputs.addPrimaryOwned(invocation.output, std::move(*bytes), asset);
  std::vector<std::uint8_t> rendered_icon;
  if (!addInventoryIconOutput(resource_outputs,
                              invocation.inventory_icon_output,
                              source.model,
                              invocation.options.inventory_icon_render,
                              rendered_icon,
                              asset) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  return execution.resourceOutputs().write(resource_outputs);
}

}  // namespace

const OutputConverterDescriptor* outputConverterDescriptor(Format output, const Module* module) {
  static constexpr OutputConverterDescriptor kFtl{writeFtlNative, writeFtlIntermediate, true};
  static constexpr OutputConverterDescriptor kJson{writeJsonNative, writeJsonIntermediate, true};
  static constexpr OutputConverterDescriptor kObj{nullptr, writeObjIntermediate, false};
  static constexpr OutputConverterDescriptor kGlb{nullptr, writeGlbIntermediate, true};
  static constexpr OutputConverterDescriptor kLevelPreviewGlb{nullptr, writeLevelPreviewGlbIntermediate, false};
  if (module) {
    if (module == &options::asLevelPreviewModule()) return output == Format::kGlb ? &kLevelPreviewGlb : nullptr;
    return nullptr;
  }
  switch (output) {
    case Format::kFtl:
      return &kFtl;
    case Format::kJson:
      return &kJson;
    case Format::kObj:
      return &kObj;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool writeNativeOutput(NativeModelFiles& files, const ExecutionContext& execution, const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_native) {
    diagnostic(DiagnosticCode::kModelUnsupportedOutput, "Model output converter requires intermediate data");
    return false;
  }
  return invocation.output_converter->write_native(files, execution, invocation);
}

bool writeIntermediateOutput(IntermediateModel& model, const ExecutionContext& execution,
                             const Invocation& invocation) {
  if (!invocation.output_converter || !invocation.output_converter->write_intermediate) {
    diagnostic(DiagnosticCode::kModelUnsupportedOutput, "Model output converter is unavailable");
    return false;
  }
  return invocation.output_converter->write_intermediate(model, execution, invocation);
}

}  // namespace cli::model
