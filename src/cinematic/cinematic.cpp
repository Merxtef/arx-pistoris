// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/cinematic.hpp"

#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.h"

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
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

template <class T>
ArxReturnCode validateCopyRange(std::size_t size, std::size_t offset, std::size_t count, T* out) noexcept {
  if (offset > size || count > size - offset) return ARX_INDEX_OUT_OF_RANGE;
  if (count != 0 && out == nullptr) return ARX_INVALID_DATA_POINTER;
  return ARX_OK;
}

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
  return std::ranges::any_of(sounds.languages, [=](const auto& entry) {
    return entry.first != ignored && ResourcePathIdentityEqual{}(entry.second, name);
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
  if (language == kSoundEffects || !sounds.languages.contains(language)) return ARX_CINEMATIC_BAD_LANGUAGE;
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
      .color = {source.color.r, source.color.g, source.color.b},
      .secondary_color = {source.secondary_color.r, source.secondary_color.g, source.secondary_color.b},
      .flash_color = {source.flash_color.r, source.flash_color.g, source.flash_color.b},
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
  result.color = {source.color.r, source.color.g, source.color.b};
  result.secondary_color = {source.secondary_color.r, source.secondary_color.g, source.secondary_color.b};
  result.flash_color = {source.flash_color.r, source.flash_color.g, source.flash_color.b};
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

Cinematic::Cinematic(const Cinematic& other) : data_(std::make_unique<Data>(*other.data_)) {}

Cinematic& Cinematic::operator=(const Cinematic& other) {
  if (this == &other) return *this;
  Cinematic copy(other);
  swap(copy);
  return *this;
}

void Cinematic::swap(Cinematic& other) noexcept { data_.swap(other.data_); }

void Cinematic::reset() { data_ = std::make_unique<Data>(); }

ArxReturnCode Cinematic::validate() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = cinematic_detail::validateStructure(static_cast<const CinematicModules&>(*data_));
    if (rc != ARX_OK) return rc;
    return cinematic_detail::soundError(sounds::validateAudio(data_->sounds));
  });
}

std::string_view Cinematic::resourcePath() const noexcept { return data_->resource.path; }

ArxReturnCode Cinematic::setResourcePath(std::string_view resource_path) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::string path;
    const ArxReturnCode rc = resourceError(resource::repairPath(ARX_RESOURCE_KIND_CINEMATIC, resource_path, path));
    if (rc != ARX_OK) return rc;
    resource::setPath(data_->resource, std::move(path));
    return ARX_OK;
  });
}

std::int32_t Cinematic::endFrame() const noexcept { return data_->cinematic.end_frame; }

float Cinematic::fps() const noexcept { return data_->cinematic.fps; }

std::size_t Cinematic::illustrationCount() const noexcept { return data_->cinematic.illustrations.size(); }

std::size_t Cinematic::keyframeCount() const noexcept { return data_->cinematic.keyframes.size(); }

std::size_t Cinematic::textureCount() const noexcept { return data_->textures.textures.size(); }

std::size_t Cinematic::soundCount(SoundKind kind) const noexcept {
  return validSoundKind(kind) ? sounds::count(data_->sounds, kind) : 0;
}

std::size_t Cinematic::languageCount() const noexcept { return data_->sounds.languages.size(); }

std::size_t Cinematic::soundEncodingCount() const noexcept { return data_->sounds.encodings.size(); }

ArxReturnCode Cinematic::copyIllustrations(std::size_t offset, std::size_t count,
                                           ArxCinematicIllustration* out) const noexcept {
  const ArxReturnCode rc = validateCopyRange(illustrationCount(), offset, count, out);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    const CinematicIllustration& source = data_->cinematic.illustrations[offset + index];
    out[index] = {source.texture, source.subdivision_scale};
  }
  return ARX_OK;
}

ArxReturnCode Cinematic::copyKeyframes(std::size_t offset, std::size_t count,
                                       ArxCinematicKeyframe* out) const noexcept {
  const ArxReturnCode rc = validateCopyRange(keyframeCount(), offset, count, out);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index)
    out[index] = cinematic_detail::publicKeyframe(data_->cinematic.keyframes[offset + index]);
  return ARX_OK;
}

