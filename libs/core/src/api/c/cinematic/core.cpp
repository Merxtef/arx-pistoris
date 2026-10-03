// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/text.h"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/texture.h"

#include "api/c/cinematic/internal.h"
#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"
#include "api/c/texture/internal.h"

#include <memory>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_cinematic_create(ArxCinematic** out_cinematic, ArxError* error) noexcept {
  if (!out_cinematic) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_cinematic = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxCinematic>();
    *out_cinematic = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_clone(const ArxCinematic* cinematic, ArxCinematic** out_cinematic,
                                           ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_cinematic) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_cinematic = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxCinematic>();
    result->value = cinematic->value;
    *out_cinematic = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

void arx_pistoris_cinematic_destroy(ArxCinematic* cinematic) noexcept { delete cinematic; }

ArxReturnCode arx_pistoris_cinematic_reset(ArxCinematic* cinematic, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.reset(), error);
}

ArxReturnCode arx_pistoris_cinematic_import_native(const ArxCin* native, ArxCinematic** out_cinematic,
                                                   ArxTextureSourcePaths** out_illustration_sources,
                                                   ArxCinematicSoundSourceReferences** out_sound_sources,
                                                   ArxNativeTextMode text_mode, ArxError* error) noexcept {
  if (!native) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_cinematic) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_cinematic = nullptr;
  if (out_illustration_sources) *out_illustration_sources = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxCinematic>();
    std::unique_ptr<ArxTextureSourcePaths> illustrations;
    std::unique_ptr<ArxCinematicSoundSourceReferences> sounds;
    if (out_illustration_sources) illustrations = std::make_unique<ArxTextureSourcePaths>();
    if (out_sound_sources) sounds = std::make_unique<ArxCinematicSoundSourceReferences>();
    auto imported = pistoris::Cinematic::importNative(native->value,
                                                      illustrations ? &illustrations->value : nullptr,
                                                      sounds ? &sounds->value : nullptr,
                                                      pistoris::c_api::nativeTextMode(text_mode));
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_cinematic = result.release();
    if (out_illustration_sources) *out_illustration_sources = illustrations.release();
    if (out_sound_sources) *out_sound_sources = sounds.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_bake_native(const ArxCinematic* cinematic,
                                                 const ArxNativeCinematicBakeOptions* options, ArxCin** out_native,
                                                 ArxNativeTextureFiles** out_illustrations,
                                                 ArxCinematicSoundFiles** out_sounds, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_native) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_native = nullptr;
  if (out_illustrations) *out_illustrations = nullptr;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    if (options && !pistoris::c_api::validNativeTextMode(options->text_mode))
      return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
    const pistoris::NativeCinematicBakeOptions cpp_options{
        .include_illustration_files = out_illustrations && (!options || options->include_illustration_files != 0),
        .include_sound_files = out_sounds && (!options || options->include_sound_files != 0),
        .illustration_format =
            options ? options->illustration_format : static_cast<ArxImageFormat>(ARX_IMAGE_FORMAT_UNKNOWN),
        .text_mode = options ? pistoris::c_api::nativeTextMode(options->text_mode) : pistoris::NativeTextMode::kAuto,
    };
    auto baked = cinematic->value.bakeNativeBundle(cpp_options);
    if (!baked) return pistoris::c_api::publish(baked, error);
    pistoris::NativeCinematicBundle bundle = std::move(*baked);

    auto native = std::make_unique<ArxCin>();
    native->value = std::move(bundle.cin);
    std::unique_ptr<ArxNativeTextureFiles> illustrations;
    std::unique_ptr<ArxCinematicSoundFiles> sounds;
    if (out_illustrations) {
      illustrations = std::make_unique<ArxNativeTextureFiles>();
      illustrations->value = std::move(bundle.illustration_files);
    }
    if (out_sounds) {
      sounds = std::make_unique<ArxCinematicSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
    }
    *out_native = native.release();
    if (out_illustrations) *out_illustrations = illustrations.release();
    if (out_sounds) *out_sounds = sounds.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_cinematic_validate(const ArxCinematic* cinematic, ArxError* error) noexcept {
  if (!cinematic) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(cinematic->value.validate(), error);
}

// NOLINTEND(readability-identifier-naming)
