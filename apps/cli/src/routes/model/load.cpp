// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/load.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "resources/model_input.h"
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
      auto result = pistoris::readTea(input.buffer);
      if (!result) return inputFailure("TEA", result, input.path);
      out = std::move(*result);
      return true;
    }
    case Format::kJson: {
      auto result = pistoris::fromTeaJson(byteStringView(input.buffer), text_mode);
      if (!result) return inputFailure("TEA", result, input.path);
      out = std::move(*result);
      return true;
    }
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedExtra, "Unsupported extra animation format: %s", input.path.c_str());
      return false;
  }
}

bool decodeFtl(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Ftl& out) {
  switch (input.facts.format) {
    case Format::kFtl: {
      auto result = pistoris::readFtl(input.buffer);
      if (!result) return inputFailure(formatName(input.facts.format), result, input.path);
      out = std::move(*result);
      return true;
    }
    case Format::kJson: {
      auto result = pistoris::fromFtlJson(byteStringView(input.buffer), text_mode);
      if (!result) return inputFailure(formatName(input.facts.format), result, input.path);
      out = std::move(*result);
      return true;
    }
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedInput, "Unsupported Model input format: %s", input.path.c_str());
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

bool loadNative(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, NativeModelFiles& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  out.text_mode = directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  if (!decodeFtl(input, out.text_mode, out.ftl)) return false;
  out.animations.reserve(invocation.extras.size());
  for (const std::size_t index : invocation.extras) {
    NativeAnimationFile animation;
    animation.text_mode =
        directCarrierTextMode(inputs[index].facts.format, invocation.output.format, invocation.native_text_mode);
    if (!decodeTea(inputs[index], animation.text_mode, animation.tea)) return false;
    animation.input = index;
    pistoris::paths::AnimationPathView parsed;
    if (pistoris::paths::animationFromTea(inputs[index].path, parsed)) animation.resource_path = inputs[index].path;
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
    out.animations.push_back(std::move(*animation));
    out.sound_sources.push_back(std::move(sound_sources));
    const std::size_t input = native.animations[index].input;
    out.animation_sources.push_back(
        {.input = input, .endpoint = sidecarEndpoint(inputs[input], ARX_RESOURCE_KIND_ANIMATION)});
  }
  return true;
}

bool loadNativeIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                            const ModelOptions&, IntermediateModel& out) {
  NativeModelFiles native;
  return loadNative(inputs, invocation, native) && nativeToIntermediate(inputs, invocation, native, out);
}

bool loadObjIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, const ModelOptions&,
                         IntermediateModel& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  ConvertedModelInput converted;
  if (!convertModelInput(input,
                         invocation.obj_material_libraries,
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

bool loadGlbIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                         const ModelOptions& options, IntermediateModel& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  ConvertedModelInput converted;
  if (!convertModelInput(input,
                         {},
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

bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const ModelOptions& options, bool native, ModelInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeModelFiles& loaded = out.emplace<NativeModelFiles>();
    if (converter.load_native(inputs, invocation, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateModel& loaded = out.emplace<IntermediateModel>();
  if (converter.load_intermediate(inputs, invocation, options, loaded)) return true;
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
