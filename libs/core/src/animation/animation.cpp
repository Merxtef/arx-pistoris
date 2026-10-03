// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.hpp"

#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "animation/internal.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "modules/animation.h"
#include "modules/resource.h"
#include "modules/sounds.h"
#include "utils/log.h"
#include "utils/math/quat.h"
#include "utils/math/rotation.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

ArxReturnCode resourceError(resource::Error error) noexcept {
  switch (error) {
    case resource::Error::kNone:
      return ARX_OK;
    case resource::Error::kBadPath:
      return ARX_ANIMATION_BAD_RESOURCE_PATH;
    case resource::Error::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

}  // namespace

namespace animation_detail {

ArxReturnCode soundErrorCode(sounds::Error error) noexcept {
  switch (error) {
    case sounds::Error::kNone:
      return ARX_OK;
    case sounds::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case sounds::Error::kTooManySounds:
      return ARX_ANIMATION_TOO_MANY_SOUNDS;
    case sounds::Error::kBadPath:
      return ARX_ANIMATION_BAD_SOUND_PATH;
    case sounds::Error::kBadAudio:
      return ARX_ANIMATION_BAD_SOUND_DATA;
    case sounds::Error::kUnsupportedChannels:
      return ARX_ANIMATION_UNSUPPORTED_SOUND_CHANNELS;
    case sounds::Error::kAudioTooLarge:
      return ARX_ANIMATION_SOUND_TOO_LARGE;
    case sounds::Error::kDuplicatePath:
      return ARX_ANIMATION_DUPLICATE_SOUND_PATH;
    case sounds::Error::kBadKind:
    case sounds::Error::kBadLanguage:
    case sounds::Error::kDuplicateEncoding:
      return ARX_INTERNAL_ERROR;
    case sounds::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
    case sounds::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_INTERNAL_ERROR;
}

}  // namespace animation_detail

namespace {

bool validSoundView(const ArxSoundView& sound) noexcept {
  return (sound.path.data || sound.path.size == 0) && (sound.encoded_audio.data || sound.encoded_audio.size == 0);
}

ArxStringView borrowedString(std::string_view value) noexcept {
  return {value.data(), value.size()};  // NOLINT(bugprone-suspicious-stringview-data-usage)
}

Sound internalSound(const ArxSoundView& source) {
  Sound result;
  result.path.assign(source.path.data ? source.path.data : "", source.path.size);
  if (source.encoded_audio.size != 0)
    result.encoded_audio.assign(source.encoded_audio.data, source.encoded_audio.data + source.encoded_audio.size);
  return result;
}

ArxReturnCode validateInput(const ArxAnimationKeyframeInput& input) noexcept {
  if (input.group_count != 0 && input.group_transforms == nullptr) return ARX_INVALID_DATA_POINTER;
  return ARX_OK;
}

AnimationGroupTransform internalTransform(const ArxAnimationGroupTransform& source) noexcept {
  return {math::canonicalizeQuaternionSign(source.rotation), source.translation, source.scale};
}

ArxReturnCode prepareInput(const ArxAnimationKeyframeInput& input, AnimationKeyframe& keyframe,
                           std::vector<AnimationGroupTransform>& transforms) {
  ArxReturnCode rc = validateInput(input);
  if (rc != ARX_OK) return rc;
  keyframe.frame = input.keyframe.frame;
  keyframe.root_translation = input.keyframe.root_translation;
  keyframe.root_rotation = input.keyframe.root_rotation;
  keyframe.footstep = input.keyframe.footstep != 0;
  keyframe.sound = input.keyframe.sound == kNoSound ? kNoSoundHandle : sounds::effectHandle(input.keyframe.sound);
  transforms.reserve(transforms.size() + input.group_count);
  for (std::size_t index = 0; index < input.group_count; ++index)
    transforms.push_back(internalTransform(input.group_transforms[index]));
  return ARX_OK;
}

void invalidateGroupStateCache(AnimationGroupStateCache& cache) noexcept {
  cache.identity_known.reset();
  cache.identity_value.reset();
}

void cacheGroupIdentity(AnimationGroupStateCache& cache, std::size_t group, bool identity) noexcept {
  cache.identity_known.set(group);
  cache.identity_value.set(group, identity);
}

void updateSetCache(AnimationGroupStateCache& cache, std::span<const AnimationGroupTransform> previous,
                    std::span<const AnimationGroupTransform> replacement) noexcept {
  for (std::size_t group = 0; group < replacement.size(); ++group) {
    if (!animation::isIdentityTransform(replacement[group])) {
      cacheGroupIdentity(cache, group, false);
    } else if (!animation::isIdentityTransform(previous[group])) {
      cache.identity_known.reset(group);
    }
  }
}

void updateAddCache(AnimationGroupStateCache& cache, std::span<const AnimationGroupTransform> transforms,
                    bool first) noexcept {
  if (first) invalidateGroupStateCache(cache);
  for (std::size_t group = 0; group < transforms.size(); ++group) {
    const bool identity = animation::isIdentityTransform(transforms[group]);
    if (first || !identity) cacheGroupIdentity(cache, group, identity);
  }
}

void updateRemoveCache(AnimationGroupStateCache& cache, std::span<const AnimationGroupTransform> removed,
                       bool last) noexcept {
  if (last) {
    invalidateGroupStateCache(cache);
    return;
  }
  for (std::size_t group = 0; group < removed.size(); ++group)
    if (!animation::isIdentityTransform(removed[group]) && cache.identity_known.test(group) &&
        !cache.identity_value.test(group))
      cache.identity_known.reset(group);
}

}  // namespace

namespace animation_detail {

ArxReturnCode errorCode(animation::Error error) noexcept {
  switch (error) {
    case animation::Error::kNone:
      return ARX_OK;
    case animation::Error::kBadName:
      return ARX_ANIMATION_BAD_NAME;
    case animation::Error::kNoKeyframes:
      return ARX_ANIMATION_NO_KEYFRAMES;
    case animation::Error::kTooManyKeyframes:
      return ARX_ANIMATION_TOO_MANY_KEYFRAMES;
    case animation::Error::kTooManyGroups:
      return ARX_ANIMATION_TOO_MANY_GROUPS;
    case animation::Error::kBadFrameLength:
      return ARX_ANIMATION_BAD_FRAME_LENGTH;
    case animation::Error::kBadFrame:
      return ARX_ANIMATION_BAD_FRAME;
    case animation::Error::kBadRootTransform:
      return ARX_ANIMATION_BAD_ROOT_TRANSFORM;
    case animation::Error::kBadGroupTransform:
      return ARX_ANIMATION_BAD_GROUP_TRANSFORM;
    case animation::Error::kBadSound:
      return ARX_ANIMATION_BAD_KEYFRAME_SOUND;
    case animation::Error::kBadTransformCount:
      return ARX_ANIMATION_BAD_TRANSFORM_COUNT;
    case animation::Error::kBadGroupClaim:
      return ARX_ANIMATION_BAD_GROUP_CLAIM;
    case animation::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode validateStructure(const AnimationModules& modules) noexcept {
  ArxReturnCode rc = resourceError(resource::validate(modules.resource, ARX_RESOURCE_KIND_ANIMATION));
  if (rc != ARX_OK) return rc;
  rc = soundErrorCode(sounds::validateStructure(modules.sounds));
  if (rc != ARX_OK) return rc;
  return errorCode(animation::validate(modules.animation, sounds::count(modules.sounds, SoundKind::kEffect)));
}

ArxAnimationKeyframe publicKeyframe(const AnimationKeyframe& source) noexcept {
  SoundIndex sound = kNoSound;
  if (source.sound != kNoSoundHandle) {
    const ArxReturnCode rc = soundHandleIndex(source.sound, sound);
    assert(rc == ARX_OK);
    (void)rc;
  }
  return {
      source.frame, source.root_translation, source.root_rotation, static_cast<std::uint8_t>(source.footstep), sound};
}

ArxAnimationGroupTransform publicTransform(const AnimationGroupTransform& source) noexcept {
  return {source.rotation, source.translation, source.scale};
}

}  // namespace animation_detail

Animation::Animation() : data_(std::make_unique<Data>()) {}

Animation::~Animation() = default;

Animation::Animation(const Animation& other) : data_(other.data_ ? std::make_unique<Data>(*other.data_) : nullptr) {}

Animation::Animation(Animation&& other) noexcept = default;

Animation& Animation::operator=(const Animation& other) {
  if (this == &other) return *this;
  Animation copy(other);
  swap(copy);
  return *this;
}

AnimationLocation animationLocation(std::string_view resource_path, AnimationElement element,
                                    std::size_t index = kNoElementIndex, std::size_t subindex = kNoElementIndex) {
  AnimationLocation result;
  result.resource_path = resource_path;
  result.element = element;
  result.index = index;
  result.subindex = subindex;
  return result;
}

Animation& Animation::operator=(Animation&& other) noexcept = default;

void Animation::swap(Animation& other) noexcept { data_.swap(other.data_); }

AnimationResult<void> Animation::reset() noexcept {
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<void> {
    auto replacement = std::make_unique<Data>();
    data_.swap(replacement);
    return AnimationResult<void>::success();
  });
}

AnimationResult<void> Animation::validate() const noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kResource);
  if (!data_) return api_detail::animationFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::resourceValidationBoundary<AnimationResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        ArxReturnCode rc = animation_detail::validateStructure(static_cast<const AnimationModules&>(*data_));
        if (rc != ARX_OK) return rc;
        return animation_detail::soundErrorCode(sounds::validateAudio(data_->sounds));
      },
      location);
}

