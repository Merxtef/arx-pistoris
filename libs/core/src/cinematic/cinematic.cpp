// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/cinematic.hpp"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "cinematic/data.h"
#include "cinematic/internal.h"
#include "modules/cinematic.h"
#include "modules/resource.h"
#include "modules/sounds.h"
#include "modules/textures.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/resource_path.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

bool validSoundKind(SoundKind kind) noexcept { return kind == SoundKind::kEffect || kind == SoundKind::kSpeech; }

ArxReturnCode resourceError(resource::Error error) noexcept {
  switch (error) {
    case resource::Error::kNone:
      return ARX_OK;
    case resource::Error::kBadPath:
      return ARX_CINEMATIC_BAD_RESOURCE_PATH;
    case resource::Error::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

ArxStringView borrowedString(std::string_view value) noexcept {
  return {value.data(), value.size()};  // NOLINT(bugprone-suspicious-stringview-data-usage)
}

ArxEncodedImageView borrowedImage(const std::vector<std::uint8_t>& value) noexcept {
  return {value.data(), value.size()};
}

ArxEncodedAudioView borrowedAudio(const std::vector<std::uint8_t>& value) noexcept {
  return {value.data(), value.size()};
}

bool copyString(ArxStringView value, std::string& out) {
  if (!value.data && value.size != 0) return false;
  out.assign(value.data ? value.data : "", value.size);
  return true;
}

bool copyImage(ArxEncodedImageView value, std::vector<std::uint8_t>& out) {
  if (!value.data && value.size != 0) return false;
  if (value.size != 0) out.assign(value.data, value.data + value.size);
  return true;
}

bool copyAudio(ArxEncodedAudioView value, std::vector<std::uint8_t>& out) {
  if (!value.data && value.size != 0) return false;
  if (value.size != 0) out.assign(value.data, value.data + value.size);
  return true;
}

bool internalTexture(const ArxTextureView& source, Texture& out) {
  return copyString(source.path, out.path) && copyImage(source.encoded_image, out.encoded_image) &&
         copyString(source.external_image_extension, out.external_image_extension);
}

bool languageNameUsed(const SoundsData& sounds, std::string_view name, LanguageId ignored) noexcept {
  return std::ranges::any_of(sounds::languages(sounds), [=](const SoundLanguage& language) {
    return language.id != ignored && ResourcePathIdentityEqual{}(language.name, name);
  });
}

bool validLanguageName(std::string_view name) noexcept {
  return isIdentifier(name) && isPortableResourcePathComponent(name);
}

ArxReturnCode validateEncodingTarget(const SoundsData& sounds, SoundHandle sound, LanguageId language) noexcept {
  if (!sounds::validHandle(sounds, sound)) return ARX_INDEX_OUT_OF_RANGE;
  SoundKind kind = SoundKind::kEffect;
  if (soundHandleKind(sound, kind) != ARX_OK) return ARX_INDEX_OUT_OF_RANGE;
  if (kind == SoundKind::kEffect) return language == kSoundEffects ? ARX_OK : ARX_CINEMATIC_BAD_LANGUAGE;
  if (language == kSoundEffects || sounds::findLanguage(sounds, language) == nullptr) return ARX_CINEMATIC_BAD_LANGUAGE;
  return ARX_OK;
}

}  // namespace

namespace cinematic_detail {

ArxReturnCode errorCode(cinematic::Error error) noexcept {
  switch (error) {
    case cinematic::Error::kNone:
      return ARX_OK;
    case cinematic::Error::kNoIllustrations:
      return ARX_CINEMATIC_NO_ILLUSTRATIONS;
    case cinematic::Error::kTooManyIllustrations:
      return ARX_CINEMATIC_TOO_MANY_ILLUSTRATIONS;
    case cinematic::Error::kBadIllustration:
      return ARX_CINEMATIC_BAD_ILLUSTRATION;
    case cinematic::Error::kBadIllustrationScale:
      return ARX_CINEMATIC_BAD_ILLUSTRATION_SCALE;
    case cinematic::Error::kBadTimeline:
      return ARX_CINEMATIC_BAD_TIMELINE;
    case cinematic::Error::kBadKeyCount:
      return ARX_CINEMATIC_BAD_KEY_COUNT;
    case cinematic::Error::kBadKeyFrame:
      return ARX_CINEMATIC_BAD_KEY_FRAME;
    case cinematic::Error::kBadKeyIllustration:
      return ARX_CINEMATIC_BAD_KEY_ILLUSTRATION;
    case cinematic::Error::kBadKeySound:
      return ARX_CINEMATIC_BAD_KEY_SOUND;
    case cinematic::Error::kBadKeyTransform:
      return ARX_CINEMATIC_BAD_KEY_TRANSFORM;
    case cinematic::Error::kBadKeyTiming:
      return ARX_CINEMATIC_BAD_KEY_TIMING;
    case cinematic::Error::kBadKeyInterpolation:
      return ARX_CINEMATIC_BAD_KEY_INTERPOLATION;
    case cinematic::Error::kBadKeyEffect:
      return ARX_CINEMATIC_BAD_KEY_EFFECT;
    case cinematic::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode textureError(textures::Error error) noexcept {
  switch (error) {
    case textures::Error::kNone:
      return ARX_OK;
    case textures::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case textures::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case textures::Error::kTooManyTextures:
      return ARX_CINEMATIC_TOO_MANY_TEXTURES;
    case textures::Error::kBadTexture:
      return ARX_CINEMATIC_BAD_TEXTURE_PATH;
    case textures::Error::kDuplicateTexture:
      return ARX_CINEMATIC_DUPLICATE_TEXTURE_PATH;
    case textures::Error::kBadImage:
      return ARX_CINEMATIC_BAD_TEXTURE_IMAGE;
    case textures::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode soundError(sounds::Error error) noexcept {
  switch (error) {
    case sounds::Error::kNone:
      return ARX_OK;
    case sounds::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case sounds::Error::kTooManySounds:
      return ARX_CINEMATIC_TOO_MANY_SOUNDS;
    case sounds::Error::kBadPath:
      return ARX_CINEMATIC_BAD_SOUND_PATH;
    case sounds::Error::kBadAudio:
      return ARX_CINEMATIC_BAD_SOUND_DATA;
    case sounds::Error::kUnsupportedChannels:
      return ARX_CINEMATIC_UNSUPPORTED_SOUND_CHANNELS;
    case sounds::Error::kAudioTooLarge:
      return ARX_CINEMATIC_SOUND_TOO_LARGE;
    case sounds::Error::kDuplicatePath:
      return ARX_CINEMATIC_DUPLICATE_SOUND_PATH;
    case sounds::Error::kBadKind:
      return ARX_INVALID_OPTIONS;
    case sounds::Error::kBadLanguage:
      return ARX_CINEMATIC_BAD_LANGUAGE;
    case sounds::Error::kDuplicateEncoding:
      return ARX_CINEMATIC_BAD_SOUND_ENCODING;
    case sounds::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case sounds::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode validateStructure(const CinematicModules& modules) noexcept {
  ArxReturnCode rc = resourceError(resource::validate(modules.resource, ARX_RESOURCE_KIND_CINEMATIC));
  if (rc != ARX_OK) return rc;
  rc = textureError(textures::validate(modules.textures.textures));
  if (rc != ARX_OK) return rc;
  rc = soundError(sounds::validateStructure(modules.sounds));
  if (rc != ARX_OK) return rc;
  return errorCode(cinematic::validate(modules.cinematic, modules.textures.textures.size(), modules.sounds));
}

ArxReturnCode internalKeyframe(const ArxCinematicKeyframe& source, CinematicKeyframe& out) noexcept {
  CinematicInterpolation interpolation;
  switch (source.interpolation) {
    case ARX_CINEMATIC_INTERPOLATION_NONE:
      interpolation = CinematicInterpolation::kNone;
      break;
    case ARX_CINEMATIC_INTERPOLATION_BEZIER:
      interpolation = CinematicInterpolation::kBezier;
      break;
    case ARX_CINEMATIC_INTERPOLATION_LINEAR:
      interpolation = CinematicInterpolation::kLinear;
      break;
    default:
      return ARX_CINEMATIC_BAD_KEY_INTERPOLATION;
  }

  CinematicBaseEffect base_effect;
  switch (source.base_effect) {
    case ARX_CINEMATIC_BASE_EFFECT_NONE:
      base_effect = CinematicBaseEffect::kNone;
      break;
    case ARX_CINEMATIC_BASE_EFFECT_FADE_IN:
      base_effect = CinematicBaseEffect::kFadeIn;
      break;
    case ARX_CINEMATIC_BASE_EFFECT_FADE_OUT:
      base_effect = CinematicBaseEffect::kFadeOut;
      break;
    case ARX_CINEMATIC_BASE_EFFECT_BLUR:
      base_effect = CinematicBaseEffect::kBlur;
      break;
    default:
      return ARX_CINEMATIC_BAD_KEY_EFFECT;
  }

  CinematicPostEffect post_effect;
  switch (source.post_effect) {
    case ARX_CINEMATIC_POST_EFFECT_NONE:
      post_effect = CinematicPostEffect::kNone;
      break;
    case ARX_CINEMATIC_POST_EFFECT_FLASH:
      post_effect = CinematicPostEffect::kFlash;
      break;
    case ARX_CINEMATIC_POST_EFFECT_SUPPRESS_FLASH:
      post_effect = CinematicPostEffect::kSuppressFlash;
      break;
    default:
      return ARX_CINEMATIC_BAD_KEY_EFFECT;
  }
  if (source.crossfade > 1 || source.dream > 1 || source.light_active > 1) return ARX_CINEMATIC_BAD_KEY_EFFECT;

  out = {
      .frame = source.frame,
      .illustration = source.illustration,
      .camera_position = source.camera_position,
      .camera_roll = source.camera_roll,
      .color = source.color,
      .secondary_color = source.secondary_color,
      .flash_color = source.flash_color,
      .flash_decay = source.flash_decay,
      .light = {source.light.position,
                source.light.fall_in,
                source.light.fall_out,
                source.light.color,
                source.light.intensity,
                source.light.random_intensity},
      .outgoing_speed = source.outgoing_speed,
      .sound = source.sound,
      .interpolation = interpolation,
      .base_effect = base_effect,
      .post_effect = post_effect,
      .crossfade = source.crossfade != 0,
      .dream = source.dream != 0,
      .light_active = source.light_active != 0,
  };
  return ARX_OK;
}

ArxCinematicKeyframe publicKeyframe(const CinematicKeyframe& source) noexcept {
  ArxCinematicKeyframe result;
  result.frame = source.frame;
  result.illustration = source.illustration;
  result.camera_position = source.camera_position;
  result.camera_roll = source.camera_roll;
  result.color = source.color;
  result.secondary_color = source.secondary_color;
  result.flash_color = source.flash_color;
  result.flash_decay = source.flash_decay;
  result.light = {source.light.position,
                  source.light.fall_in,
                  source.light.fall_out,
                  source.light.color,
                  source.light.intensity,
                  source.light.random_intensity};
  result.outgoing_speed = source.outgoing_speed;
  result.sound = source.sound;
  result.interpolation = static_cast<ArxCinematicInterpolation>(source.interpolation);
  result.base_effect = static_cast<ArxCinematicBaseEffect>(source.base_effect);
  result.post_effect = static_cast<ArxCinematicPostEffect>(source.post_effect);
  result.crossfade = source.crossfade ? 1U : 0U;
  result.dream = source.dream ? 1U : 0U;
  result.light_active = source.light_active ? 1U : 0U;
  return result;
}

}  // namespace cinematic_detail

Cinematic::Cinematic() : data_(std::make_unique<Data>()) {}

Cinematic::~Cinematic() = default;

Cinematic::Cinematic(const Cinematic& other) : data_(other.data_ ? std::make_unique<Data>(*other.data_) : nullptr) {}

Cinematic::Cinematic(Cinematic&& other) noexcept = default;

Cinematic& Cinematic::operator=(const Cinematic& other) {
  if (this == &other) return *this;
  Cinematic copy(other);
  swap(copy);
  return *this;
}

Cinematic& Cinematic::operator=(Cinematic&& other) noexcept = default;

void Cinematic::swap(Cinematic& other) noexcept { data_.swap(other.data_); }

CinematicResult<void> Cinematic::reset() noexcept {
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<void> {
    auto replacement = std::make_unique<Data>();
    data_.swap(replacement);
    return CinematicResult<void>::success();
  });
}

CinematicResult<void> Cinematic::validate() const noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::resourceValidationBoundary<CinematicResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const ArxReturnCode rc = cinematic_detail::validateStructure(static_cast<const CinematicModules&>(*data_));
        if (rc != ARX_OK) return rc;
        return cinematic_detail::soundError(sounds::validateAudio(data_->sounds));
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
}

std::string_view Cinematic::resourcePath() const noexcept {
  return data_ ? std::string_view(data_->resource.path) : std::string_view{};
}

CinematicResult<void> Cinematic::setResourcePath(std::string_view resource_path) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::string path;
        const ArxReturnCode rc = resourceError(resource::repairPath(ARX_RESOURCE_KIND_CINEMATIC, resource_path, path));
        if (rc != ARX_OK) return rc;
        resource::setPath(data_->resource, std::move(path));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
}

std::int32_t Cinematic::endFrame() const noexcept { return data_ ? data_->cinematic.end_frame : 0; }

float Cinematic::fps() const noexcept { return data_ ? data_->cinematic.fps : 0.0f; }

std::size_t Cinematic::illustrationCount() const noexcept { return data_ ? data_->cinematic.illustrations.size() : 0; }

std::size_t Cinematic::keyframeCount() const noexcept { return data_ ? data_->cinematic.keyframes.size() : 0; }

std::size_t Cinematic::textureCount() const noexcept { return data_ ? data_->textures.textures.size() : 0; }

std::size_t Cinematic::soundCount(SoundKind kind) const noexcept {
  return data_ && validSoundKind(kind) ? sounds::count(data_->sounds, kind) : 0;
}

std::size_t Cinematic::languageCount() const noexcept { return data_ ? sounds::languages(data_->sounds).size() : 0; }

std::size_t Cinematic::soundEncodingCount() const noexcept { return data_ ? data_->sounds.encodings.size() : 0; }

ArxCinematicIllustration Cinematic::illustrationAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Cinematic*>(owner);
  const CinematicIllustration& source = self.data_->cinematic.illustrations[index];
  return {source.texture, source.subdivision_scale};
}

ArxCinematicKeyframe Cinematic::keyframeAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Cinematic*>(owner);
  return cinematic_detail::publicKeyframe(self.data_->cinematic.keyframes[index]);
}

ArxTextureView Cinematic::textureAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Cinematic*>(owner);
  const Texture& source = self.data_->textures.textures[index];
  return {borrowedString(source.path),
          borrowedImage(source.encoded_image),
          borrowedString(source.external_image_extension)};
}

ArxCinematicSoundView Cinematic::soundAt(const void* owner, std::size_t kind_value, std::size_t index) noexcept {
  const auto& self = *static_cast<const Cinematic*>(owner);
  const auto kind = static_cast<SoundKind>(kind_value);
  SoundHandle handle = kNoSoundHandle;
  const ArxReturnCode rc = soundHandle(kind, static_cast<SoundIndex>(index), handle);
  assert(rc == ARX_OK);
  (void)rc;
  return {handle, borrowedString(sounds::path(self.data_->sounds, handle))};
}

ArxCinematicLanguageView Cinematic::languageAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Cinematic*>(owner);
  const SoundLanguage& language = sounds::languages(self.data_->sounds)[index];
  return {language.id, borrowedString(language.name)};
}

