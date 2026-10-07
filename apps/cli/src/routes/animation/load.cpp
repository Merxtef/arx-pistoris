// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/load.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "resources/input.h"
#include "resources/native_bundle.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::animation {
namespace {

template <class Result>
bool inputFailure(const char* what, const Result& result, std::string_view path) {
  return conversionInputFailure(DiagnosticCode::kAnimationInputFailed, what, path, result);
}

bool loadNative(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                NativeAnimation& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  const pistoris::NativeTextMode text_mode =
      directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  auto loaded = io.resources().loadAnimationNativeBundle(
      input.document, {.native_text_mode = text_mode, .suppress_related_resource_errors = false});
  if (!loaded) return inputFailure("Animation native bundle", loaded, input.path);
  out.animation = loaded->animation().carrier();
  out.text_mode = loaded->animation().textMode();
  if (nativeSoundFiles(loaded->resources(), ARX_RESOURCE_KIND_ANIMATION, 0, io, "Animation sound", out.sound_files))
    return true;
  diagnostic(DiagnosticCode::kAnimationInputFailed, "Animation native bundle contains an invalid sound reference");
  return false;
}

bool loadIntermediate(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                      IntermediateAnimation& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  NativeAnimation native;
  if (!loadNative(inputs, invocation, io, native)) return false;
  std::vector<pistoris::SoundSourceReference> sources;
  auto converted = pistoris::Animation::importNative(native.animation, &sources, native.text_mode);
  if (!converted) return inputFailure("Animation", converted, input.path);
  pistoris::paths::AnimationPathView parsed;
  if (pistoris::paths::animationFromTea(input.path, parsed)) {
    const auto identity = converted->setResourcePath(input.path);
    if (!identity) return inputFailure("Animation resource identity", identity, input.path);
  }
  for (const pistoris::SoundFile& sound : native.sound_files) {
    const auto loaded =
        converted->setSoundData(sound.source_sound, {sound.encoded_audio.data(), sound.encoded_audio.size()});
    if (!loaded) return inputFailure("Animation sound", loaded, sound.path);
  }
  out.animation = std::move(*converted);
  out.sound_sources = std::move(sources);
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kNative{loadNative, loadIntermediate};
  switch (route.input) {
    case Format::kTea:
    case Format::kJson:
      return &kNative;
    default:
      return nullptr;
  }
}

bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, IoService& io, bool native, AnimationInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeAnimation& loaded = out.emplace<NativeAnimation>();
    if (converter.load_native(inputs, invocation, io, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateAnimation& loaded = out.emplace<IntermediateAnimation>();
  if (converter.load_intermediate(inputs, invocation, io, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::animation
