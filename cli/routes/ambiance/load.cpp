// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/load.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/sound.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "routes/ambiance/invocation.h"
#include "routes/ambiance/options.h"
#include "routes/ambiance/state.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <utility>
#include <variant>
#include <vector>

namespace cli::ambiance {
namespace {

bool inputFailure(const char* what, ArxReturnCode rc, const ClassifiedPath& input) {
  diagnostic(DiagnosticCode::kAmbianceInputFailed,
             "%s input failed (%s): %s (code %d)",
             what,
             input.path.c_str(),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool applyResourcePath(const ClassifiedPath& input, pistoris::Ambiance& out) {
  pistoris::paths::AmbiancePathView parsed;
  if (!pistoris::paths::ambianceFromAmb(input.path, parsed)) return true;
  const ArxReturnCode rc = out.setResourcePath(input.path);
  return rc == ARX_OK || inputFailure("Ambiance resource identity", rc, input);
}

bool decodeAmb(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Amb& out) {
  ArxReturnCode rc = ARX_OK;
  switch (input.facts.format) {
    case Format::kAmb:
      rc = pistoris::readAmb(input.buffer, out);
      break;
    case Format::kJson:
      rc = pistoris::fromJson(byteStringView(input.buffer), out, text_mode);
      break;
    default:
      diagnostic(DiagnosticCode::kAmbianceUnsupportedInput, "Unsupported Ambiance input format");
      return false;
  }
  return rc == ARX_OK || inputFailure(formatName(input.facts.format), rc, input);
}

bool loadNative(const ClassifiedPath& input, const Invocation& invocation, NativeAmbiance& out) {
  out.text_mode = directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  return decodeAmb(input, out.text_mode, out.ambiance);
}

bool loadNativeIntermediate(const ClassifiedPath& input, const Invocation& invocation, IntermediateAmbiance& out) {
  pistoris::Amb native;
  const pistoris::NativeTextMode text_mode = carrierTextMode(input.facts.format, invocation.native_text_mode);
  if (!decodeAmb(input, text_mode, native)) return false;
  pistoris::Ambiance converted;
  std::vector<pistoris::SoundSourceReference> sound_sources;
  const ArxReturnCode rc = pistoris::Ambiance::importNative(converted, native, &sound_sources, text_mode);
  if (rc != ARX_OK) return inputFailure("AMB Ambiance", rc, input);
  if (!applyResourcePath(input, converted)) return false;
  out.ambiance.swap(converted);
  out.sound_sources = std::move(sound_sources);
  return true;
}

bool loadGlbIntermediate(const ClassifiedPath& input, const Invocation& invocation, IntermediateAmbiance& out) {
  pistoris::Ambiance converted;
  std::vector<pistoris::SoundSourceReference> sound_sources;
  const ArxReturnCode rc =
      pistoris::Ambiance::importGlb(converted, input.buffer, invocation.options.glb_import, &sound_sources);
  if (rc != ARX_OK) return inputFailure("GLB Ambiance", rc, input);
  out.ambiance.swap(converted);
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