ArxCinematicSoundEncodingView Cinematic::soundEncodingAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Cinematic*>(owner);
  const SoundEncoding& source = self.data_->sounds.encodings[index];
  return {source.sound, source.language, borrowedAudio(source.encoded_audio)};
}

Cinematic::IllustrationsView Cinematic::illustrations() const noexcept {
  return data_ ? IllustrationsView(this, 0, illustrationCount(), &Cinematic::illustrationAt) : IllustrationsView{};
}

Cinematic::KeyframesView Cinematic::keyframes() const noexcept {
  return data_ ? KeyframesView(this, 0, keyframeCount(), &Cinematic::keyframeAt) : KeyframesView{};
}

Cinematic::TexturesView Cinematic::textures() const noexcept {
  return data_ ? TexturesView(this, 0, textureCount(), &Cinematic::textureAt) : TexturesView{};
}

Cinematic::SoundsView Cinematic::sounds(SoundKind kind) const noexcept {
  return data_ && validSoundKind(kind)
             ? SoundsView(this, static_cast<std::size_t>(kind), soundCount(kind), &Cinematic::soundAt)
             : SoundsView{};
}

Cinematic::LanguagesView Cinematic::languages() const noexcept {
  return data_ ? LanguagesView(this, 0, languageCount(), &Cinematic::languageAt) : LanguagesView{};
}

