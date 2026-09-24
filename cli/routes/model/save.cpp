// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/save.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/glb.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime.hpp"
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
#include "resources/output.h"
#include "resources/resource_output.h"
#include "resources/selector.h"
#include "resources/sidecar_io.h"
#include "resources/sound_io.h"
#include "resources/texture_io.h"
#include "routes/model/invocation.h"
#include "routes/model/options/modules.h"
#include "routes/model/state.h"
#include "routes/native_text.h"

#include <cstddef>
#include <cstdint>
#include <memory>
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

bool outputFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kModelOutputFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool writeAnimationFiles(std::span<const AnimationWriteEntry> animations, const ExecutionContext& execution,
                         const Invocation& invocation, bool json) {
  bool success = true;
  for (const AnimationWriteEntry& output : animations) {
    const NativeAnimationFile& animation = *output.file;
    if (json) {
      std::string text;
      const ArxReturnCode rc = pistoris::toJson(animation.tea, text, invocation.format.pretty, animation.text_mode);
      if (rc != ARX_OK) {
        outputFailure("TEA JSON output", rc);
        success = false;
        continue;
      }
      if (!writeOutput(execution.io(), *output.target, text.data(), text.size())) success = false;
      continue;
    }

    std::vector<std::uint8_t> bytes;
    const ArxReturnCode rc = pistoris::writeTea(animation.tea, bytes);
    if (rc != ARX_OK) {
      outputFailure("TEA output", rc);
      success = false;
      continue;
    }
    if (!writeOutput(execution.io(), *output.target, bytes.data(), bytes.size())) success = false;
  }
  return success;
}

bool addAnimationSoundOutputs(ResourceOutputPlan& plan, std::span<const AnimationWriteEntry> animations,
                              const ExecutionContext& execution, const Invocation& invocation) {
  for (const AnimationWriteEntry& output : animations) {
    const NativeAnimationFile& animation = *output.file;
    const std::string& identity = animation.resource_path.empty() ? output.target->path : animation.resource_path;
    const ResourceAssetId asset = plan.addAsset(ResourceAssetKind::kAnimation, identity);
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

void reserveNativeModelOutputs(ResourceOutputPlan& plan, const Invocation& invocation,
                               std::span<const AnimationWriteEntry> animations) {
  plan.reserveOutput(invocation.output);
  for (const AnimationWriteEntry& output : animations) plan.reserveOutput(*output.target);
}

bool compactAnimationSounds(pistoris::Animation& animation) {
  std::size_t removed = 0;
  const ArxReturnCode rc = animation.compactSounds(&removed);
  if (rc != ARX_OK) return outputFailure("Animation sound compaction", rc);
  return true;
}

bool rebaseAnimationSounds(pistoris::Animation& animation, const Invocation& invocation) {
  const ArxReturnCode rc = animation.rebaseSoundPaths(invocation.sound_rebase.directory);
  return rc == ARX_OK || outputFailure("Animation sound rebasing", rc);
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
  for (const std::unique_ptr<pistoris::Animation>& animation : source.animations) {
    if (!animation) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Model contains a null Animation sidecar");
      return false;
    }
    if (!compactAnimationSounds(*animation)) return false;
  }
  AutomaticSoundRebaseScan scan;
  if (invocation.sound_rebase_mode == AnimationSoundRebaseMode::kAutomatic)
    for (std::size_t index = 0; index < source.animations.size(); ++index)
      inspectAutomaticSoundRebase(*source.animations[index], source.animation_sources[index], invocation, scan);
  if (shouldRebaseAnimationSounds(invocation, scan))
    for (const std::unique_ptr<pistoris::Animation>& animation : source.animations)
      if (!rebaseAnimationSounds(*animation, invocation)) return false;
  return true;
}

bool prepareSelectedIntermediateSounds(IntermediateModel& source, const Invocation& invocation) {
  if (source.animations.size() != source.animation_sources.size()) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "Model Animation source metadata is inconsistent");
    return false;
  }
  for (const AnimationSidecarOutput& output : invocation.animation_outputs) {
    if (output.source >= source.animations.size() || output.source >= source.animation_sources.size() ||
        !source.animations[output.source]) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Animation output has an invalid source index");
      return false;
    }
    pistoris::Animation& animation = *source.animations[output.source];
    if (!compactAnimationSounds(animation)) return false;
  }
  AutomaticSoundRebaseScan scan;
  if (invocation.sound_rebase_mode == AnimationSoundRebaseMode::kAutomatic)
    for (const AnimationSidecarOutput& output : invocation.animation_outputs)
      inspectAutomaticSoundRebase(
          *source.animations[output.source], source.animation_sources[output.source], invocation, scan);
  if (shouldRebaseAnimationSounds(invocation, scan))
    for (const AnimationSidecarOutput& output : invocation.animation_outputs)
      if (!rebaseAnimationSounds(*source.animations[output.source], invocation)) return false;
  return true;
}

