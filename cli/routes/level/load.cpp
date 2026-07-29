// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/load.h"

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "resources/level_json.h"
#include "routes/level/invocation.h"
#include "routes/level/native_input.h"
#include "routes/level/options.h"
#include "routes/level/state.h"
#include "routes/types.h"

#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace cli::level {
namespace {

bool inputFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kLevelInputFailed,
             "%s Level input failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

bool loadNativeExtras(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, NativeLevelFiles& out) {
  if (invocation.llf != kNoClassifiedPath) {
    pistoris::Llf loaded;
    ArxReturnCode rc = decodeLlf(inputs[invocation.llf], loaded);
    if (rc != ARX_OK) return inputFailure("LLF", rc);
    out.llf = std::move(loaded);
  }

  if (invocation.dlf == kNoClassifiedPath) return true;

  pistoris::Dlf loaded;
  std::optional<pistoris::Llf> embedded_lighting;
  std::optional<pistoris::Llf>* embedded_output = out.llf ? nullptr : &embedded_lighting;
  ArxReturnCode rc = decodeDlf(inputs[invocation.dlf], loaded, embedded_output);
  if (rc != ARX_OK) return inputFailure("DLF", rc);
  out.dlf = std::move(loaded);
  if (embedded_lighting) out.llf = std::move(*embedded_lighting);
  return true;
}

bool loadNative(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation, NativeLevelFiles& out) {
  pistoris::Fts fts;
  ArxReturnCode rc = decodeFts(inputs[invocation.input], fts);
  if (rc != ARX_OK) return inputFailure("FTS", rc);
  out.fts = std::move(fts);
  if (!loadNativeExtras(inputs, invocation, out)) return false;

  const ClassifiedPath& primary = inputs[invocation.input];
  if (primary.facts.format == Format::kJson) {
    LevelJsonPath path;
    if (!parseLevelFtsJsonPath(primary.path, path)) {
      diagnostic(DiagnosticCode::kResourceSelectorInvalid,
                 "Level JSON input must use the central name level<N>.fts.json: %s",
                 primary.path.c_str());
      return false;
    }
    applyLevelNumber(path.level, *out.fts, out.dlf ? &*out.dlf : nullptr);
  }
  return true;
}

bool loadNativeIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                            const LevelOptions&, IntermediateLevel& out) {
  NativeLevelFiles native;
  if (!loadNative(inputs, invocation, native)) return false;
  if (!native.fts) {
    diagnostic(DiagnosticCode::kLevelInputFailed, "Native Level input did not produce FTS data");
    return false;
  }
  pistoris::Fts& fts = native.fts.value();

  pistoris::Level level;
  ArxReturnCode rc =
      pistoris::Level::fromNative(level, fts, native.llf ? &*native.llf : nullptr, native.dlf ? &*native.dlf : nullptr);
  if (rc != ARX_OK) return inputFailure("Native", rc);
  out.source_fts_offset = fts.scene.Mscenepos;
  out.level.swap(level);
  return true;
}

bool loadGlbIntermediate(const std::vector<ClassifiedPath>& inputs, const Invocation& invocation,
                         const LevelOptions& options, IntermediateLevel& out) {
  pistoris::Level level;
  ArxReturnCode rc = pistoris::Level::fromGlb(level, inputs[invocation.input].buffer, options.glb_import);
  if (rc != ARX_OK) return inputFailure("GLB", rc);
  out.level.swap(level);
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
               const Invocation& invocation, const LevelOptions& options, bool native, LevelInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeLevelFiles& loaded = out.emplace<NativeLevelFiles>();
    if (converter.load_native(inputs, invocation, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateLevel& loaded = out.emplace<IntermediateLevel>();
  if (converter.load_intermediate(inputs, invocation, options, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::level