std::optional<LanguageId> Cinematic::findLanguage(std::string_view name) const noexcept {
  if (!data_) return std::nullopt;
  for (const SoundLanguage& language : sounds::languages(data_->sounds)) {
    if (ResourcePathIdentityEqual{}(language.name, name)) return language.id;
  }
  return std::nullopt;
}

Cinematic::SoundEncodingsView Cinematic::soundEncodings() const noexcept {
  return data_ ? SoundEncodingsView(this, 0, soundEncodingCount(), &Cinematic::soundEncodingAt) : SoundEncodingsView{};
}

CinematicResult<void> Cinematic::setTimeline(std::int32_t end_frame, float fps_value) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  if (end_frame <= 0 || !math::finite(fps_value) || fps_value <= 0.0f)
    return api_detail::cinematicStatus<void>(ARX_CINEMATIC_BAD_TIMELINE,
                                             api_detail::resourceLocation(resourcePath(), CinematicElement::kTimeline));
  data_->cinematic.end_frame = end_frame;
  data_->cinematic.fps = fps_value;
  return CinematicResult<void>::success();
}

CinematicResult<void> Cinematic::setKeyframe(std::size_t index, const ArxCinematicKeyframe& source) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= keyframeCount()) return ARX_INDEX_OUT_OF_RANGE;
        CinematicKeyframe key;
        ArxReturnCode rc = cinematic_detail::internalKeyframe(source, key);
        if (rc != ARX_OK) return rc;
        rc = cinematic_detail::errorCode(
            cinematic::validateKeyframe(key, illustrationCount(), data_->sounds, index + 1U == keyframeCount()));
        if (rc != ARX_OK) return rc;
        if ((index != 0 && data_->cinematic.keyframes[index - 1U].frame >= key.frame) ||
            (index + 1U < keyframeCount() && data_->cinematic.keyframes[index + 1U].frame <= key.frame))
          return ARX_CINEMATIC_BAD_KEY_FRAME;
        cinematic::setKeyframe(data_->cinematic, index, key);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kKeyframe, index));
}