ArxReturnCode Cinematic::copyTextureViews(std::size_t offset, std::size_t count, ArxTextureView* out) const noexcept {
  const ArxReturnCode rc = validateCopyRange(textureCount(), offset, count, out);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    const Texture& source = data_->textures.textures[offset + index];
    out[index] = {borrowedString(source.path),
                  borrowedImage(source.encoded_image),
                  borrowedString(source.external_image_extension)};
  }
  return ARX_OK;
}

ArxReturnCode Cinematic::copySoundViews(SoundKind kind, std::size_t offset, std::size_t count,
                                        ArxCinematicSoundView* out) const noexcept {
  if (!validSoundKind(kind)) return ARX_INVALID_OPTIONS;
  const ArxReturnCode rc = validateCopyRange(soundCount(kind), offset, count, out);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    SoundHandle handle = kNoSoundHandle;
    if (soundHandle(kind, static_cast<SoundIndex>(offset + index), handle) != ARX_OK) return ARX_INTERNAL_ERROR;
    const std::string_view path = sounds::path(data_->sounds, handle);
    out[index] = {handle, borrowedString(path)};
  }
  return ARX_OK;
}

ArxReturnCode Cinematic::copyLanguages(std::size_t offset, std::size_t count,
                                       ArxCinematicLanguageView* out) const noexcept {
  const ArxReturnCode rc = validateCopyRange(languageCount(), offset, count, out);
  if (rc != ARX_OK) return rc;
  auto item = data_->sounds.languages.cbegin();
  std::advance(item, static_cast<std::ptrdiff_t>(offset));
  for (std::size_t index = 0; index < count; ++index, ++item) out[index] = {item->first, borrowedString(item->second)};
  return ARX_OK;
}

ArxReturnCode Cinematic::copySoundEncodings(std::size_t offset, std::size_t count,
                                            ArxCinematicSoundEncodingView* out) const noexcept {
  const ArxReturnCode rc = validateCopyRange(soundEncodingCount(), offset, count, out);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    const SoundEncoding& source = data_->sounds.encodings[offset + index];
    out[index] = {source.sound, source.language, borrowedAudio(source.encoded_audio)};
  }
  return ARX_OK;
}

ArxReturnCode Cinematic::setTimeline(std::int32_t end_frame, float fps_value) noexcept {
  if (end_frame <= 0 || !math::finite(fps_value) || fps_value <= 0.0f) return ARX_CINEMATIC_BAD_TIMELINE;
  data_->cinematic.end_frame = end_frame;
  data_->cinematic.fps = fps_value;
  return ARX_OK;
}

ArxReturnCode Cinematic::setKeyframe(std::size_t index, const ArxCinematicKeyframe& source) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

