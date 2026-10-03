// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/text.h"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"
#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"
#include "api/c/sound/internal.h"

#include <memory>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_create(ArxAmbiance** out_ambiance, ArxError* error) noexcept {
  if (!out_ambiance) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_ambiance = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    *out_ambiance = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_ambiance_clone(const ArxAmbiance* ambiance, ArxAmbiance** out_ambiance,
                                          ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_ambiance) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_ambiance = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    result->value = ambiance->value;
    *out_ambiance = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

void arx_pistoris_ambiance_destroy(ArxAmbiance* ambiance) noexcept { delete ambiance; }

ArxReturnCode arx_pistoris_ambiance_reset(ArxAmbiance* ambiance, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(ambiance->value.reset(), error);
}

ArxReturnCode arx_pistoris_ambiance_import_native(const ArxAmb* native, ArxAmbiance** out_ambiance,
                                                  ArxSoundSourceReferences** out_sound_sources,
                                                  ArxNativeTextMode text_mode, ArxError* error) noexcept {
  if (!native) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_ambiance) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_ambiance = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    std::unique_ptr<ArxSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxSoundSourceReferences>();
    auto imported = pistoris::Ambiance::importNative(
        native->value, sources ? &sources->value : nullptr, pistoris::c_api::nativeTextMode(text_mode));
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_ambiance = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_ambiance_bake_native(const ArxAmbiance* ambiance,
                                                const ArxNativeAmbianceBakeOptions* options, ArxAmb** out_native,
                                                ArxSoundFiles** out_sounds, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_native) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_native = nullptr;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    if (options && !pistoris::c_api::validNativeTextMode(options->text_mode))
      return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
    const pistoris::NativeAmbianceBakeOptions cpp_options{
        .include_sound_files = out_sounds && (!options || options->include_sound_files != 0),
        .text_mode = options ? pistoris::c_api::nativeTextMode(options->text_mode) : pistoris::NativeTextMode::kAuto};
    auto baked = ambiance->value.bakeNativeBundle(cpp_options);
    if (!baked) return pistoris::c_api::publish(baked, error);
    pistoris::NativeAmbianceBundle bundle = std::move(*baked);
    auto native = std::make_unique<ArxAmb>();
    native->value = std::move(bundle.amb);
    std::unique_ptr<ArxSoundFiles> sounds;
    if (out_sounds) {
      sounds = std::make_unique<ArxSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
    }
    *out_native = native.release();
    if (out_sounds) *out_sounds = sounds.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_ambiance_validate(const ArxAmbiance* ambiance, ArxError* error) noexcept {
  if (!ambiance) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(ambiance->value.validate(), error);
}

// NOLINTEND(readability-identifier-naming)