CinematicResult<std::size_t> Cinematic::addKeyframe(const ArxCinematicKeyframe& source) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<std::size_t> {
    const CinematicLocation location =
        api_detail::resourceLocation(resourcePath(), CinematicElement::kKeyframe, data_->cinematic.keyframes.size());
    if (keyframeCount() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
      return api_detail::cinematicFailure<std::size_t>(ARX_CINEMATIC_BAD_KEY_COUNT, location);
    CinematicKeyframe key;
    ArxReturnCode rc = cinematic_detail::internalKeyframe(source, key);
    if (rc != ARX_OK) return api_detail::cinematicFailure<std::size_t>(rc, location);
    auto position = std::ranges::lower_bound(data_->cinematic.keyframes, key.frame, {}, &CinematicKeyframe::frame);
    if (position != data_->cinematic.keyframes.end() && position->frame == key.frame)
      return api_detail::cinematicFailure<std::size_t>(ARX_CINEMATIC_BAD_KEY_FRAME, location);
    rc = cinematic_detail::errorCode(cinematic::validateKeyframe(
        key, illustrationCount(), data_->sounds, position == data_->cinematic.keyframes.end()));
    if (rc != ARX_OK) return api_detail::cinematicFailure<std::size_t>(rc, location);
    if (position == data_->cinematic.keyframes.end() && !data_->cinematic.keyframes.empty() &&
        data_->cinematic.keyframes.back().outgoing_speed <= 0.0f)
      return api_detail::cinematicFailure<std::size_t>(ARX_CINEMATIC_BAD_KEY_TIMING, location);
    const std::size_t index = static_cast<std::size_t>(position - data_->cinematic.keyframes.begin());
    cinematic::insertKeyframe(data_->cinematic, index, key);
    return CinematicResult<std::size_t>::success(index);
  });
}