AnimationResult<void> Animation::scale(float factor) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kResource);
  if (!data_) return api_detail::animationFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!std::isfinite(factor) || factor <= 0.0f) return ARX_INVALID_OPTIONS;
        const ArxReturnCode rc = animation_detail::errorCode(animation::validateScale(data_->animation, factor));
        if (rc != ARX_OK) return rc;
        animation::applyScale(data_->animation, factor);
        invalidateGroupStateCache(data_->group_state_cache);
        return ARX_OK;
      },
      location);
}

AnimationResult<void> Animation::rotate(ArxQuat rotation) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kResource);
  if (!data_) return api_detail::animationFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!math::normalizeRotation(rotation)) return ARX_INVALID_OPTIONS;
        const ArxReturnCode rc = animation_detail::errorCode(animation::validateRotation(data_->animation, rotation));
        if (rc != ARX_OK) return rc;
        animation::applyRotation(data_->animation, rotation);
        invalidateGroupStateCache(data_->group_state_cache);
        return ARX_OK;
      },
      location);
}

std::string_view Animation::name() const noexcept {
  return data_ ? std::string_view(data_->animation.name) : std::string_view{};
}

AnimationResult<void> Animation::setName(std::string_view name_value) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kResource);
  if (!data_) return api_detail::animationFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const ArxReturnCode rc = animation_detail::errorCode(animation::validateName(name_value));
        if (rc != ARX_OK) return rc;
        animation::setName(data_->animation, name_value);
        return ARX_OK;
      },
      location);
}

