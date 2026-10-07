// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/model_input.h"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/texture.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "resources/input.h"
#include "routes/conversion_failure.h"

#include <cstddef>
#include <string_view>
#include <utility>

namespace cli {
namespace {

template <class Result>
bool conversionFailure(DiagnosticCode code, std::string_view description, const ClassifiedPath& input,
                       const Result& result) {
  return conversionInputFailure(code, description, input.path, result);
}

bool loadModelResource(const ClassifiedPath& input, IoService& io, const ModelInputConversionOptions& options,
                       DiagnosticCode failure_code, std::string_view description, ConvertedModelInput& out) {
  const pistoris::resource_io::ModelLoadOptions load_options = {.glb = options.glb,
                                                                .native_text_mode = options.native_text_mode};
  auto loaded = io.resources().loadModel(input.document, load_options);
  if (!loaded) return conversionFailure(failure_code, description, input, loaded);
  out.model = std::move(loaded->model);
  out.animations = std::move(loaded->animations);
  out.texture_source_paths.reserve(out.model.textureCount());
  for (const ArxTextureView texture : out.model.textures())
    out.texture_source_paths.emplace_back(texture.path.data, texture.path.size);
  for (std::size_t animation_index = 0; animation_index < out.animations.size(); ++animation_index) {
    const pistoris::Animation& animation = out.animations[animation_index];
    for (std::size_t sound_index = 0; sound_index < animation.soundCount(); ++sound_index) {
      const ArxSoundView sound = animation.sounds()[sound_index];
      out.sound_sources.push_back(
          {animation_index,
           {static_cast<pistoris::SoundIndex>(sound_index), std::string(sound.path.data, sound.path.size)}});
    }
  }
  return true;
}

}  // namespace

bool isModelInput(FileFacts facts) noexcept {
  switch (facts.format) {
    case Format::kFtl:
    case Format::kObj:
    case Format::kGlb:
      return facts.kind != PayloadKind::kTea;
    case Format::kJson:
      return facts.kind == PayloadKind::kFtl || facts.kind == PayloadKind::kUnknown;
    default:
      return false;
  }
}

bool convertModelInput(const ClassifiedPath& input, IoService& io, const ModelInputConversionOptions& options,
                       DiagnosticCode failure_code, std::string_view description, ConvertedModelInput& out) {
  ConvertedModelInput converted;
  if (!loadModelResource(input, io, options, failure_code, description, converted)) return false;
  out.model.swap(converted.model);
  out.animations = std::move(converted.animations);
  out.sound_sources = std::move(converted.sound_sources);
  out.texture_source_paths = std::move(converted.texture_source_paths);
  return true;
}

}  // namespace cli
