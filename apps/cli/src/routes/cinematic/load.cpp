// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/cinematic/load.h"

#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/native_bundle.hpp"
#include "arx_pistoris/resource_io/resources.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/service.h"
#include "resources/cinematic_sound_io.h"
#include "resources/input.h"
#include "resources/native_bundle.h"
#include "routes/cinematic/invocation.h"
#include "routes/cinematic/state.h"
#include "routes/conversion_failure.h"
#include "routes/native_text.h"
#include "routes/types.h"

#include <initializer_list>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace cli::cinematic {
namespace {

template <class Result>
bool inputFailure(const char* what, const Result& result, const ClassifiedPath& input) {
  return conversionInputFailure(DiagnosticCode::kCinematicInputFailed, what, input.path, result);
}

bool loadNative(ClassifiedPath& input, const Invocation& invocation, IoService& io, NativeCinematic& out) {
  const pistoris::NativeTextMode text_mode =
      directCarrierTextMode(input.facts.format, invocation.output.format, invocation.native_text_mode);
  auto loaded = io.resources().loadCinematicNativeBundle(
      input.document, {.native_text_mode = text_mode, .suppress_related_resource_errors = false});
  if (!loaded) return inputFailure("Cinematic native bundle", loaded, input);
  out.cinematic = loaded->cinematic().carrier();
  out.text_mode = loaded->cinematic().textMode();
  if (!nativeTextureFiles(loaded->resources(),
                          pistoris::resource_io::NativeResourceRole::kIllustration,
                          ARX_RESOURCE_KIND_CINEMATIC,
                          0,
                          io,
                          "Cinematic illustration image",
                          out.illustration_files) ||
      !nativeSoundFiles(loaded->resources(), ARX_RESOURCE_KIND_CINEMATIC, 0, io, "Cinematic sound", out.sound_files)) {
    diagnostic(DiagnosticCode::kCinematicInputFailed, "Cinematic native bundle contains an invalid media reference");
    return false;
  }
  return true;
}

bool loadIntermediate(ClassifiedPath& input, const Invocation& invocation, IoService& io, IntermediateCinematic& out) {
  const pistoris::resource_io::CinematicLoadOptions options = {.native_text_mode = invocation.native_text_mode};
  auto converted = io.resources().loadCinematic(input.document, options);
  if (!converted) return inputFailure("Cinematic", converted, input);
  out.cinematic = std::move(*converted);
  out.illustration_sources.reserve(out.cinematic.textureCount());
  for (const ArxTextureView illustration : out.cinematic.textures())
    out.illustration_sources.emplace_back(illustration.path.data, illustration.path.size);
  for (const pistoris::SoundKind kind : {pistoris::SoundKind::kEffect, pistoris::SoundKind::kSpeech}) {
    for (const ArxCinematicSoundView& sound : out.cinematic.sounds(kind)) {
      out.sound_sources.push_back({sound.handle, std::string(sound.path.data, sound.path.size)});
    }
  }
  out.sound_source_format =
      input.facts.format == Format::kGlb ? CinematicSoundSourceFormat::kGlb : CinematicSoundSourceFormat::kCin;
  return true;
}

}  // namespace

const InputConverterDescriptor* inputConverterDescriptor(Route route) {
  static constexpr InputConverterDescriptor kCin{loadNative, loadIntermediate};
  static constexpr InputConverterDescriptor kGlb{nullptr, loadIntermediate};
  switch (route.input) {
    case Format::kCin:
      return &kCin;
    case Format::kGlb:
      return &kGlb;
    default:
      return nullptr;
  }
}

bool loadInput(const InputConverterDescriptor& converter, std::vector<ClassifiedPath>& inputs,
               const Invocation& invocation, IoService& io, bool native, CinematicInput& out) {
  ClassifiedPath& input = inputs[invocation.input];
  if (native) {
    if (!converter.load_native) return false;
    NativeCinematic& loaded = out.emplace<NativeCinematic>();
    if (converter.load_native(input, invocation, io, loaded)) return true;
    out.emplace<std::monostate>();
    return false;
  }

  if (!converter.load_intermediate) return false;
  IntermediateCinematic& loaded = out.emplace<IntermediateCinematic>();
  if (converter.load_intermediate(input, invocation, io, loaded)) return true;
  out.emplace<std::monostate>();
  return false;
}

}  // namespace cli::cinematic