std::string_view Animation::resourcePath() const noexcept {
  return data_ ? std::string_view(data_->resource.path) : std::string_view{};
}

AnimationResult<void> Animation::setResourcePath(std::string_view resource_path) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kResource);
  if (!data_) return api_detail::animationFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::string path;
        const ArxReturnCode rc = resourceError(resource::repairPath(ARX_RESOURCE_KIND_ANIMATION, resource_path, path));
        if (rc != ARX_OK) return rc;
        resource::setPath(data_->resource, std::move(path));
        return ARX_OK;
      },
      location);
}

std::uint32_t Animation::frameLength() const noexcept { return data_ ? data_->animation.frame_length : 0; }

std::size_t Animation::groupCount() const noexcept { return data_ ? data_->animation.group_count : 0; }

AnimationResult<bool> Animation::isGroupVoid(std::size_t group) const noexcept {
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<bool> {
    if (!data_)
      return api_detail::animationFailure<bool>(
          ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
    if (group >= data_->animation.group_count)
      return api_detail::animationFailure<bool>(
          ARX_INDEX_OUT_OF_RANGE, animationLocation(data_->resource.path, AnimationElement::kGroup, group));
    if (animation::isGroupClaimed(data_->animation, group)) return AnimationResult<bool>::success(false);
    if (!data_->group_state_cache.identity_known.test(group))
      cacheGroupIdentity(data_->group_state_cache, group, animation::isIdentityGroup(data_->animation, group));
    return AnimationResult<bool>::success(data_->group_state_cache.identity_value.test(group));
  });
}