bool prepareIntermediateTextures(IntermediateModel& source, const Invocation& invocation) {
  std::size_t removed = 0;
  ArxReturnCode rc = source.model.compactTextures(&removed);
  if (rc != ARX_OK) return outputFailure("Model texture compaction", rc);
  if (removed != 0) log(ARX_LOG_INFO, "removed %zu unused Model texture(s)", removed);
  if (invocation.texture_rebase.enabled) {
    rc = source.model.rebaseTexturePaths(invocation.texture_rebase.directory);
    if (rc != ARX_OK) return outputFailure("Model texture rebasing", rc);
  }
  return true;
}

bool bakeIntermediate(IntermediateModel& source, bool include_texture_files, bool include_sound_files,
                      const Invocation& invocation, NativeModelFiles& out,
                      std::vector<pistoris::NativeTextureFile>& texture_files) {
  out.text_mode = carrierTextMode(invocation.output.format, invocation.native_text_mode);
  pistoris::NativeModelBundle bundle;
  const pistoris::NativeModelBakeOptions options{.include_texture_files = include_texture_files,
                                                 .text_mode = out.text_mode};
  ArxReturnCode rc = source.model.bakeNativeBundle(options, bundle);
  if (rc != ARX_OK) return outputFailure("Model native output", rc);
  out.ftl = std::move(bundle.ftl);
  texture_files = std::move(bundle.texture_files);

  out.animations.reserve(invocation.animation_outputs.size());
  for (const AnimationSidecarOutput& output : invocation.animation_outputs) {
    if (output.source >= source.animations.size() || !source.animations[output.source]) {
      diagnostic(DiagnosticCode::kModelOutputFailed, "Animation output has an invalid source index");
      return false;
    }
    const pistoris::Animation& animation = *source.animations[output.source];
    pistoris::NativeAnimationBundle native;
    rc = animation.bakeNativeBundle({.include_sound_files = include_sound_files, .text_mode = out.text_mode}, native);
    if (rc != ARX_OK) return outputFailure("Animation native output", rc);
    NativeAnimationFile file;
    file.tea = std::move(native.tea);
    file.resource_path = std::string(animation.resourcePath());
    file.sound_files = std::move(native.sound_files);
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
  std::vector<std::uint8_t> bytes;
  const ArxReturnCode rc = pistoris::writeFtl(ftl, bytes, invocation.format.compress);
  if (rc != ARX_OK) return outputFailure("FTL output", rc);
  ResourceOutputPlan resource_outputs;
  reserveNativeModelOutputs(resource_outputs, invocation, animations);
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, invocation.output.path);
  if (!addNativeTextureFileOutputs(resource_outputs,
                                   execution.io(),
                                   invocation.texture_output,
                                   texture_files,
                                   model_asset,
                                   DiagnosticCode::kModelOutputFailed,
                                   "Model") ||
      !addInventoryIconOutput(
          resource_outputs, invocation.inventory_icon_output, inventory_icon, inventory_icon_format, model_asset) ||
      !addAnimationSoundOutputs(resource_outputs, animations, execution, invocation) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  if (!writeOutput(execution.io(), invocation.output, bytes.data(), bytes.size())) return false;
  return writeAnimationFiles(animations, execution, invocation, false) &&
         execution.resourceOutputs().write(resource_outputs);
}

void prepareNativeFtlOutput(const NativeModelFiles& files, const ExecutionContext& execution,
                            const Invocation& invocation, std::vector<pistoris::NativeTextureFile>& texture_files) {
  if (invocation.texture_options.export_files)
    loadNativeTextureFiles(files.ftl, files.text_mode, execution.io(), invocation.textures, texture_files);
}

void prepareNativeSoundOutput(std::span<AnimationWriteEntry> animations, const ExecutionContext& execution,
                              const Invocation& invocation) {
  if (!invocation.sound_options.export_files) return;
  for (const AnimationWriteEntry& output : animations) {
    NativeAnimationFile& animation = *output.file;
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
  return projectInventoryIconBmp(source.model, invocation.options.inventory_icon_render, rendered_icon) &&
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
  std::string text;
  const ArxReturnCode rc = pistoris::toJson(files.ftl, text, invocation.format.pretty, files.text_mode);
  if (rc != ARX_OK) return outputFailure("FTL JSON output", rc);
  ResourceOutputPlan resource_outputs;
  reserveNativeModelOutputs(resource_outputs, invocation, animations);
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, invocation.output.path);
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
      !addAnimationSoundOutputs(resource_outputs, animations, execution, invocation) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  if (!writeOutput(execution.io(), invocation.output, text.data(), text.size())) return false;
  return writeAnimationFiles(animations, execution, invocation, true) &&
         execution.resourceOutputs().write(resource_outputs);
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
  std::string text;
  const ArxReturnCode rc = pistoris::toJson(files.ftl, text, invocation.format.pretty, files.text_mode);
  if (rc != ARX_OK) return outputFailure("FTL JSON output", rc);
  ResourceOutputPlan resource_outputs;
  reserveNativeModelOutputs(resource_outputs, invocation, animations);
  std::string identity(source.model.resourcePath());
  if (identity.empty()) identity = invocation.output.path;
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(identity));
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
      !addAnimationSoundOutputs(resource_outputs, animations, execution, invocation) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  if (!writeOutput(execution.io(), invocation.output, text.data(), text.size())) return false;
  return writeAnimationFiles(animations, execution, invocation, true) &&
         execution.resourceOutputs().write(resource_outputs);
}

