// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.h"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.h"

#include "api/c/animation/internal.h"
#include "api/c/internal.h"
#include "api/c/native/internal.h"  // IWYU pragma: keep
#include "api/c/native/text_internal.h"
#include "api/c/sound/internal.h"

#include <memory>
#include <utility>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_animation_create(ArxAnimation** out_animation, ArxError* error) noexcept {
  if (!out_animation) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_animation = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAnimation>();
    *out_animation = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_animation_clone(const ArxAnimation* animation, ArxAnimation** out_animation,
                                           ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_animation) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_animation = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAnimation>();
    result->value = animation->value;
    *out_animation = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

void arx_pistoris_animation_destroy(ArxAnimation* animation) noexcept { delete animation; }

ArxReturnCode arx_pistoris_animation_reset(ArxAnimation* animation, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.reset(), error);
}

ArxReturnCode arx_pistoris_animation_import_native(const ArxTea* native, ArxAnimation** out_animation,
                                                   ArxSoundSourceReferences** out_sound_sources,
                                                   ArxNativeTextMode text_mode, ArxError* error) noexcept {
  if (!native) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_animation) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_animation = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxAnimation>();
    std::unique_ptr<ArxSoundSourceReferences> sources;
    if (out_sound_sources) sources = std::make_unique<ArxSoundSourceReferences>();
    auto imported = pistoris::Animation::importNative(
        native->value, sources ? &sources->value : nullptr, pistoris::c_api::nativeTextMode(text_mode));
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_animation = result.release();
    if (out_sound_sources) *out_sound_sources = sources.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_animation_bake_native(const ArxAnimation* animation,
                                                 const ArxNativeAnimationBakeOptions* options, ArxTea** out_native,
                                                 ArxSoundFiles** out_sounds, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_native) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_native = nullptr;
  if (out_sounds) *out_sounds = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    if (options && !pistoris::c_api::validNativeTextMode(options->text_mode))
      return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
    const pistoris::NativeAnimationBakeOptions cpp_options{
        .include_sound_files = out_sounds && (!options || options->include_sound_files != 0),
        .text_mode = options ? pistoris::c_api::nativeTextMode(options->text_mode) : pistoris::NativeTextMode::kAuto};
    auto baked = animation->value.bakeNativeBundle(cpp_options);
    if (!baked) return pistoris::c_api::publish(baked, error);
    pistoris::NativeAnimationBundle bundle = std::move(*baked);
    auto native = std::make_unique<ArxTea>();
    native->value = std::move(bundle.tea);
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

ArxReturnCode arx_pistoris_animation_validate(const ArxAnimation* animation, ArxError* error) noexcept {
  if (!animation) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(animation->value.validate(), error);
}

// NOLINTEND(readability-identifier-naming)
