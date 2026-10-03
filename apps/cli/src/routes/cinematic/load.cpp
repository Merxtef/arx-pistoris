// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/cinematic/load.h"

#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "resources/cinematic_sound_io.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <utility>
#include <variant>
#include <vector>

namespace cli::cinematic {
namespace {

template <class Result>
bool inputFailure(const char* what, const Result& result, const ClassifiedPath& input) {
  return conversionInputFailure(DiagnosticCode::kCinematicInputFailed, what, input.path, result);
}

bool decodeCin(const ClassifiedPath& input, pistoris::Cin& out) {
  auto result = pistoris::readCin(input.buffer);
  if (!result) return inputFailure("CIN", result, input);
  out = std::move(*result);
  return true;
}

bool applyResourcePath(const ClassifiedPath& input, pistoris::Cinematic& out) {
  pistoris::paths::CinematicPathView parsed;
  if (!pistoris::paths::cinematicFromCin(input.path, parsed)) return true;
  const auto result = out.setResourcePath(input.path);
  return result || inputFailure("Cinematic resource identity", result, input);
}

bool loadNative(const ClassifiedPath& input, const Invocation& invocation, NativeCinematic& out) {
  out.text_mode = directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  return decodeCin(input, out.cinematic);
}

bool loadNativeIntermediate(const ClassifiedPath& input, const Invocation& invocation, IntermediateCinematic& out) {
  pistoris::Cin native;
  if (!decodeCin(input, native)) return false;
  auto converted = pistoris::Cinematic::importNative(
      native, &out.illustration_sources, &out.sound_sources, invocation.native_text_mode);
  if (!converted) return inputFailure("CIN Cinematic", converted, input);
  if (!applyResourcePath(input, *converted)) return false;
  out.cinematic = std::move(*converted);
  out.sound_source_format = CinematicSoundSourceFormat::kCin;
  return true;
}

bool loadGlbIntermediate(const ClassifiedPath& input, const Invocation&, IntermediateCinematic& out) {
  auto converted = pistoris::Cinematic::importGlb(input.buffer, &out.sound_sources);
  if (!converted) return inputFailure("GLB Cinematic", converted, input);
  out.cinematic = std::move(*converted);
  out.sound_source_format = CinematicSoundSourceFormat::kGlb;
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kCin{loadNative, loadNativeIntermediate};
  static constexpr InputConverterDescriptor kGlb{nullptr, loadGlbIntermediate};
  switch (route.input) {
    case Format::kCin:
      return &kCin;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, bool native, CinematicInput& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  if (native) {
    if (!converter.load_native) return false;
    NativeCinematic& loaded = out.emplace<NativeCinematic>();
    if (converter.load_native(input, invocation, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateCinematic& loaded = out.emplace<IntermediateCinematic>();
  if (converter.load_intermediate(input, invocation, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::cinematic
