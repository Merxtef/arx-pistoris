// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.h"

#include "api/c/animation/internal.h"
#include "api/c/internal.h"
#include "api/c/native/internal.h"  // IWYU pragma: keep
#include "api/c/sound/internal.h"

#include <memory>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_create(ArxAnimation** out_animation) noexcept {
  if (!out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAnimation>();
    *out_animation = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_animation_clone(const ArxAnimation* animation, ArxAnimation** out_animation) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAnimation>();
    result->value = animation->value;
    *out_animation = result.release();
    return ARX_OK;
  });
}

void arx_pistoris_animation_destroy(ArxAnimation* animation) noexcept { delete animation; }

ArxReturnCode arx_pistoris_animation_reset(ArxAnimation* animation) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    animation->value.reset();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_animation_import_native(const ArxTea* native, ArxAnimation** out_animation,
                                                   ArxSoundSourceReferences** out_sound_sources) noexcept {
  if (!native) return ARX_INVALID_HANDLE;
  if (!out_animation) return ARX_INVALID_DATA_POINTER;
  *out_animation = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAnimation>();
    std::unique_ptr<ArxSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxSoundSourceReferences>();
    const ArxReturnCode rc =
        pistoris::Animation::importNative(result->value, native->value, sources ? &sources->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_animation = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_animation_bake_native(const ArxAnimation* animation,
                                                 const ArxNativeSoundBakeOptions* options, ArxTea** out_native,
                                                 ArxSoundFiles** out_sounds) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  if (!out_native) return ARX_INVALID_DATA_POINTER;
  *out_native = nullptr;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    const pistoris::NativeSoundBakeOptions cpp_options{.include_files =
                                                           out_sounds && (!options || options->include_files != 0)};
    pistoris::NativeAnimationBundle bundle;
    const ArxReturnCode rc = animation->value.bakeNativeBundle(cpp_options, bundle);
    if (rc != ARX_OK) return rc;
    auto native = std::make_unique<ArxTea>();
    native->value = std::move(bundle.tea);
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

ArxReturnCode arx_pistoris_animation_validate(const ArxAnimation* animation) noexcept {
  if (!animation) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return animation->value.validate(); });
}

// NOLINTEND(readability-identifier-naming)
