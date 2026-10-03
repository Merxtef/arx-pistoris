// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/load.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/sound.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <utility>
#include <variant>
#include <vector>

namespace cli::ambiance {
namespace {

template <class Result>
bool inputFailure(const char* what, const Result& result, const ClassifiedPath& input) {
  return conversionInputFailure(DiagnosticCode::kAmbianceInputFailed, what, input.path, result);
}

bool applyResourcePath(const ClassifiedPath& input, pistoris::Ambiance& out) {
  pistoris::paths::AmbiancePathView parsed;
  if (!pistoris::paths::ambianceFromAmb(input.path, parsed)) return true;
  const auto result = out.setResourcePath(input.path);
  return result || inputFailure("Ambiance resource identity", result, input);
}

bool decodeAmb(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Amb& out) {
  switch (input.facts.format) {
    case Format::kAmb: {
      auto result = pistoris::readAmb(input.buffer);
      if (!result) return inputFailure(formatName(input.facts.format), result, input);
      out = std::move(*result);
      return true;
    }
    case Format::kJson: {
      auto result = pistoris::fromAmbJson(byteStringView(input.buffer), text_mode);
      if (!result) return inputFailure(formatName(input.facts.format), result, input);
      out = std::move(*result);
      return true;
    }
    default:
      diagnostic(DiagnosticCode::kAmbianceUnsupportedInput, "Unsupported Ambiance input format");
      return false;
  }
}

bool loadNative(const ClassifiedPath& input, const Invocation& invocation, NativeAmbiance& out) {
  out.text_mode = directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  return decodeAmb(input, out.text_mode, out.ambiance);
}

bool loadNativeIntermediate(const ClassifiedPath& input, const Invocation& invocation, IntermediateAmbiance& out) {
  pistoris::Amb native;
  const pistoris::NativeTextMode text_mode = carrierTextMode(input.facts.format, invocation.native_text_mode);
  if (!decodeAmb(input, text_mode, native)) return false;
  std::vector<pistoris::SoundSourceReference> sound_sources;
  auto converted = pistoris::Ambiance::importNative(native, &sound_sources, text_mode);
  if (!converted) return inputFailure("AMB Ambiance", converted, input);
  if (!applyResourcePath(input, *converted)) return false;
  out.ambiance = std::move(*converted);
  out.sound_sources = std::move(sound_sources);
  return true;
}

bool loadGlbIntermediate(const ClassifiedPath& input, const Invocation& invocation, IntermediateAmbiance& out) {
  std::vector<pistoris::SoundSourceReference> sound_sources;
  auto converted = pistoris::Ambiance::importGlb(input.buffer, invocation.options.glb_import, &sound_sources);
  if (!converted) return inputFailure("GLB Ambiance", converted, input);
  out.ambiance = std::move(*converted);
  out.sound_sources = std::move(sound_sources);
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kAmb{loadNative, loadNativeIntermediate};
  static constexpr InputConverterDescriptor kGlb{nullptr, loadGlbIntermediate};
  switch (route.input) {
    case Format::kAmb:
    case Format::kJson:
      return &kAmb;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, bool native, AmbianceInput& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  if (native) {
    if (!converter.load_native) return false;
    NativeAmbiance& loaded = out.emplace<NativeAmbiance>();
    if (converter.load_native(input, invocation, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateAmbiance& loaded = out.emplace<IntermediateAmbiance>();
  if (converter.load_intermediate(input, invocation, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::ambiance