AnimationResult<bool> Animation::isGroupClaimed(std::size_t group) const noexcept {
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<bool> {
    if (!data_)
      return api_detail::animationFailure<bool>(
          ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
    if (group >= data_->animation.group_count)
      return api_detail::animationFailure<bool>(
          ARX_INDEX_OUT_OF_RANGE, animationLocation(data_->resource.path, AnimationElement::kGroup, group));
    return AnimationResult<bool>::success(animation::isGroupClaimed(data_->animation, group));
  });
}

std::size_t Animation::keyframeCount() const noexcept { return data_ ? data_->animation.keyframes.size() : 0; }

std::size_t Animation::soundCount() const noexcept {
  return data_ ? sounds::count(data_->sounds, SoundKind::kEffect) : 0;
}

ArxSoundView Animation::soundAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Animation*>(owner);
  const SoundHandle handle = sounds::effectHandle(static_cast<SoundIndex>(index));
  const std::string_view source_path = sounds::path(self.data_->sounds, handle);
  const std::span<const std::uint8_t> source_audio = sounds::encodedAudio(self.data_->sounds, handle);
  return {borrowedString(source_path), {source_audio.data(), source_audio.size()}};
}

ArxAnimationKeyframe Animation::keyframeAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Animation*>(owner);
  return animation_detail::publicKeyframe(self.data_->animation.keyframes[index]);
}

ArxAnimationGroupTransform Animation::groupTransformAt(const void* owner, std::size_t keyframe,
                                                       std::size_t group) noexcept {
  const auto& self = *static_cast<const Animation*>(owner);
  return animation_detail::publicTransform(
      self.data_->animation.group_transforms[keyframe * self.data_->animation.group_count + group]);
}

Animation::SoundsView Animation::sounds() const noexcept {
  return data_ ? SoundsView(this, 0, soundCount(), &Animation::soundAt) : SoundsView{};
}

Animation::KeyframesView Animation::keyframes() const noexcept {
  return data_ ? KeyframesView(this, 0, keyframeCount(), &Animation::keyframeAt) : KeyframesView{};
}

AnimationResult<Animation::GroupTransformsView> Animation::groupTransforms(std::size_t keyframe) const noexcept {
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<GroupTransformsView> {
    if (!data_)
      return api_detail::animationFailure<GroupTransformsView>(
          ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
    if (keyframe >= keyframeCount())
      return api_detail::animationFailure<GroupTransformsView>(
          ARX_INDEX_OUT_OF_RANGE, animationLocation(data_->resource.path, AnimationElement::kKeyframe, keyframe));
    return AnimationResult<GroupTransformsView>::success(
        GroupTransformsView(this, keyframe, groupCount(), &Animation::groupTransformAt));
  });
}

AnimationResult<void> Animation::setFrameLength(std::uint32_t frame_length) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kResource);
  if (!data_) return api_detail::animationFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const ArxReturnCode rc =
            animation_detail::errorCode(animation::validateFrameLength(data_->animation, frame_length));
        if (rc != ARX_OK) return rc;
        animation::setFrameLength(data_->animation, frame_length);
        return ARX_OK;
      },
      location);
}

AnimationResult<void> Animation::setKeyframe(std::size_t index, const ArxAnimationKeyframeInput& input) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kKeyframe, index);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= data_->animation.keyframes.size()) return ARX_INDEX_OUT_OF_RANGE;
        if (input.group_count != data_->animation.group_count) return ARX_ANIMATION_BAD_TRANSFORM_COUNT;
        AnimationKeyframe replacement;
        std::vector<AnimationGroupTransform> transforms;
        ArxReturnCode rc = prepareInput(input, replacement, transforms);
        if (rc != ARX_OK) return rc;
        rc = animation_detail::errorCode(
            animation::validateKeyframeSet(data_->animation, index, replacement, transforms, soundCount()));
        if (rc != ARX_OK) return rc;
        const std::size_t first = index * data_->animation.group_count;
        updateSetCache(data_->group_state_cache,
                       std::span<const AnimationGroupTransform>(data_->animation.group_transforms)
                           .subspan(first, data_->animation.group_count),
                       transforms);
        animation::setKeyframe(data_->animation, index, replacement, transforms);
        return ARX_OK;
      },
      location);
}

