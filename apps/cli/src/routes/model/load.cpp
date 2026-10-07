// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/load.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/native_bundle.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "resources/model_input.h"
#include "resources/native_bundle.h"
#include "resources/sidecar_io.h"
#include "routes/conversion_failure.h"
#include "routes/model/invocation.h"
#include "routes/model/options.h"
#include "routes/model/state.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::model {
namespace {

template <class Result>
bool inputFailure(const char* what, const Result& result, std::string_view path = {}) {
  return conversionInputFailure(DiagnosticCode::kModelInputFailed, what, path, result);
}

bool decodeTea(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Tea& out) {
  switch (input.facts.format) {
    case Format::kTea: {
      auto result = pistoris::readTea(input.document.data);
      if (!result) return inputFailure("TEA", result, input.path);
      out = std::move(*result);
      return true;
    }
    case Format::kJson: {
      auto result = pistoris::fromTeaJson(byteStringView(input.document.data), text_mode);
      if (!result) return inputFailure("TEA", result, input.path);
      out = std::move(*result);
      return true;
    }
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedExtra, "Unsupported extra animation format: %s", input.path.c_str());
      return false;
  }
}

bool applyModelResourcePath(const ClassifiedPath& input, pistoris::Model& out) {
  pistoris::paths::ModelPathView parsed;
  if (!pistoris::paths::modelFromFtl(input.path, parsed)) return true;
  const auto result = out.setResourcePath(input.path);
  return result || inputFailure("Model resource identity", result, input.path);
}

bool applyAnimationResourcePath(const ClassifiedPath& input, pistoris::Animation& out) {
  pistoris::paths::AnimationPathView parsed;
  if (!pistoris::paths::animationFromTea(input.path, parsed)) return true;
  const auto result = out.setResourcePath(input.path);
  return result || inputFailure("Animation resource identity", result, input.path);
}

bool loadNative(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                NativeModelFiles& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  std::vector<pistoris::resource_io::ResourceDocument> animations;
  animations.reserve(invocation.extras.size());
  for (const std::size_t index : invocation.extras) animations.push_back(std::move(inputs[index].document));
  const pistoris::NativeTextMode text_mode =
      directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  auto loaded = io.resources().loadModelNativeBundle(
      input.document, animations, {.native_text_mode = text_mode, .suppress_related_resource_errors = false});
  if (!loaded) return inputFailure("Model native bundle", loaded, input.path);

  out.ftl = loaded->model().carrier();
  out.text_mode = loaded->model().textMode();
  if (!nativeTextureFiles(loaded->resources(),
                          pistoris::resource_io::NativeResourceRole::kTexture,
                          ARX_RESOURCE_KIND_MODEL,
                          0,
                          io,
                          "Model texture image",
                          out.texture_files)) {
    diagnostic(DiagnosticCode::kModelInputFailed, "Model native bundle contains an invalid texture reference");
    return false;
  }
  out.animations.reserve(loaded->animations().size());
  for (std::size_t animation_index = 0; animation_index < loaded->animations().size(); ++animation_index) {
    const auto& member = loaded->animations()[animation_index];
    NativeAnimationFile animation;
    animation.tea = member.carrier();
    animation.resource_path = member.source().logical_path;
    animation.input = invocation.extras[animation_index];
    animation.text_mode = member.textMode();
    if (!nativeSoundFiles(loaded->resources(),
                          ARX_RESOURCE_KIND_ANIMATION,
                          animation_index,
                          io,
                          "Animation sound",
                          animation.sound_files)) {
      diagnostic(DiagnosticCode::kModelInputFailed, "Model native bundle contains an invalid sound reference");
      return false;
    }
    out.animations.push_back(std::move(animation));
  }
  return true;
}

bool nativeToIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                          NativeModelFiles& native, IntermediateModel& out) {
  std::vector<std::string> texture_source_paths;
  auto model = pistoris::Model::importNative(native.ftl, &texture_source_paths, native.text_mode);
  if (!model) return inputFailure("FTL Model", model, inputs[invocation.input].path);
  if (!applyModelResourcePath(inputs[invocation.input], *model)) return false;
  out.model = std::move(*model);
  out.texture_source_paths = std::move(texture_source_paths);
  for (const pistoris::NativeTextureFile& texture : native.texture_files) {
    const auto loaded =
        out.model.setTextureImage(texture.source_texture, {texture.encoded_image.data(), texture.encoded_image.size()});
    if (!loaded) return inputFailure("Model texture", loaded, texture.resource_path);
  }

  out.animations.reserve(native.animations.size());
  out.sound_sources.reserve(native.animations.size());
  out.animation_sources.reserve(native.animations.size());
  for (std::size_t index = 0; index < native.animations.size(); ++index) {
    if (native.animations[index].input >= inputs.size()) {
      diagnostic(DiagnosticCode::kModelInputFailed, "Native Animation has an invalid source index");
      return false;
    }
    std::vector<pistoris::SoundSourceReference> sound_sources;
    auto animation = pistoris::Animation::importNative(
        native.animations[index].tea, &sound_sources, native.animations[index].text_mode);
    if (!animation) return inputFailure("TEA Animation", animation, inputs[native.animations[index].input].path);
    if (!native.animations[index].resource_path.empty()) {
      const auto identity = animation->setResourcePath(native.animations[index].resource_path);
      if (!identity)
        return inputFailure("Animation resource identity", identity, native.animations[index].resource_path);
    }
    for (const pistoris::SoundFile& sound : native.animations[index].sound_files) {
      const auto loaded =
          animation->setSoundData(sound.source_sound, {sound.encoded_audio.data(), sound.encoded_audio.size()});
      if (!loaded) return inputFailure("Animation sound", loaded, sound.path);
    }
    out.animations.push_back(std::move(*animation));
    out.sound_sources.push_back(std::move(sound_sources));
    const std::size_t input = native.animations[index].input;
    out.animation_sources.push_back(
        {.input = input, .endpoint = sidecarEndpoint(inputs[input], ARX_RESOURCE_KIND_ANIMATION)});
  }
  return true;
}

