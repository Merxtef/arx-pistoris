// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/load.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/document.hpp"
#include "arx_pistoris/resource_io/native_bundle.hpp"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/native_path.h"
#include "io/service.h"
#include "media/encoded.h"
#include "resources/input.h"
#include "resources/level_image_io.h"
#include "resources/native_bundle.h"
#include "routes/conversion_failure.h"
#include "routes/level/invocation.h"
#include "routes/level/options.h"
#include "routes/level/state.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cli::level {
namespace {

template <class Result>
bool inputFailure(const char* what, std::string_view path, const Result& result) {
  return conversionInputFailure(DiagnosticCode::kLevelInputFailed, what, path, result);
}

void loadNativeLevelImages(const pistoris::resource_io::LevelNativeBundle& bundle, LoadedLevelImages& out) {
  out = {};
  for (const auto& reference : bundle.resources().references()) {
    if (reference.role() != pistoris::resource_io::NativeResourceRole::kMinimap &&
        reference.role() != pistoris::resource_io::NativeResourceRole::kLoadingScreen)
      continue;
    const auto* file = resolvedNativeResource(bundle.resources(), reference);
    if (!file) continue;
    media::PreparedImage prepared;
    const ArxReturnCode rc =
        media::prepareImage(std::vector<std::uint8_t>(file->data().begin(), file->data().end()), prepared);
    if (rc != ARX_OK) {
      const std::string path = io_detail::pathToUtf8(file->nativePath());
      log(ARX_LOG_WARN,
          "Level %s sidecar was skipped because '%s' is invalid: %s (code %d)",
          reference.role() == pistoris::resource_io::NativeResourceRole::kMinimap ? "minimap" : "loading-screen",
          path.c_str(),
          pistoris::errorString(rc),
          static_cast<int>(rc));
      continue;
    }
    if (reference.role() == pistoris::resource_io::NativeResourceRole::kMinimap) {
      out.minimap.image = std::move(prepared);
      out.minimap.projection_offset = bundle.minimapProjectionOffset().value_or(pistoris::ArxVector2{});
    } else {
      out.loading_screen = std::move(prepared);
    }
  }
}

bool loadNative(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, IoService& io,
                NativeLevelFiles& out) {
  const Format input = inputs[invocation.input].facts.format;
  const pistoris::NativeTextMode text_mode =
      directCarrierTextMode(input, invocation.output.format, invocation.native_text_mode);
  const pistoris::resource_io::ResourceDocument* lighting =
      invocation.llf == kNoClassifiedPath ? nullptr : &inputs[invocation.llf].document;
  const pistoris::resource_io::ResourceDocument* scene =
      invocation.dlf == kNoClassifiedPath || invocation.dlf == invocation.input ? nullptr
                                                                                : &inputs[invocation.dlf].document;
  auto loaded =
      io.resources().loadLevelNativeBundle(inputs[invocation.input].document,
                                           lighting,
                                           scene,
                                           {.native_text_mode = text_mode, .suppress_related_resource_errors = false});
  if (!loaded) return inputFailure("Level native bundle", inputs[invocation.input].path, loaded);
  if (const auto* geometry = loaded->geometry()) {
    out.fts = geometry->carrier();
    out.text_mode = geometry->textMode();
  }
  if (const auto* member = loaded->lighting()) out.llf = member->carrier();
  if (const auto* member = loaded->scene()) out.dlf = member->carrier();
  if (!nativeTextureFiles(loaded->resources(),
                          pistoris::resource_io::NativeResourceRole::kTexture,
                          ARX_RESOURCE_KIND_LEVEL,
                          0,
                          io,
                          "Level texture image",
                          out.texture_files)) {
    diagnostic(DiagnosticCode::kLevelInputFailed, "Level native bundle contains an invalid texture reference");
    return false;
  }
  loadNativeLevelImages(*loaded, out.images);
  return true;
}

bool loadNativeIntermediate(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, const LevelOptions&,
                            IoService& io, IntermediateLevel& out) {
  NativeLevelFiles native;
  if (!loadNative(inputs, invocation, io, native)) return false;
  if (!native.fts) {
    diagnostic(DiagnosticCode::kLevelInputFailed,
               "Level native bundle has no geometry: %s",
               inputs[invocation.input].path.c_str());
    return false;
  }
  out.source_fts_offset = native.fts->scene.Mscenepos;
  std::vector<std::string> texture_sources;
  auto level = pistoris::Level::importNative(*native.fts,
                                             native.llf ? &*native.llf : nullptr,
                                             native.dlf ? &*native.dlf : nullptr,
                                             &texture_sources,
                                             native.text_mode);
  if (!level) return inputFailure("Level", inputs[invocation.input].path, level);
  for (const pistoris::NativeTextureFile& texture : native.texture_files) {
    const auto loaded =
        level->setTextureImage(texture.source_texture, {texture.encoded_image.data(), texture.encoded_image.size()});
    if (!loaded) return inputFailure("Level texture", texture.resource_path, loaded);
  }
  applyLevelImages(*level, native.images);
  out.level = std::move(*level);
  out.texture_source_paths = std::move(texture_sources);
  return true;
}

bool loadGlbIntermediate(std::vector<ClassifiedPath>& inputs, const Invocation& invocation, const LevelOptions& options,
                         IoService& io, IntermediateLevel& out) {
  const ClassifiedPath& input = inputs[invocation.input];
  pistoris::resource_io::LevelLoadOptions load_options;
  load_options.glb = options.glb_import;
  auto level = io.resources().loadLevel(input.document, nullptr, nullptr, load_options);
  if (!level) return inputFailure("GLB", input.path, level);
  out.level = std::move(*level);
  out.texture_source_paths.reserve(out.level.textureCount());
  for (const ArxTextureView texture : out.level.textures())
    out.texture_source_paths.emplace_back(texture.path.data, texture.path.size);
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

bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, const LevelOptions& options, IoService& io, bool native, LevelInput& out) {
  if (native) {
    if (!converter.load_native) return false;
    NativeLevelFiles& loaded = out.emplace<NativeLevelFiles>();
    if (converter.load_native(inputs, invocation, io, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateLevel& loaded = out.emplace<IntermediateLevel>();
  if (converter.load_intermediate(inputs, invocation, options, io, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::level