AnimationResult<std::size_t> Animation::addKeyframe(const ArxAnimationKeyframeInput& input) noexcept {
  if (!data_)
    return api_detail::animationFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  const AnimationLocation location =
      api_detail::resourceLocation(resourcePath(), AnimationElement::kKeyframe, data_->animation.keyframes.size());
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<std::size_t> {
    ArxReturnCode rc = animation_detail::errorCode(
        animation::validateKeyframeAppend(data_->animation, input.keyframe.frame, input.group_count));
    if (rc != ARX_OK) return api_detail::animationFailure<std::size_t>(rc, location);
    AnimationKeyframe keyframe;
    std::vector<AnimationGroupTransform> transforms;
    rc = prepareInput(input, keyframe, transforms);
    if (rc != ARX_OK) return api_detail::animationFailure<std::size_t>(rc, location);
    rc = animation_detail::errorCode(animation::validateKeyframe(keyframe, soundCount()));
    if (rc != ARX_OK) return api_detail::animationFailure<std::size_t>(rc, location);
    for (std::size_t group = 0; group < transforms.size(); ++group) {
      const AnimationGroupTransform& transform = transforms[group];
      rc = animation_detail::errorCode(animation::validateTransform(transform));
      if (rc != ARX_OK)
        return api_detail::animationFailure<std::size_t>(
            rc,
            api_detail::resourceLocation(
                resourcePath(), AnimationElement::kGroupTransform, data_->animation.keyframes.size(), group));
    }
    const bool first = data_->animation.keyframes.empty();
    const std::size_t index = animation::addKeyframe(data_->animation, keyframe, transforms);
    updateAddCache(data_->group_state_cache, transforms, first);
    return AnimationResult<std::size_t>::success(index);
  });
}

AnimationResult<void> Animation::removeKeyframe(std::size_t index) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kKeyframe, index);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= data_->animation.keyframes.size()) return ARX_INDEX_OUT_OF_RANGE;
        const std::size_t first = index * data_->animation.group_count;
        updateRemoveCache(data_->group_state_cache,
                          std::span<const AnimationGroupTransform>(data_->animation.group_transforms)
                              .subspan(first, data_->animation.group_count),
                          data_->animation.keyframes.size() == 1U);
        animation::removeKeyframe(data_->animation, index);
        return ARX_OK;
      },
      location);
}

AnimationResult<void> Animation::replaceKeyframes(std::uint32_t frame_length,
                                                  const ArxAnimationKeyframeInput* keyframes_input,
                                                  std::size_t keyframe_count) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kKeyframe);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (keyframe_count != 0 && keyframes_input == nullptr) return ARX_INVALID_DATA_POINTER;
        const std::size_t groups = keyframe_count == 0 ? 0 : keyframes_input[0].group_count;
        ArxReturnCode rc =
            animation_detail::errorCode(animation::validateTimelineShape(frame_length, keyframe_count, groups));
        if (rc != ARX_OK) return rc;
        std::vector<AnimationKeyframe> keyframes;
        std::vector<AnimationGroupTransform> transforms;
        keyframes.reserve(keyframe_count);
        transforms.reserve(keyframe_count * groups);
        for (std::size_t index = 0; index < keyframe_count; ++index) {
          if (keyframes_input[index].group_count != groups) return ARX_ANIMATION_BAD_TRANSFORM_COUNT;
          AnimationKeyframe keyframe;
          rc = prepareInput(keyframes_input[index], keyframe, transforms);
          if (rc != ARX_OK) return rc;
          keyframes.push_back(keyframe);
        }
        rc = animation_detail::errorCode(
            animation::validateTimeline(frame_length, groups, keyframes, transforms, soundCount()));
        if (rc != ARX_OK) return rc;
        animation::replaceKeyframes(
            data_->animation, frame_length, groups, std::move(keyframes), std::move(transforms));
        invalidateGroupStateCache(data_->group_state_cache);
        return ARX_OK;
      },
      location);
}