CinematicResult<void> Cinematic::removeKeyframe(std::size_t index) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  if (index >= keyframeCount())
    return api_detail::cinematicStatus<void>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), CinematicElement::kKeyframe, index));
  cinematic::removeKeyframe(data_->cinematic, index);
  return CinematicResult<void>::success();
}

void Cinematic::clearKeyframes() noexcept {
  if (data_) cinematic::clearKeyframes(data_->cinematic);
}

CinematicResult<void> Cinematic::setIllustration(CinematicIllustrationIndex index,
                                                 ArxCinematicIllustration value) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  const CinematicLocation location =
      api_detail::resourceLocation(resourcePath(), CinematicElement::kIllustration, index);
  if (static_cast<std::size_t>(index) >= illustrationCount())
    return api_detail::cinematicStatus<void>(ARX_INDEX_OUT_OF_RANGE, location);
  const CinematicIllustration illustration{value.texture, value.subdivision_scale};
  const ArxReturnCode rc = cinematic_detail::errorCode(cinematic::validateIllustration(illustration, textureCount()));
  if (rc != ARX_OK) return api_detail::cinematicStatus<void>(rc, location);
  cinematic::setIllustration(data_->cinematic, index, illustration);
  return CinematicResult<void>::success();
}

CinematicResult<CinematicIllustrationIndex> Cinematic::addIllustration(ArxCinematicIllustration value) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<CinematicIllustrationIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<CinematicIllustrationIndex> {
    const CinematicLocation location =
        api_detail::resourceLocation(resourcePath(), CinematicElement::kIllustration, illustrationCount());
    ArxReturnCode rc = cinematic_detail::errorCode(cinematic::validateIllustrationCount(illustrationCount() + 1U));
    if (rc != ARX_OK) return api_detail::cinematicFailure<CinematicIllustrationIndex>(rc, location);
    const CinematicIllustration illustration{value.texture, value.subdivision_scale};
    rc = cinematic_detail::errorCode(cinematic::validateIllustration(illustration, textureCount()));
    if (rc != ARX_OK) return api_detail::cinematicFailure<CinematicIllustrationIndex>(rc, location);
    return CinematicResult<CinematicIllustrationIndex>::success(
        cinematic::addIllustration(data_->cinematic, illustration));
  });
}

CinematicResult<void> Cinematic::removeIllustration(CinematicIllustrationIndex index) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  const CinematicLocation location =
      api_detail::resourceLocation(resourcePath(), CinematicElement::kIllustration, index);
  if (static_cast<std::size_t>(index) >= illustrationCount())
    return api_detail::cinematicStatus<void>(ARX_INDEX_OUT_OF_RANGE, location);
  if (std::ranges::any_of(data_->cinematic.keyframes,
                          [=](const CinematicKeyframe& key) { return key.illustration == index; }))
    return api_detail::cinematicStatus<void>(ARX_CINEMATIC_ILLUSTRATION_IN_USE, location);
  cinematic::removeIllustration(data_->cinematic, index);
  return CinematicResult<void>::success();
}

void Cinematic::clearIllustrations() noexcept {
  if (data_) cinematic::clearIllustrations(data_->cinematic);
}

CinematicResult<std::size_t> Cinematic::compactIllustrations() noexcept {
  if (!data_)
    return api_detail::cinematicFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<std::size_t> {
    std::vector<std::uint8_t> used(textureCount(), 0);
    for (std::size_t i = 0; i < data_->cinematic.illustrations.size(); ++i) {
      const CinematicIllustration& illustration = data_->cinematic.illustrations[i];
      if (static_cast<std::size_t>(illustration.texture) >= used.size())
        return api_detail::cinematicFailure<std::size_t>(
            ARX_CINEMATIC_BAD_ILLUSTRATION,
            api_detail::resourceLocation(resourcePath(), CinematicElement::kIllustration, i));
      used[illustration.texture] = 1;
    }
    std::vector<TextureIndex> remap;
    std::size_t count = 0;
    const ArxReturnCode rc = cinematic_detail::textureError(textures::compact(data_->textures, used, remap, count));
    if (rc != ARX_OK)
      return api_detail::cinematicFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
    for (std::size_t i = 0; i < data_->cinematic.illustrations.size(); ++i) {
      CinematicIllustration& illustration = data_->cinematic.illustrations[i];
      if (illustration.texture >= remap.size() || remap[illustration.texture] == kNoTexture)
        return api_detail::cinematicFailure<std::size_t>(
            ARX_INTERNAL_ERROR, api_detail::resourceLocation(resourcePath(), CinematicElement::kIllustration, i));
      illustration.texture = remap[illustration.texture];
    }
    return CinematicResult<std::size_t>::success(count);
  });
}

