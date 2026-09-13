// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/sound.h"

#include "api/c/ambiance/internal.h"
#include "api/c/internal.h"
#include "api/c/native/internal.h"
#include "api/c/sound/internal.h"

#include <memory>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_ambiance_create(ArxAmbiance** out_ambiance) noexcept {
  if (!out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    *out_ambiance = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ambiance_clone(const ArxAmbiance* ambiance, ArxAmbiance** out_ambiance) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    result->value = ambiance->value;
    *out_ambiance = result.release();
    return ARX_OK;
  });
}

void arx_pistoris_ambiance_destroy(ArxAmbiance* ambiance) noexcept { delete ambiance; }

ArxReturnCode arx_pistoris_ambiance_reset(ArxAmbiance* ambiance) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    ambiance->value.reset();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ambiance_import_native(const ArxAmb* native, ArxAmbiance** out_ambiance,
                                                  ArxSoundSourceReferences** out_sound_sources) noexcept {
  if (!native) return ARX_INVALID_HANDLE;
  if (!out_ambiance) return ARX_INVALID_DATA_POINTER;
  *out_ambiance = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAmbiance>();
    std::unique_ptr<ArxSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxSoundSourceReferences>();
    const ArxReturnCode rc =
        pistoris::Ambiance::importNative(result->value, native->value, sources ? &sources->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_ambiance = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ambiance_bake_native(const ArxAmbiance* ambiance, const ArxNativeSoundBakeOptions* options,
                                                ArxAmb** out_native, ArxSoundFiles** out_sounds) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  if (!out_native) return ARX_INVALID_DATA_POINTER;
  *out_native = nullptr;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    const pistoris::NativeSoundBakeOptions cpp_options{.include_files =
                                                           out_sounds && (!options || options->include_files != 0)};
    pistoris::NativeAmbianceBundle bundle;
    const ArxReturnCode rc = ambiance->value.bakeNativeBundle(cpp_options, bundle);
    if (rc != ARX_OK) return rc;
    auto native = std::make_unique<ArxAmb>();
    native->value = std::move(bundle.amb);
    std::unique_ptr<ArxSoundFiles> sounds;
    if (out_sounds) {
      sounds = std::make_unique<ArxSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
    }
    *out_native = native.release();
    if (out_sounds) *out_sounds = sounds.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ambiance_validate(const ArxAmbiance* ambiance) noexcept {
  if (!ambiance) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return ambiance->value.validate(); });
}

// NOLINTEND(readability-identifier-naming)
