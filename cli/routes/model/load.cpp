// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/model/load.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "resources/model_input.h"
#include "resources/sidecar_io.h"
#include "routes/model/invocation.h"
#include "routes/model/options.h"
#include "routes/model/state.h"
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

bool inputFailure(const char* what, ArxReturnCode rc, std::string_view path = {}) {
  if (path.empty()) {
    diagnostic(DiagnosticCode::kModelInputFailed,
               "%s input failed: %s (code %d)",
               what,
               pistoris::errorString(rc),
               static_cast<int>(rc));
  } else {
    diagnostic(DiagnosticCode::kModelInputFailed,
               "%s input failed (%.*s): %s (code %d)",
               what,
               static_cast<int>(path.size()),
               path.data(),
               pistoris::errorString(rc),
               static_cast<int>(rc));
  }
  return false;
}

bool decodeTea(const ClassifiedPath& input, pistoris::Tea& out) {
  ArxReturnCode rc = ARX_OK;
  switch (input.facts.format) {
    case Format::kTea:
      rc = pistoris::readTea(input.buffer, out);
      break;
    case Format::kJson:
      rc = pistoris::fromJson(byteStringView(input.buffer), out);
      break;
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedExtra, "Unsupported extra animation format: %s", input.path.c_str());
      return false;
  }
  return rc == ARX_OK || inputFailure("TEA", rc, input.path);
}

bool decodeFtl(const ClassifiedPath& input, pistoris::Ftl& out) {
  ArxReturnCode rc = ARX_OK;
  switch (input.facts.format) {
    case Format::kFtl:
      rc = pistoris::readFtl(input.buffer, out);
      break;
    case Format::kJson:
      rc = pistoris::fromJson(byteStringView(input.buffer), out);
      break;
    default:
      diagnostic(DiagnosticCode::kModelUnsupportedInput, "Unsupported Model input format: %s", input.path.c_str());
      return false;
  }
  return rc == ARX_OK || inputFailure(formatName(input.facts.format), rc, input.path);
}

bool applyModelResourcePath(const ClassifiedPath& input, pistoris::Model& out) {
  pistoris::paths::ModelPathView parsed;
  if (!pistoris::paths::modelFromFtl(input.path, parsed)) return true;
  const ArxReturnCode rc = out.setResourcePath(input.path);
  return rc == ARX_OK || inputFailure("Model resource identity", rc, input.path);
}

bool applyAnimationResourcePath(const ClassifiedPath& input, pistoris::Animation& out) {
  pistoris::paths::AnimationPathView parsed;
  if (!pistoris::paths::animationFromTea(input.path, parsed)) return true;
  const ArxReturnCode rc = out.setResourcePath(input.path);
  return rc == ARX_OK || inputFailure("Animation resource identity", rc, input.path);
}

bool loadNative(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, NativeModelFiles& out) {
  if (!decodeFtl(inputs[invocation.input], out.ftl)) return false;
  out.animations.reserve(invocation.extras.size());
  for (const std::size_t index : invocation.extras) {
    NativeAnimationFile animation;
    if (!decodeTea(inputs[index], animation.tea)) return false;
    animation.input = index;
    pistoris::paths::AnimationPathView parsed;
    if (pistoris::paths::animationFromTea(inputs[index].path, parsed)) animation.resource_path = inputs[index].path;
    out.animations.push_back(std::move(animation));
  }
  return true;
}

bool nativeToIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                          NativeModelFiles& native, IntermediateModel& out) {
  pistoris::Model model;
  std::vector<std::string> texture_source_paths;
  ArxReturnCode rc = pistoris::Model::importNative(model, native.ftl, &texture_source_paths);
  if (rc != ARX_OK) return inputFailure("FTL Model", rc, inputs[invocation.input].path);
  if (!applyModelResourcePath(inputs[invocation.input], model)) return false;
  out.model.swap(model);
  out.texture_source_paths = std::move(texture_source_paths);

  out.animations.reserve(native.animations.size());
  out.sound_sources.reserve(native.animations.size());
  out.animation_sources.reserve(native.animations.size());
  for (std::size_t index = 0; index < native.animations.size(); ++index) {
    if (native.animations[index].input >= inputs.size()) {
      diagnostic(DiagnosticCode::kModelInputFailed, "Native Animation has an invalid source index");
      return false;
    }
    auto animation = std::make_unique<pistoris::Animation>();
    std::vector<pistoris::SoundSourceReference> sound_sources;
    rc = pistoris::Animation::importNative(*animation, native.animations[index].tea, &sound_sources);
    if (rc != ARX_OK) return inputFailure("TEA Animation", rc, inputs[invocation.extras[index]].path);
    if (!native.animations[index].resource_path.empty()) {
      rc = animation->setResourcePath(native.animations[index].resource_path);
      if (rc != ARX_OK) return inputFailure("Animation resource identity", rc, native.animations[index].resource_path);
    }
    out.animations.push_back(std::move(animation));
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
  if (!convertModelInput(
          input, invocation.obj_material_libraries, {}, DiagnosticCode::kModelInputFailed, "OBJ Model", converted))
    return false;
  out.model.swap(converted.model);
  out.texture_source_paths = std::move(converted.texture_source_paths);

  out.animations.reserve(invocation.extras.size());
  out.sound_sources.reserve(invocation.extras.size());
  out.animation_sources.reserve(invocation.extras.size());
  for (const std::size_t index : invocation.extras) {
    pistoris::Tea native_animation;
    if (!decodeTea(inputs[index], native_animation)) return false;
    auto animation = std::make_unique<pistoris::Animation>();
    std::vector<pistoris::SoundSourceReference> sound_sources;
    const ArxReturnCode rc = pistoris::Animation::importNative(*animation, native_animation, &sound_sources);
    if (rc != ARX_OK) return inputFailure("TEA Animation", rc, inputs[index].path);
    if (!applyAnimationResourcePath(inputs[index], *animation)) return false;
    out.animations.push_back(std::move(animation));
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
  if (!convertModelInput(input, {}, options.glb_import, DiagnosticCode::kModelInputFailed, "GLB Model", converted))
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
    pistoris::Tea native_animation;
    if (!decodeTea(inputs[index], native_animation)) return false;
    auto animation = std::make_unique<pistoris::Animation>();
    std::vector<pistoris::SoundSourceReference> sound_sources;
    const ArxReturnCode rc = pistoris::Animation::importNative(*animation, native_animation, &sound_sources);
    if (rc != ARX_OK) return inputFailure("TEA Animation", rc, inputs[index].path);
    if (!applyAnimationResourcePath(inputs[index], *animation)) return false;
    out.animations.push_back(std::move(animation));
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

bool loadReferenceModel(std::span<const std::uint8_t> data, std::string_view path, IntermediateModel& out) {
  pistoris::Ftl native;
  ArxReturnCode rc = pistoris::readFtl(data, native);
  if (rc != ARX_OK) return inputFailure("Reference FTL", rc, path);

  auto reference = std::make_unique<pistoris::Model>();
  rc = pistoris::Model::importNative(*reference, native);
  if (rc != ARX_OK) return inputFailure("Reference Model", rc, path);
  log(ARX_LOG_INFO, "using reference FTL: %.*s", static_cast<int>(path.size()), path.data());
  out.reference = std::move(reference);
  return true;
}

}  // namespace cli::model