CinematicResult<void> Cinematic::rebaseTexturePaths(std::string_view directory) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        textures::PathRebaseInfo info;
        const ArxReturnCode rc =
            cinematic_detail::textureError(textures::rebasePaths(data_->textures, directory, &info));
        if (rc != ARX_OK) return rc;
        for (const textures::PathRebaseInfo::Repair& repair : info.repairs)
          log(ARX_LOG_WARN, "Cinematic texture rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
}

CinematicResult<void> Cinematic::setTexture(TextureIndex index, const ArxTextureView& value) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
        Texture texture;
        if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
        textures::PathRepairInfo repair;
        ArxReturnCode rc =
            cinematic_detail::textureError(textures::repairPath(data_->textures, texture, index, &repair));
        if (rc != ARX_OK) return rc;
        rc = cinematic_detail::textureError(textures::validateTexture(texture));
        if (rc != ARX_OK) return rc;
        textures::setTexture(data_->textures, index, std::move(texture));
        for (const textures::PathRepairInfo::Repair& item : repair.repairs)
          log(ARX_LOG_WARN, "Cinematic texture: '{}' normalized to '{}'", item.original, item.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kTexture, index));
}

CinematicResult<TextureIndex> Cinematic::addTexture(const ArxTextureView& value) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<TextureIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<TextureIndex> {
    const CinematicLocation location =
        api_detail::resourceLocation(resourcePath(), CinematicElement::kTexture, textureCount());
    Texture texture;
    if (!internalTexture(value, texture))
      return api_detail::cinematicFailure<TextureIndex>(ARX_INVALID_DATA_POINTER, location);
    ArxReturnCode rc = cinematic_detail::textureError(textures::validateTextureCount(textureCount() + 1U));
    if (rc != ARX_OK) return api_detail::cinematicFailure<TextureIndex>(rc, location);
    textures::PathRepairInfo repair;
    rc = cinematic_detail::textureError(textures::repairPath(data_->textures, texture, kNoTexture, &repair));
    if (rc != ARX_OK) return api_detail::cinematicFailure<TextureIndex>(rc, location);
    rc = cinematic_detail::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return api_detail::cinematicFailure<TextureIndex>(rc, location);
    const TextureIndex index = textures::addTexture(data_->textures, std::move(texture));
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Cinematic texture: '{}' normalized to '{}'", item.original, item.repaired);
    return CinematicResult<TextureIndex>::success(index);
  });
}

CinematicResult<void> Cinematic::setTexturePath(TextureIndex index, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
        Texture candidate(requested);
        textures::PathRepairInfo repair;
        ArxReturnCode rc =
            cinematic_detail::textureError(textures::repairPath(data_->textures, candidate, index, &repair));
        if (rc != ARX_OK) return rc;
        rc = cinematic_detail::textureError(textures::validateTexture(candidate));
        if (rc != ARX_OK) return rc;
        textures::setPath(data_->textures, index, std::move(candidate.path));
        for (const textures::PathRepairInfo::Repair& item : repair.repairs)
          log(ARX_LOG_WARN, "Cinematic texture: '{}' normalized to '{}'", item.original, item.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kTexture, index));
}

CinematicResult<void> Cinematic::setTextureExternalImageExtension(TextureIndex index,
                                                                  std::string_view requested) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
        const Texture& current = data_->textures.textures[index];
        if (!current.encoded_image.empty() && !requested.empty()) return ARX_CINEMATIC_BAD_TEXTURE_IMAGE;
        Texture candidate(current.path);
        candidate.external_image_extension = requested;
        const ArxReturnCode rc = cinematic_detail::textureError(textures::validateTexture(candidate));
        if (rc != ARX_OK) return rc;
        textures::setExternalImageExtension(data_->textures, index, std::move(candidate.external_image_extension));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kTexture, index));
}

CinematicResult<void> Cinematic::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
        if (encoded_image.size == 0) return ARX_CINEMATIC_BAD_TEXTURE_IMAGE;
        std::vector<std::uint8_t> image;
        if (!copyImage(encoded_image, image)) return ARX_INVALID_DATA_POINTER;
        const ArxReturnCode rc = cinematic_detail::textureError(textures::validateEncodedImage(image));
        if (rc != ARX_OK) return rc;
        textures::setEncodedImage(data_->textures, index, std::move(image));
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kTexture, index));
}

CinematicResult<void> Cinematic::clearTextureImage(TextureIndex index) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  if (static_cast<std::size_t>(index) >= textureCount())
    return api_detail::cinematicStatus<void>(
        ARX_INDEX_OUT_OF_RANGE, api_detail::resourceLocation(resourcePath(), CinematicElement::kTexture, index));
  textures::clearEncodedImage(data_->textures, index);
  return CinematicResult<void>::success();
}