void Animation::clearKeyframes() noexcept {
  if (!data_) return;
  animation::clearKeyframes(data_->animation);
  invalidateGroupStateCache(data_->group_state_cache);
}

AnimationResult<void> Animation::claimGroup(std::size_t group) noexcept {
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
        animation::claimGroup(data_->animation, group);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AnimationElement::kGroup, group));
}

AnimationResult<void> Animation::unclaimGroup(std::size_t group) noexcept {
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
        animation::unclaimGroup(data_->animation, group);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AnimationElement::kGroup, group));
}

AnimationResult<void> Animation::voidGroup(std::size_t group) noexcept {
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
        animation::voidGroup(data_->animation, group);
        cacheGroupIdentity(data_->group_state_cache, group, true);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AnimationElement::kGroup, group));
}

AnimationResult<std::size_t> Animation::compactSounds() noexcept {
  if (!data_)
    return api_detail::animationFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<std::size_t> {
    std::vector<std::uint8_t> used(soundCount(), 0);
    for (std::size_t keyframe_index = 0; keyframe_index < data_->animation.keyframes.size(); ++keyframe_index) {
      const AnimationKeyframe& keyframe = data_->animation.keyframes[keyframe_index];
      if (keyframe.sound == kNoSoundHandle) continue;
      SoundIndex index = kNoSound;
      if (soundHandleIndex(keyframe.sound, index) != ARX_OK || index >= used.size())
        return api_detail::animationFailure<std::size_t>(
            ARX_ANIMATION_BAD_KEYFRAME_SOUND,
            api_detail::resourceLocation(resourcePath(), AnimationElement::kKeyframe, keyframe_index));
      used[index] = 1;
    }
    std::vector<SoundIndex> remap;
    std::size_t removed_count = 0;
    const ArxReturnCode rc = animation_detail::soundErrorCode(
        sounds::compact(data_->sounds, SoundKind::kEffect, used, remap, removed_count));
    if (rc != ARX_OK)
      return api_detail::animationFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), AnimationElement::kSound));
    for (std::size_t keyframe_index = 0; keyframe_index < data_->animation.keyframes.size(); ++keyframe_index) {
      AnimationKeyframe& keyframe = data_->animation.keyframes[keyframe_index];
      if (keyframe.sound == kNoSoundHandle) continue;
      SoundIndex index = kNoSound;
      if (soundHandleIndex(keyframe.sound, index) != ARX_OK || index >= remap.size() || remap[index] == kNoSound)
        return api_detail::animationFailure<std::size_t>(
            ARX_INTERNAL_ERROR,
            api_detail::resourceLocation(resourcePath(), AnimationElement::kKeyframe, keyframe_index));
      keyframe.sound = sounds::effectHandle(remap[index]);
    }
    return AnimationResult<std::size_t>::success(removed_count);
  });
}

AnimationResult<void> Animation::rebaseSoundPaths(std::string_view directory) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kSound);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        sounds::PathRebaseInfo info;
        const ArxReturnCode rc = animation_detail::soundErrorCode(sounds::rebasePaths(data_->sounds, directory, &info));
        if (rc != ARX_OK) return rc;
        for (const sounds::PathRebaseInfo::Repair& repair : info.repairs)
          log(ARX_LOG_WARN, "Animation sound rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      location);
}

AnimationResult<void> Animation::setSound(SoundIndex index, const ArxSoundView& sound) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kSound, index);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validSoundView(sound)) return ARX_INVALID_DATA_POINTER;
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        Sound next = internalSound(sound);
        sounds::PathRepairInfo repairs;
        ArxReturnCode rc = animation_detail::soundErrorCode(sounds::repairPath(data_->sounds, next, index, &repairs));
        if (rc != ARX_OK) return rc;
        rc = animation_detail::soundErrorCode(sounds::validateSound(next));
        if (rc != ARX_OK) return rc;
        sounds::setSound(data_->sounds, index, std::move(next));
        for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
          log(ARX_LOG_WARN, "Animation sound path '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      location);
}

