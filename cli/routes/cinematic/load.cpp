// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/cinematic/load.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "resources/cinematic_sound_io.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/state.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <variant>
#include <vector>

namespace cli::cinematic {
namespace {

bool inputFailure(const char* what, ArxReturnCode rc, const ClassifiedPath& input) {
  diagnostic(DiagnosticCode::kCinematicInputFailed,
             "%s input failed (%s): %s (code %d)",
             what,
             input.path.c_str(),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool decodeCin(const ClassifiedPath& input, pistoris::Cin& out) {
  const ArxReturnCode rc = pistoris::readCin(input.buffer, out);
  return rc == ARX_OK || inputFailure("CIN", rc, input);
}

bool applyResourcePath(const ClassifiedPath& input, pistoris::Cinematic& out) {
  pistoris::paths::CinematicPathView parsed;
  if (!pistoris::paths::cinematicFromCin(input.path, parsed)) return true;
  const ArxReturnCode rc = out.setResourcePath(input.path);
  return rc == ARX_OK || inputFailure("Cinematic resource identity", rc, input);
}

bool loadNative(const ClassifiedPath& input, const Invocation& invocation, NativeCinematic& out) {
  out.text_mode = directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  return decodeCin(input, out.cinematic);
}

bool loadNativeIntermediate(const ClassifiedPath& input, const Invocation& invocation, IntermediateCinematic& out) {
  pistoris::Cin native;
  if (!decodeCin(input, native)) return false;
  pistoris::Cinematic converted;
  const ArxReturnCode rc = pistoris::Cinematic::importNative(
      converted, native, &out.illustration_sources, &out.sound_sources, invocation.native_text_mode);
  if (rc != ARX_OK) return inputFailure("CIN Cinematic", rc, input);
  if (!applyResourcePath(input, converted)) return false;
  out.cinematic.swap(converted);
  out.sound_source_format = CinematicSoundSourceFormat::kCin;
  return true;
}

bool loadGlbIntermediate(const ClassifiedPath& input, const Invocation&, IntermediateCinematic& out) {
  pistoris::Cinematic converted;
  const ArxReturnCode rc = pistoris::Cinematic::importGlb(converted, input.buffer, &out.sound_sources);
  if (rc != ARX_OK) return inputFailure("GLB Cinematic", rc, input);
  out.cinematic.swap(converted);
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