ArxReturnCode Cinematic::addKeyframe(const ArxCinematicKeyframe& source, std::size_t& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = std::numeric_limits<std::size_t>::max();
    if (keyframeCount() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
      return ARX_CINEMATIC_BAD_KEY_COUNT;
    CinematicKeyframe key;
    ArxReturnCode rc = cinematic_detail::internalKeyframe(source, key);
    if (rc != ARX_OK) return rc;
    auto position = std::ranges::lower_bound(data_->cinematic.keyframes, key.frame, {}, &CinematicKeyframe::frame);
    if (position != data_->cinematic.keyframes.end() && position->frame == key.frame)
      return ARX_CINEMATIC_BAD_KEY_FRAME;
    rc = cinematic_detail::errorCode(cinematic::validateKeyframe(
        key, illustrationCount(), data_->sounds, position == data_->cinematic.keyframes.end()));
    if (rc != ARX_OK) return rc;
    if (position == data_->cinematic.keyframes.end() && !data_->cinematic.keyframes.empty() &&
        data_->cinematic.keyframes.back().outgoing_speed <= 0.0f)
      return ARX_CINEMATIC_BAD_KEY_TIMING;
    const std::size_t index = static_cast<std::size_t>(position - data_->cinematic.keyframes.begin());
    cinematic::insertKeyframe(data_->cinematic, index, key);
    out_index = index;
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::removeKeyframe(std::size_t index) noexcept {
  if (index >= keyframeCount()) return ARX_INDEX_OUT_OF_RANGE;
  cinematic::removeKeyframe(data_->cinematic, index);
  return ARX_OK;
}

void Cinematic::clearKeyframes() noexcept { cinematic::clearKeyframes(data_->cinematic); }

ArxReturnCode Cinematic::setIllustration(CinematicIllustrationIndex index, ArxCinematicIllustration value) noexcept {
  if (static_cast<std::size_t>(index) >= illustrationCount()) return ARX_INDEX_OUT_OF_RANGE;
  const CinematicIllustration illustration{value.texture, value.subdivision_scale};
  const ArxReturnCode rc = cinematic_detail::errorCode(cinematic::validateIllustration(illustration, textureCount()));
  if (rc != ARX_OK) return rc;
  cinematic::setIllustration(data_->cinematic, index, illustration);
  return ARX_OK;
}

ArxReturnCode Cinematic::addIllustration(ArxCinematicIllustration value,
                                         CinematicIllustrationIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidCinematicIllustrationIndex;
    ArxReturnCode rc = cinematic_detail::errorCode(cinematic::validateIllustrationCount(illustrationCount() + 1U));
    if (rc != ARX_OK) return rc;
    const CinematicIllustration illustration{value.texture, value.subdivision_scale};
    rc = cinematic_detail::errorCode(cinematic::validateIllustration(illustration, textureCount()));
    if (rc != ARX_OK) return rc;
    out_index = cinematic::addIllustration(data_->cinematic, illustration);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::removeIllustration(CinematicIllustrationIndex index) noexcept {
  if (static_cast<std::size_t>(index) >= illustrationCount()) return ARX_INDEX_OUT_OF_RANGE;
  if (std::ranges::any_of(data_->cinematic.keyframes,
                          [=](const CinematicKeyframe& key) { return key.illustration == index; }))
    return ARX_CINEMATIC_ILLUSTRATION_IN_USE;
  cinematic::removeIllustration(data_->cinematic, index);
  return ARX_OK;
}

void Cinematic::clearIllustrations() noexcept { cinematic::clearIllustrations(data_->cinematic); }

ArxReturnCode Cinematic::compactTextures(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (removed) *removed = 0;
    std::vector<std::uint8_t> used(textureCount(), 0);
    for (const CinematicIllustration& illustration : data_->cinematic.illustrations) {
      if (static_cast<std::size_t>(illustration.texture) >= used.size()) return ARX_CINEMATIC_BAD_ILLUSTRATION;
      used[illustration.texture] = 1;
    }
    std::vector<TextureIndex> remap;
    std::size_t count = 0;
    const ArxReturnCode rc = cinematic_detail::textureError(textures::compact(data_->textures, used, remap, count));
    if (rc != ARX_OK) return rc;
    for (CinematicIllustration& illustration : data_->cinematic.illustrations) {
      if (illustration.texture >= remap.size() || remap[illustration.texture] == kNoTexture) return ARX_INTERNAL_ERROR;
      illustration.texture = remap[illustration.texture];
    }
    if (removed) *removed = count;
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::rebaseTexturePaths(std::string_view directory) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    textures::PathRebaseInfo info;
    const ArxReturnCode rc = cinematic_detail::textureError(textures::rebasePaths(data_->textures, directory, &info));
    if (rc != ARX_OK) return rc;
    for (const textures::PathRebaseInfo::Repair& repair : info.repairs)
      log(ARX_LOG_WARN, "Cinematic texture rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::setTexture(TextureIndex index, const ArxTextureView& value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
    Texture texture;
    if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
    textures::PathRepairInfo repair;
    ArxReturnCode rc = cinematic_detail::textureError(textures::repairPath(data_->textures, texture, index, &repair));
    if (rc != ARX_OK) return rc;
    rc = cinematic_detail::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return rc;
    textures::setTexture(data_->textures, index, std::move(texture));
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Cinematic texture: '{}' normalized to '{}'", item.original, item.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::addTexture(const ArxTextureView& value, TextureIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kNoTexture;
    Texture texture;
    if (!internalTexture(value, texture)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc = cinematic_detail::textureError(textures::validateTextureCount(textureCount() + 1U));
    if (rc != ARX_OK) return rc;
    textures::PathRepairInfo repair;
    rc = cinematic_detail::textureError(textures::repairPath(data_->textures, texture, kNoTexture, &repair));
    if (rc != ARX_OK) return rc;
    rc = cinematic_detail::textureError(textures::validateTexture(texture));
    if (rc != ARX_OK) return rc;
    out_index = textures::addTexture(data_->textures, std::move(texture));
    for (const textures::PathRepairInfo::Repair& item : repair.repairs)
      log(ARX_LOG_WARN, "Cinematic texture: '{}' normalized to '{}'", item.original, item.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
    if (encoded_image.size == 0) return ARX_CINEMATIC_BAD_TEXTURE_IMAGE;
    std::vector<std::uint8_t> image;
    if (!copyImage(encoded_image, image)) return ARX_INVALID_DATA_POINTER;
    const ArxReturnCode rc = cinematic_detail::textureError(textures::validateEncodedImage(image));
    if (rc != ARX_OK) return rc;
    textures::setEncodedImage(data_->textures, index, std::move(image));
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::clearTextureImage(TextureIndex index) noexcept {
  if (static_cast<std::size_t>(index) >= textureCount()) return ARX_INDEX_OUT_OF_RANGE;
  textures::clearEncodedImage(data_->textures, index);
  return ARX_OK;
}

ArxReturnCode Cinematic::compactSounds(SoundKind kind, std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (removed) *removed = 0;
    if (!validSoundKind(kind)) return ARX_INVALID_OPTIONS;
    std::vector<std::uint8_t> used(soundCount(kind), 0);
    for (const CinematicKeyframe& key : data_->cinematic.keyframes) {
      if (key.sound == kNoSoundHandle) continue;
      SoundKind key_kind = SoundKind::kEffect;
      SoundIndex index = kNoSound;
      if (soundHandleKind(key.sound, key_kind) != ARX_OK || soundHandleIndex(key.sound, index) != ARX_OK)
        return ARX_CINEMATIC_BAD_KEY_SOUND;
      if (key_kind == kind) {
        if (index >= used.size()) return ARX_CINEMATIC_BAD_KEY_SOUND;
        used[index] = 1;
      }
    }
    std::vector<SoundIndex> remap;
    std::size_t count = 0;
    ArxReturnCode rc = cinematic_detail::soundError(sounds::compact(data_->sounds, kind, used, remap, count));
    if (rc != ARX_OK) return rc;
    for (CinematicKeyframe& key : data_->cinematic.keyframes) {
      if (key.sound == kNoSoundHandle) continue;
      SoundKind key_kind = SoundKind::kEffect;
      SoundIndex index = kNoSound;
      if (soundHandleKind(key.sound, key_kind) != ARX_OK || soundHandleIndex(key.sound, index) != ARX_OK)
        return ARX_INTERNAL_ERROR;
      if (key_kind != kind) continue;
      if (index >= remap.size() || remap[index] == kNoSound) return ARX_INTERNAL_ERROR;
      if (soundHandle(kind, remap[index], key.sound) != ARX_OK) return ARX_INTERNAL_ERROR;
    }
    if (removed) *removed = count;
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::rebaseSoundPaths(SoundKind kind, std::string_view directory) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validSoundKind(kind)) return ARX_INVALID_OPTIONS;
    sounds::PathRebaseInfo info;
    const ArxReturnCode rc = cinematic_detail::soundError(sounds::rebasePaths(data_->sounds, kind, directory, &info));
    if (rc != ARX_OK) return rc;
    for (const sounds::PathRebaseInfo::Repair& repair : info.repairs)
      log(ARX_LOG_WARN, "Cinematic sound rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::setSoundPath(SoundHandle sound, std::string_view requested) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

ArxReturnCode Cinematic::addSound(SoundKind kind, std::string_view requested, SoundHandle& out_sound) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_sound = kNoSoundHandle;
    if (!validSoundKind(kind)) return ARX_INVALID_OPTIONS;
    ArxReturnCode rc = cinematic_detail::soundError(sounds::validateSoundCount(soundCount(kind) + 1U));
    if (rc != ARX_OK) return rc;
    std::string path(requested);
    sounds::PathRepairInfo repairs;
    rc = cinematic_detail::soundError(sounds::repairPath(data_->sounds, kind, path, kNoSound, &repairs));
    if (rc != ARX_OK) return rc;
    if (!sounds::validPath(path)) return ARX_CINEMATIC_BAD_SOUND_PATH;
    out_sound = sounds::addPath(data_->sounds, kind, std::move(path));
    for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
      log(ARX_LOG_WARN, "Cinematic sound: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::removeSound(SoundHandle sound) noexcept {
  if (!sounds::validHandle(data_->sounds, sound)) return ARX_INDEX_OUT_OF_RANGE;
  if (std::ranges::any_of(data_->cinematic.keyframes, [=](const CinematicKeyframe& key) { return key.sound == sound; }))
    return ARX_CINEMATIC_SOUND_IN_USE;
  SoundKind kind = SoundKind::kEffect;
  SoundIndex removed = kNoSound;
  if (soundHandleKind(sound, kind) != ARX_OK || soundHandleIndex(sound, removed) != ARX_OK)
    return ARX_INDEX_OUT_OF_RANGE;
  sounds::removeSound(data_->sounds, sound);
  for (CinematicKeyframe& key : data_->cinematic.keyframes) {
    if (key.sound == kNoSoundHandle) continue;
    SoundKind key_kind = SoundKind::kEffect;
    SoundIndex index = kNoSound;
    if (soundHandleKind(key.sound, key_kind) == ARX_OK && soundHandleIndex(key.sound, index) == ARX_OK &&
        key_kind == kind && index > removed)
      (void)soundHandle(kind, index - 1U, key.sound);
  }
  return ARX_OK;
}

ArxReturnCode Cinematic::setSoundData(SoundHandle sound, LanguageId language,
                                      ArxEncodedAudioView encoded_audio) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validateEncodingTarget(data_->sounds, sound, language);
    if (rc != ARX_OK) return rc;
    if (encoded_audio.size == 0) return ARX_CINEMATIC_BAD_SOUND_DATA;
    std::vector<std::uint8_t> data;
    if (!copyAudio(encoded_audio, data)) return ARX_INVALID_DATA_POINTER;
    rc = cinematic_detail::soundError(sounds::validateEncodedAudio(data));
    if (rc != ARX_OK) return rc;
    sounds::setEncodedAudio(data_->sounds, sound, language, std::move(data));
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::clearSoundData(SoundHandle sound, LanguageId language) noexcept {
  const ArxReturnCode rc = validateEncodingTarget(data_->sounds, sound, language);
  if (rc != ARX_OK) return rc;
  sounds::clearEncodedAudio(data_->sounds, sound, language);
  return ARX_OK;
}

ArxReturnCode Cinematic::setLanguage(LanguageId language, std::string_view name) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (language == kSoundEffects || language == kInvalidLanguageId || !validLanguageName(name))
      return ARX_CINEMATIC_BAD_LANGUAGE;
    if (languageNameUsed(data_->sounds, name, language)) return ARX_CINEMATIC_DUPLICATE_LANGUAGE;
    sounds::setLanguage(data_->sounds, language, std::string(name));
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::addLanguage(std::string_view name, LanguageId& out_language) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_language = kInvalidLanguageId;
    if (!validLanguageName(name)) return ARX_CINEMATIC_BAD_LANGUAGE;
    if (languageNameUsed(data_->sounds, name, kInvalidLanguageId)) return ARX_CINEMATIC_DUPLICATE_LANGUAGE;
    LanguageId next = 1;
    for (const auto& [language, unused] : data_->sounds.languages) {
      (void)unused;
      if (language != next) break;
      if (next == kInvalidLanguageId - 1U) return ARX_CINEMATIC_BAD_LANGUAGE;
      ++next;
    }
    sounds::setLanguage(data_->sounds, next, std::string(name));
    out_language = next;
    return ARX_OK;
  });
}

ArxReturnCode Cinematic::removeLanguage(LanguageId language) noexcept {
  if (language == kSoundEffects || !data_->sounds.languages.contains(language)) return ARX_INDEX_OUT_OF_RANGE;
  sounds::removeLanguage(data_->sounds, language);
  return ARX_OK;
}

}  // namespace pistoris