bool loadNativeIntermediate(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, const ModelOptions&,
                            IoService& io, IntermediateModel& out) {
  NativeModelFiles native;
  return loadNative(inputs, invocation, io, native) && nativeToIntermediate(inputs, invocation, native, out);
}

bool loadObjIntermediate(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, const ModelOptions&,
                         IoService& io, IntermediateModel& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  ConvertedModelInput converted;
  if (!convertModelInput(input,
                         io,
                         {.glb = {}, .native_text_mode = invocation.native_text_mode},
                         DiagnosticCode::kModelInputFailed,
                         "OBJ Model",
                         converted))
    return false;
  out.model.swap(converted.model);
  out.texture_source_paths = std::move(converted.texture_source_paths);

  out.animations.reserve(invocation.extras.size());
  out.sound_sources.reserve(invocation.extras.size());
  out.animation_sources.reserve(invocation.extras.size());
  for (const std::size_t index : invocation.extras) {
    const pistoris::NativeTextMode text_mode = carrierTextMode(inputs[index].facts.format, invocation.native_text_mode);
    pistoris::Tea native_animation;
    if (!decodeTea(inputs[index], text_mode, native_animation)) return false;
    std::vector<pistoris::SoundSourceReference> sound_sources;
    auto animation = pistoris::Animation::importNative(native_animation, &sound_sources, text_mode);
    if (!animation) return inputFailure("TEA Animation", animation, inputs[index].path);
    if (!applyAnimationResourcePath(inputs[index], *animation)) return false;
    out.animations.push_back(std::move(*animation));
    out.sound_sources.push_back(std::move(sound_sources));
    out.animation_sources.push_back(
        {.input = index, .endpoint = sidecarEndpoint(inputs[index], ARX_RESOURCE_KIND_ANIMATION)});
  }
  return true;
}

bool loadGlbIntermediate(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, const ModelOptions& options,
                         IoService& io, IntermediateModel& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  ConvertedModelInput converted;
  if (!convertModelInput(input,
                         io,
                         {.glb = options.glb_import, .native_text_mode = invocation.native_text_mode},
                         DiagnosticCode::kModelInputFailed,
                         "GLB Model",
                         converted))
    return false;
  out.model.swap(converted.model);
  out.texture_source_paths = std::move(converted.texture_source_paths);
  out.sound_sources.resize(converted.animations.size());
  out.animation_sources.resize(
      converted.animations.size(),
      {.input = invocation.input, .endpoint = sidecarEndpoint(input, ARX_RESOURCE_KIND_ANIMATION)});
  for (pistoris::AnimationSoundSourceReference& source : converted.sound_sources) {
    if (source.animation_index < out.sound_sources.size())
      out.sound_sources[source.animation_index].push_back(std::move(source.reference));
  }
  out.animations = std::move(converted.animations);

  out.animations.reserve(out.animations.size() + invocation.extras.size());
  out.sound_sources.reserve(out.animations.capacity());
  out.animation_sources.reserve(out.animations.capacity());
  for (const std::size_t index : invocation.extras) {
    const pistoris::NativeTextMode text_mode = carrierTextMode(inputs[index].facts.format, invocation.native_text_mode);
    pistoris::Tea native_animation;
    if (!decodeTea(inputs[index], text_mode, native_animation)) return false;
    std::vector<pistoris::SoundSourceReference> sound_sources;
    auto animation = pistoris::Animation::importNative(native_animation, &sound_sources, text_mode);
    if (!animation) return inputFailure("TEA Animation", animation, inputs[index].path);
    if (!applyAnimationResourcePath(inputs[index], *animation)) return false;
    out.animations.push_back(std::move(*animation));
    out.sound_sources.push_back(std::move(sound_sources));
    out.animation_sources.push_back(
        {.input = index, .endpoint = sidecarEndpoint(inputs[index], ARX_RESOURCE_KIND_ANIMATION)});
  }
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kNative{loadNative, loadNativeIntermediate};
  static constexpr InputConverterDescriptor kObj{nullptr, loadObjIntermediate};
  static constexpr InputConverterDescriptor kGlb{nullptr, loadGlbIntermediate};
  switch (route.input) {
    case Format::kFtl:
    case Format::kJson:
      return &kNative;
    case Format::kObj:
      return &kObj;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const ModelOptions& options, IoService& io, bool native, ModelInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeModelFiles& loaded = out.emplace<NativeModelFiles>();
    if (converter.load_native(inputs, invocation, io, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateModel& loaded = out.emplace<IntermediateModel>();
  if (converter.load_intermediate(inputs, invocation, options, io, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

bool loadReferenceModel(std::span<const std::uint8_t> data, std::string_view path, pistoris::NativeTextMode text_mode,
                        IntermediateModel& out) {
  auto native = pistoris::readFtl(data);
  if (!native) return inputFailure("Reference FTL", native, path);

  auto imported = pistoris::Model::importNative(*native, nullptr, text_mode);
  if (!imported) return inputFailure("Reference Model", imported, path);
  auto reference = std::make_unique<pistoris::Model>(std::move(*imported));
  log(ARX_LOG_INFO, "using reference FTL: %.*s", static_cast<int>(path.size()), path.data());
  out.reference = std::move(reference);
  return true;
}

}  // namespace cli::model