AnimationResult<SoundIndex> Animation::addSound(const ArxSoundView& sound) noexcept {
  if (!data_)
    return api_detail::animationFailure<SoundIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  const AnimationLocation location =
      api_detail::resourceLocation(resourcePath(), AnimationElement::kSound, soundCount());
  return api_detail::animationBoundary(resourcePath(), [&]() -> AnimationResult<SoundIndex> {
    if (!validSoundView(sound)) return api_detail::animationFailure<SoundIndex>(ARX_INVALID_DATA_POINTER, location);
    ArxReturnCode rc = animation_detail::soundErrorCode(sounds::validateSoundCount(soundCount() + 1U));
    if (rc != ARX_OK) return api_detail::animationFailure<SoundIndex>(rc, location);
    Sound next = internalSound(sound);
    sounds::PathRepairInfo repairs;
    rc = animation_detail::soundErrorCode(sounds::repairPath(data_->sounds, next, kNoSound, &repairs));
    if (rc != ARX_OK) return api_detail::animationFailure<SoundIndex>(rc, location);
    rc = animation_detail::soundErrorCode(sounds::validateSound(next));
    if (rc != ARX_OK) return api_detail::animationFailure<SoundIndex>(rc, location);
    const SoundIndex index = sounds::addSound(data_->sounds, std::move(next));
    for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
      log(ARX_LOG_WARN, "Animation sound path '{}' normalized to '{}'", repair.original, repair.repaired);
    return AnimationResult<SoundIndex>::success(index);
  });
}

AnimationResult<void> Animation::setSoundPath(SoundIndex index, std::string_view requested) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kSound, index);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        Sound candidate{std::string(requested), {}};
        sounds::PathRepairInfo repairs;
        ArxReturnCode rc =
            animation_detail::soundErrorCode(sounds::repairPath(data_->sounds, candidate, index, &repairs));
        if (rc != ARX_OK) return rc;
        rc = animation_detail::soundErrorCode(sounds::validateSound(candidate));
        if (rc != ARX_OK) return rc;
        sounds::setPath(data_->sounds, index, std::move(candidate.path));
        for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
          log(ARX_LOG_WARN, "Animation sound path '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      location);
}

AnimationResult<void> Animation::setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept {
  const AnimationLocation location = api_detail::resourceLocation(resourcePath(), AnimationElement::kSound, index);
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!encoded_audio.data && encoded_audio.size != 0) return ARX_INVALID_DATA_POINTER;
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        std::vector<std::uint8_t> data;
        if (encoded_audio.size != 0) data.assign(encoded_audio.data, encoded_audio.data + encoded_audio.size);
        const ArxReturnCode rc = animation_detail::soundErrorCode(sounds::validateEncodedAudio(data));
        if (rc != ARX_OK) return rc;
        sounds::setEncodedAudio(data_->sounds, index, std::move(data));
        return ARX_OK;
      },
      location);
}

AnimationResult<void> Animation::clearSoundData(SoundIndex index) noexcept {
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        sounds::clearEncodedAudio(data_->sounds, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AnimationElement::kSound, index));
}

AnimationResult<void> Animation::removeSound(SoundIndex index) noexcept {
  if (!data_)
    return api_detail::animationFailure<void>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AnimationElement::kResource));
  return api_detail::animationStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        const SoundHandle removed_handle = sounds::effectHandle(index);
        if (std::ranges::any_of(data_->animation.keyframes, [removed_handle](const AnimationKeyframe& keyframe) {
              return keyframe.sound == removed_handle;
            }))
          return ARX_ANIMATION_SOUND_IN_USE;
        sounds::removeSound(data_->sounds, index);
        for (AnimationKeyframe& keyframe : data_->animation.keyframes) {
          if (keyframe.sound == kNoSoundHandle) continue;
          SoundIndex current = kNoSound;
          if (soundHandleIndex(keyframe.sound, current) == ARX_OK && current > index)
            keyframe.sound = sounds::effectHandle(current - 1U);
        }
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AnimationElement::kSound, index));
}

}  // namespace pistoris
