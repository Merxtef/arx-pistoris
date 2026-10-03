// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/load.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
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

bool decodeTea(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Tea& out) {
  switch (input.facts.format) {
    case Format::kTea: {
      auto result = pistoris::readTea(input.buffer);
      if (!result) return inputFailure(formatName(input.facts.format), result, input.path);
      out = std::move(*result);
      return true;
    }
    case Format::kJson: {
      auto result = pistoris::fromTeaJson(byteStringView(input.buffer), text_mode);
      if (!result) return inputFailure(formatName(input.facts.format), result, input.path);
      out = std::move(*result);
      return true;
    }
    default:
      diagnostic(DiagnosticCode::kAnimationUnsupportedInput, "Unsupported Animation input format");
      return false;
  }
}

bool applyResourcePath(const ClassifiedPath& input, pistoris::Animation& out) {
  pistoris::paths::AnimationPathView parsed;
  if (!pistoris::paths::animationFromTea(input.path, parsed)) return true;
  const auto result = out.setResourcePath(input.path);
  return result || inputFailure("Animation resource identity", result, input.path);
}

bool loadNative(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, NativeAnimation& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  out.text_mode = directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  return decodeTea(input, out.text_mode, out.animation);
}

bool loadIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                      IntermediateAnimation& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  const pistoris::NativeTextMode text_mode = carrierTextMode(input.facts.format, invocation.native_text_mode);
  pistoris::Tea native;
  if (!decodeTea(input, text_mode, native)) return false;
  auto converted = pistoris::Animation::importNative(native, &out.sound_sources, text_mode);
  if (!converted) return inputFailure("TEA Animation", converted, input.path);
  out.animation = std::move(*converted);
  return applyResourcePath(input, out.animation);
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

bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, bool native, AnimationInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeAnimation& loaded = out.emplace<NativeAnimation>();
    if (converter.load_native(inputs, invocation, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateAnimation& loaded = out.emplace<IntermediateAnimation>();
  if (converter.load_intermediate(inputs, invocation, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::animation
