// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/load.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "resources/input.h"
#include "resources/native_bundle.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <cstddef>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cli::ambiance {
namespace {

template <class Result>
bool inputFailure(const char* what, const Result& result, const ClassifiedPath& input) {
  return conversionInputFailure(DiagnosticCode::kAmbianceInputFailed, what, input.path, result);
}

bool loadNative(ClassifiedPath& input, const Invocation& invocation, IoService& io, NativeAmbiance& out) {
  const pistoris::NativeTextMode text_mode =
      directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  auto loaded = io.resources().loadAmbianceNativeBundle(
      input.document, {.native_text_mode = text_mode, .suppress_related_resource_errors = false});
  if (!loaded) return inputFailure("Ambiance native bundle", loaded, input);
  out.ambiance = loaded->ambiance().carrier();
  out.text_mode = loaded->ambiance().textMode();
  if (nativeSoundFiles(loaded->resources(), ARX_RESOURCE_KIND_AMBIANCE, 0, io, "Ambiance sound", out.sound_files))
    return true;
  diagnostic(DiagnosticCode::kAmbianceInputFailed, "Ambiance native bundle contains an invalid sound reference");
  return false;
}

bool loadIntermediate(ClassifiedPath& input, const Invocation& invocation, IoService& io, IntermediateAmbiance& out) {
  if (input.facts.format == Format::kAmb || input.facts.format == Format::kJson) {
    NativeAmbiance native;
    if (!loadNative(input, invocation, io, native)) return false;
    std::vector<pistoris::SoundSourceReference> sources;
    auto converted = pistoris::Ambiance::importNative(native.ambiance, &sources, native.text_mode);
    if (!converted) return inputFailure("Ambiance", converted, input);
    for (const pistoris::SoundFile& sound : native.sound_files) {
      const auto loaded =
          converted->setSoundData(sound.source_sound, {sound.encoded_audio.data(), sound.encoded_audio.size()});
      if (!loaded) return inputFailure("Ambiance sound", loaded, input);
    }
    out.ambiance = std::move(*converted);
    out.sound_sources = std::move(sources);
    return true;
  }
  const pistoris::resource_io::AmbianceLoadOptions options = {
      .glb = invocation.options.glb_import,
      .native_text_mode = carrierTextMode(input.facts.format, invocation.native_text_mode)};
  auto converted = io.resources().loadAmbiance(input.document, options);
  if (!converted) return inputFailure("Ambiance", converted, input);
  out.ambiance = std::move(*converted);
  out.sound_sources.reserve(out.ambiance.soundCount());
  for (std::size_t index = 0; index < out.ambiance.soundCount(); ++index) {
    const ArxSoundView sound = out.ambiance.sounds()[index];
    out.sound_sources.push_back(
        {static_cast<pistoris::SoundIndex>(index), std::string(sound.path.data, sound.path.size)});
  }
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kAmb{loadNative, loadIntermediate};
  static constexpr InputConverterDescriptor kGlb{nullptr, loadIntermediate};
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

bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, IoService& io, bool native, AmbianceInput& out) {
  ClassifiedPath& input = inputs[invocation.input];
  if (native) {
    if (!converter.load_native) return false;
    NativeAmbiance& loaded = out.emplace<NativeAmbiance>();
    if (converter.load_native(input, invocation, io, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateAmbiance& loaded = out.emplace<IntermediateAmbiance>();
  if (converter.load_intermediate(input, invocation, io, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::ambiance