CinematicResult<std::size_t> Cinematic::compactSounds(SoundKind kind) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<std::size_t> {
    if (!validSoundKind(kind))
      return api_detail::cinematicFailure<std::size_t>(
          ARX_INVALID_OPTIONS, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
    std::vector<std::uint8_t> used(soundCount(kind), 0);
    for (std::size_t i = 0; i < data_->cinematic.keyframes.size(); ++i) {
      const CinematicKeyframe& key = data_->cinematic.keyframes[i];
      if (key.sound == kNoSoundHandle) continue;
      SoundKind key_kind = SoundKind::kEffect;
      SoundIndex index = kNoSound;
      if (soundHandleKind(key.sound, key_kind) != ARX_OK || soundHandleIndex(key.sound, index) != ARX_OK)
        return api_detail::cinematicFailure<std::size_t>(
            ARX_CINEMATIC_BAD_KEY_SOUND, api_detail::resourceLocation(resourcePath(), CinematicElement::kKeyframe, i));
      if (key_kind == kind) {
        if (index >= used.size())
          return api_detail::cinematicFailure<std::size_t>(
              ARX_CINEMATIC_BAD_KEY_SOUND,
              api_detail::resourceLocation(resourcePath(), CinematicElement::kKeyframe, i));
        used[index] = 1;
      }
    }
    std::vector<SoundIndex> remap;
    std::size_t count = 0;
    ArxReturnCode rc = cinematic_detail::soundError(sounds::compact(data_->sounds, kind, used, remap, count));
    if (rc != ARX_OK)
      return api_detail::cinematicFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
    for (std::size_t i = 0; i < data_->cinematic.keyframes.size(); ++i) {
      CinematicKeyframe& key = data_->cinematic.keyframes[i];
      const CinematicLocation location = api_detail::resourceLocation(resourcePath(), CinematicElement::kKeyframe, i);
      if (key.sound == kNoSoundHandle) continue;
      SoundKind key_kind = SoundKind::kEffect;
      SoundIndex index = kNoSound;
      if (soundHandleKind(key.sound, key_kind) != ARX_OK || soundHandleIndex(key.sound, index) != ARX_OK)
        return api_detail::cinematicFailure<std::size_t>(ARX_INTERNAL_ERROR, location);
      if (key_kind != kind) continue;
      if (index >= remap.size() || remap[index] == kNoSound)
        return api_detail::cinematicFailure<std::size_t>(ARX_INTERNAL_ERROR, location);
      if (soundHandle(kind, remap[index], key.sound) != ARX_OK)
        return api_detail::cinematicFailure<std::size_t>(ARX_INTERNAL_ERROR, location);
    }
    return CinematicResult<std::size_t>::success(count);
  });
}

CinematicResult<void> Cinematic::rebaseSoundPaths(SoundKind kind, std::string_view directory) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validSoundKind(kind)) return ARX_INVALID_OPTIONS;
        sounds::PathRebaseInfo info;
        const ArxReturnCode rc =
            cinematic_detail::soundError(sounds::rebasePaths(data_->sounds, kind, directory, &info));
        if (rc != ARX_OK) return rc;
        for (const sounds::PathRebaseInfo::Repair& repair : info.repairs)
          log(ARX_LOG_WARN, "Cinematic sound rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
}

CinematicResult<void> Cinematic::setSoundPath(SoundHandle sound, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        SoundKind kind = SoundKind::kEffect;
        SoundIndex index = kNoSound;
        if (!sounds::validHandle(data_->sounds, sound) || soundHandleKind(sound, kind) != ARX_OK ||
            soundHandleIndex(sound, index) != ARX_OK)
          return ARX_INDEX_OUT_OF_RANGE;
        std::string path(requested);
        sounds::PathRepairInfo repairs;
        const ArxReturnCode rc =
            cinematic_detail::soundError(sounds::repairPath(data_->sounds, kind, path, index, &repairs));
        if (rc != ARX_OK) return rc;
        sounds::setPath(data_->sounds, sound, std::move(path));
        for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
          log(ARX_LOG_WARN, "Cinematic sound: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      api_detail::cinematicSoundLocation(resourcePath(), sound));
}

CinematicResult<SoundHandle> Cinematic::addSound(SoundKind kind, std::string_view requested) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<SoundHandle>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<SoundHandle> {
    const CinematicLocation resource_location =
        api_detail::resourceLocation(resourcePath(), CinematicElement::kResource);
    if (!validSoundKind(kind)) return api_detail::cinematicFailure<SoundHandle>(ARX_INVALID_OPTIONS, resource_location);
    ArxReturnCode rc = cinematic_detail::soundError(sounds::validateSoundCount(soundCount(kind) + 1U));
    if (rc != ARX_OK) return api_detail::cinematicFailure<SoundHandle>(rc, resource_location);
    std::string path(requested);
    sounds::PathRepairInfo repairs;
    rc = cinematic_detail::soundError(sounds::repairPath(data_->sounds, kind, path, kNoSound, &repairs));
    if (rc != ARX_OK) return api_detail::cinematicFailure<SoundHandle>(rc, resource_location);
    if (!sounds::validPath(path))
      return api_detail::cinematicFailure<SoundHandle>(ARX_CINEMATIC_BAD_SOUND_PATH, resource_location);
    const SoundHandle sound = sounds::addPath(data_->sounds, kind, std::move(path));
    for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
      log(ARX_LOG_WARN, "Cinematic sound: '{}' normalized to '{}'", repair.original, repair.repaired);
    return CinematicResult<SoundHandle>::success(sound);
  });
}