bool writeObjIntermediate(IntermediateModel& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (!source.animations.empty()) {
    log(ARX_LOG_WARN, "OBJ output ignores %zu Animation sidecar(s)", source.animations.size());
  }

  if (!prepareIntermediateTextures(source, invocation)) return false;
  pistoris::ObjBundle object;
  const std::string object_stem = resourceStem(invocation.output.path);
  const pistoris::ObjExportOptions options{.include_files = invocation.texture_options.export_files};
  const ArxReturnCode rc = source.model.exportObj(object_stem, options, object);
  if (rc != ARX_OK) return outputFailure("OBJ output", rc);

  ResourceOutputPlan resource_outputs;
  resource_outputs.reserveOutput(invocation.output);
  if (!object.mtl.empty()) resource_outputs.reserveOutput(invocation.obj_mtl_output);
  std::string identity(source.model.resourcePath());
  if (identity.empty()) identity = invocation.output.path;
  const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(identity));
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
                                object.texture_files,
                                asset,
                                DiagnosticCode::kModelOutputFailed,
                                "Model") ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;

  if (!writeOutput(execution.io(), invocation.output, object.text.data(), object.text.size())) return false;
  if (!object.mtl.empty() &&
      !writeOutput(execution.io(), invocation.obj_mtl_output, object.mtl.data(), object.mtl.size()))
    return false;
  return execution.resourceOutputs().write(resource_outputs);
}

bool writeGlbIntermediate(IntermediateModel& source, const ExecutionContext& execution, const Invocation& invocation) {
  if (!prepareIntermediateTextures(source, invocation) || !prepareIntermediateSounds(source, invocation)) return false;
  std::vector<const pistoris::Animation*> animations;
  animations.reserve(source.animations.size());
  for (const std::unique_ptr<pistoris::Animation>& animation : source.animations) animations.push_back(animation.get());

  pistoris::ModelGlbBundle bundle;
  ArxReturnCode rc = ARX_OK;
  if (invocation.sound_options.export_files) {
    rc = source.model.exportGlbBundle(animations, invocation.options.glb_export, nullptr, bundle);
  } else {
    rc = source.model.exportGlb(bundle.glb, animations, invocation.options.glb_export, nullptr);
  }
  if (rc != ARX_OK) return outputFailure("GLB output", rc);
  ResourceOutputPlan resource_outputs;
  resource_outputs.reserveOutput(invocation.output);
  std::string model_identity(source.model.resourcePath());
  if (model_identity.empty()) model_identity = invocation.output.path;
  const ResourceAssetId model_asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(model_identity));
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
    const pistoris::Animation& animation = *source.animations[index];
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
  return writeOutput(execution.io(), invocation.output, bundle.glb.data(), bundle.glb.size()) &&
         execution.resourceOutputs().write(resource_outputs);
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
  std::vector<std::uint8_t> bytes;
  const ArxReturnCode rc = source.model.exportLevelPreviewGlb(bytes, options);
  if (rc != ARX_OK) return outputFailure("Level preview GLB output", rc);
  ResourceOutputPlan resource_outputs;
  resource_outputs.reserveOutput(invocation.output);
  std::string identity(source.model.resourcePath());
  if (identity.empty()) identity = invocation.output.path;
  const ResourceAssetId asset = resource_outputs.addAsset(ResourceAssetKind::kModel, std::move(identity));
  std::vector<std::uint8_t> rendered_icon;
  if (!addInventoryIconOutput(resource_outputs,
                              invocation.inventory_icon_output,
                              source.model,
                              invocation.options.inventory_icon_render,
                              rendered_icon,
                              asset) ||
      !resolveResourceOutputs(resource_outputs, execution))
    return false;
  return writeOutput(execution.io(), invocation.output, bytes.data(), bytes.size()) &&
         execution.resourceOutputs().write(resource_outputs);
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
