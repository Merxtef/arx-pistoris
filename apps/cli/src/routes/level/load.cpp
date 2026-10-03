// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/load.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/text.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "resources/level_json.h"
#include "routes/conversion_failure.h"
#include "routes/level/invocation.h"
#include "routes/level/native_carriers.h"
#include "routes/level/options.h"
#include "routes/level/state.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::level {
namespace {

bool inputFailure(const char* what, std::string_view path, ArxReturnCode /* rc */, std::string_view description = {}) {
  if (!description.empty()) return conversionInputFailure(DiagnosticCode::kLevelInputFailed, what, path, description);
  return conversionInputFailure(DiagnosticCode::kLevelInputFailed, what, path);
}

template <class Result>
bool inputFailure(const char* what, std::string_view path, const Result& result) {
  return conversionInputFailure(DiagnosticCode::kLevelInputFailed, what, path, result);
}

bool loadNativeExtras(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                      pistoris::NativeTextMode text_mode, InputConverterDescriptor::DecodedDlfInput* decoded_dlf,
                      NativeLevelFiles& out) {
  if (invocation.llf != kNoClassifiedPath) {
    pistoris::Llf loaded;
    std::string failure;
    ArxReturnCode rc = decodeLlf(inputs[invocation.llf], loaded, &failure);
    if (rc != ARX_OK) return inputFailure("LLF", inputs[invocation.llf].path, rc, failure);
    out.llf = std::move(loaded);
  }

  if (invocation.dlf == kNoClassifiedPath) return true;

  if (decoded_dlf) {
    out.dlf = std::move(decoded_dlf->dlf);
    if (!out.llf && decoded_dlf->embedded_lighting) out.llf = std::move(*decoded_dlf->embedded_lighting);
    return true;
  }

  pistoris::Dlf loaded;
  std::optional<pistoris::Llf> embedded_lighting;
  std::optional<pistoris::Llf>* embedded_output = out.llf ? nullptr : &embedded_lighting;
  std::string failure;
  ArxReturnCode rc = decodeDlf(inputs[invocation.dlf], text_mode, loaded, embedded_output, &failure);
  if (rc != ARX_OK) return inputFailure("DLF", inputs[invocation.dlf].path, rc, failure);
  out.dlf = std::move(loaded);
  if (embedded_lighting) out.llf = std::move(*embedded_lighting);
  return true;
}

bool loadNativeFiles(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                     pistoris::NativeTextMode text_mode, InputConverterDescriptor::DecodedDlfInput* decoded_dlf,
                     NativeLevelFiles& out) {
  out.text_mode = text_mode;
  pistoris::Fts fts;
  std::string failure;
  ArxReturnCode rc = decodeFts(inputs[invocation.input], text_mode, fts, &failure);
  if (rc != ARX_OK) return inputFailure("FTS", inputs[invocation.input].path, rc, failure);
  out.fts = std::move(fts);
  if (!loadNativeExtras(inputs, invocation, text_mode, decoded_dlf, out)) return false;

  const ClassifiedPath& primary = inputs[invocation.input];
  if (primary.facts.format == Format::kJson) {
    LevelJsonPath path;
    if (!parseLevelFtsJsonPath(primary.path, path)) {
      diagnostic(DiagnosticCode::kLevelInputFailed,
                 "Level JSON input must use the central name level<N>.fts.json: %s",
                 primary.path.c_str());
      return false;
    }
    if (out.dlf) applyLevelNumber(path.level, *out.dlf);
  }
  return true;
}

bool loadNative(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                InputConverterDescriptor::DecodedDlfInput* decoded_dlf, NativeLevelFiles& out) {
  const Format input = inputs[invocation.input].facts.format;
  const pistoris::NativeTextMode text_mode =
      directCarrierTextMode(input, invocation.output.format, invocation.native_text_mode);
  return loadNativeFiles(inputs, invocation, text_mode, decoded_dlf, out);
}

bool loadNativeIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                            const LevelOptions&, InputConverterDescriptor::DecodedDlfInput* decoded_dlf,
                            IntermediateLevel& out) {
  NativeLevelFiles native;
  const Format input = inputs[invocation.input].facts.format;
  const pistoris::NativeTextMode text_mode = carrierTextMode(input, invocation.native_text_mode);
  if (!loadNativeFiles(inputs, invocation, text_mode, decoded_dlf, native)) return false;
  if (!native.fts) {
    diagnostic(DiagnosticCode::kLevelInputFailed, "Native Level input did not produce FTS data");
    return false;
  }
  pistoris::Fts& fts = native.fts.value();

  std::vector<std::string> texture_source_paths;
  auto level = pistoris::Level::importNative(fts,
                                             native.llf ? &*native.llf : nullptr,
                                             native.dlf ? &*native.dlf : nullptr,
                                             &texture_source_paths,
                                             native.text_mode);
  if (!level) return inputFailure("Native", inputs[invocation.input].path, level);
  out.source_fts_offset = fts.scene.Mscenepos;
  out.level = std::move(*level);
  out.texture_source_paths = std::move(texture_source_paths);
  return true;
}

bool loadGlbIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                         const LevelOptions& options, InputConverterDescriptor::DecodedDlfInput*,
                         IntermediateLevel& out) {
  std::vector<std::string> texture_source_paths;
  auto level =
      pistoris::Level::importGlb(inputs[invocation.input].buffer, options.glb_import, nullptr, &texture_source_paths);
  if (!level) return inputFailure("GLB", inputs[invocation.input].path, level);
  out.level = std::move(*level);
  out.texture_source_paths = std::move(texture_source_paths);
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kNative{loadNative, loadNativeIntermediate};
  static constexpr InputConverterDescriptor kGlb{nullptr, loadGlbIntermediate};
  switch (route.input) {
    case Format::kFts:
    case Format::kDlf:
    case Format::kJson: {
      return &kNative;
    }
    case Format::kGlb: {
      return &kGlb;
    }
    default:
      return nullptr;
  }
}

bool loadInput(const InputConverterDescriptor& converter, const std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const LevelOptions& options,
               InputConverterDescriptor::DecodedDlfInput* decoded_dlf, bool native, LevelInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeLevelFiles& loaded = out.emplace<NativeLevelFiles>();
    if (converter.load_native(inputs, invocation, decoded_dlf, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateLevel& loaded = out.emplace<IntermediateLevel>();
  if (converter.load_intermediate(inputs, invocation, options, decoded_dlf, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::level