CinematicResult<void> Cinematic::removeSound(SoundHandle sound) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  const CinematicLocation location = api_detail::cinematicSoundLocation(resourcePath(), sound);
  if (!sounds::validHandle(data_->sounds, sound))
    return api_detail::cinematicStatus<void>(ARX_INDEX_OUT_OF_RANGE, location);
  if (std::ranges::any_of(data_->cinematic.keyframes, [=](const CinematicKeyframe& key) { return key.sound == sound; }))
    return api_detail::cinematicStatus<void>(ARX_CINEMATIC_SOUND_IN_USE, location);
  SoundKind kind = SoundKind::kEffect;
  SoundIndex removed = kNoSound;
  if (soundHandleKind(sound, kind) != ARX_OK || soundHandleIndex(sound, removed) != ARX_OK)
    return api_detail::cinematicStatus<void>(ARX_INDEX_OUT_OF_RANGE, location);
  sounds::removeSound(data_->sounds, sound);
  for (CinematicKeyframe& key : data_->cinematic.keyframes) {
    if (key.sound == kNoSoundHandle) continue;
    SoundKind key_kind = SoundKind::kEffect;
    SoundIndex index = kNoSound;
    if (soundHandleKind(key.sound, key_kind) == ARX_OK && soundHandleIndex(key.sound, index) == ARX_OK &&
        key_kind == kind && index > removed)
      (void)soundHandle(kind, index - 1U, key.sound);
  }
  return CinematicResult<void>::success();
}

CinematicResult<void> Cinematic::setSoundData(SoundHandle sound, LanguageId language,
                                              ArxEncodedAudioView encoded_audio) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        ArxReturnCode rc = validateEncodingTarget(data_->sounds, sound, language);
        if (rc != ARX_OK) return rc;
        if (encoded_audio.size == 0) return ARX_CINEMATIC_BAD_SOUND_DATA;
        std::vector<std::uint8_t> data;
        if (!copyAudio(encoded_audio, data)) return ARX_INVALID_DATA_POINTER;
        rc = cinematic_detail::soundError(sounds::validateEncodedAudio(data));
        if (rc != ARX_OK) return rc;
        sounds::setEncodedAudio(data_->sounds, sound, language, std::move(data));
        return ARX_OK;
      },
      api_detail::cinematicSoundEncodingLocation(resourcePath(), sound, language));
}

CinematicResult<void> Cinematic::clearSoundData(SoundHandle sound, LanguageId language) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  const ArxReturnCode rc = validateEncodingTarget(data_->sounds, sound, language);
  if (rc != ARX_OK)
    return api_detail::cinematicStatus<void>(
        rc, api_detail::cinematicSoundEncodingLocation(resourcePath(), sound, language));
  sounds::clearEncodedAudio(data_->sounds, sound, language);
  return CinematicResult<void>::success();
}

CinematicResult<void> Cinematic::setLanguage(LanguageId language, std::string_view name) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (language == kSoundEffects || language == kInvalidLanguageId || !validLanguageName(name))
          return ARX_CINEMATIC_BAD_LANGUAGE;
        std::string canonical = normalizeIdentifier(name, {.letter_case = IdentifierCase::kLower}).value;
        if (languageNameUsed(data_->sounds, canonical, language)) return ARX_CINEMATIC_DUPLICATE_LANGUAGE;
        sounds::setLanguage(data_->sounds, language, std::move(canonical));
        return ARX_OK;
      },
      api_detail::cinematicLanguageLocation(resourcePath(), language));
}

CinematicResult<LanguageId> Cinematic::addLanguage(std::string_view name) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<LanguageId>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  return api_detail::cinematicBoundary(resourcePath(), [&]() -> CinematicResult<LanguageId> {
    const CinematicLocation location = api_detail::resourceLocation(resourcePath(), CinematicElement::kLanguage);
    if (!validLanguageName(name)) return api_detail::cinematicFailure<LanguageId>(ARX_CINEMATIC_BAD_LANGUAGE, location);
    std::string canonical = normalizeIdentifier(name, {.letter_case = IdentifierCase::kLower}).value;
    if (languageNameUsed(data_->sounds, canonical, kInvalidLanguageId))
      return api_detail::cinematicFailure<LanguageId>(ARX_CINEMATIC_DUPLICATE_LANGUAGE, location);
    const LanguageId next = sounds::firstAvailableLanguage(data_->sounds);
    if (next == kInvalidLanguageId)
      return api_detail::cinematicFailure<LanguageId>(ARX_CINEMATIC_BAD_LANGUAGE, location);
    sounds::setLanguage(data_->sounds, next, std::move(canonical));
    return CinematicResult<LanguageId>::success(next);
  });
}

CinematicResult<void> Cinematic::removeLanguage(LanguageId language) noexcept {
  if (!data_)
    return api_detail::cinematicFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), CinematicElement::kResource));
  if (language == kSoundEffects || sounds::findLanguage(data_->sounds, language) == nullptr)
    return api_detail::cinematicStatus<void>(ARX_INDEX_OUT_OF_RANGE,
                                             api_detail::cinematicLanguageLocation(resourcePath(), language));
  sounds::removeLanguage(data_->sounds, language);
  return CinematicResult<void>::success();
}

}  // namespace pistoris
