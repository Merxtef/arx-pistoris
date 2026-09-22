// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/animation/load.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"

#include "base/bytes.h"
#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "routes/animation/invocation.h"
#include "routes/animation/state.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <string_view>
#include <variant>
#include <vector>

namespace cli::animation {
namespace {

bool inputFailure(const char* what, ArxReturnCode rc, std::string_view path) {
  diagnostic(DiagnosticCode::kAnimationInputFailed,
             "%s input failed (%.*s): %s (code %d)",
             what,
             static_cast<int>(path.size()),
             path.data(),
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool decodeTea(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Tea& out) {
  ArxReturnCode rc = ARX_OK;
  switch (input.facts.format) {
    case Format::kTea:
      rc = pistoris::readTea(input.buffer, out);
      break;
    case Format::kJson:
      rc = pistoris::fromJson(byteStringView(input.buffer), out, text_mode);
      break;
    default:
      diagnostic(DiagnosticCode::kAnimationUnsupportedInput, "Unsupported Animation input format");
      return false;
  }
  return rc == ARX_OK || inputFailure(formatName(input.facts.format), rc, input.path);
}

bool applyResourcePath(const ClassifiedPath& input, pistoris::Animation& out) {
  pistoris::paths::AnimationPathView parsed;
  if (!pistoris::paths::animationFromTea(input.path, parsed)) return true;
  const ArxReturnCode rc = out.setResourcePath(input.path);
  return rc == ARX_OK || inputFailure("Animation resource identity", rc, input.path);
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
  const ArxReturnCode rc = pistoris::Animation::importNative(out.animation, native, &out.sound_sources, text_mode);
  if (rc != ARX_OK) return inputFailure("TEA Animation", rc, input.path);
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
