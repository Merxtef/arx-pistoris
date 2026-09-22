// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.hpp"

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "animation/internal.h"
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

Animation::Animation(const Animation& other) : data_(std::make_unique<Data>(*other.data_)) {}

Animation& Animation::operator=(const Animation& other) {
  if (this == &other) return *this;
  Animation copy(other);
  swap(copy);
  return *this;
}

void Animation::swap(Animation& other) noexcept { data_.swap(other.data_); }

void Animation::reset() { data_ = std::make_unique<Data>(); }

ArxReturnCode Animation::validate() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = animation_detail::validateStructure(static_cast<const AnimationModules&>(*data_));
    if (rc != ARX_OK) return rc;
    return animation_detail::soundErrorCode(sounds::validateAudio(data_->sounds));
  });
}

ArxReturnCode Animation::scale(float factor) noexcept {
  if (!std::isfinite(factor) || factor <= 0.0f) return ARX_INVALID_OPTIONS;
  const ArxReturnCode rc = animation_detail::errorCode(animation::validateScale(data_->animation, factor));
  if (rc != ARX_OK) return rc;
  animation::applyScale(data_->animation, factor);
  invalidateGroupStateCache(data_->group_state_cache);
  return ARX_OK;
}

ArxReturnCode Animation::rotate(ArxQuat rotation) noexcept {
  if (!math::normalizeRotation(rotation)) return ARX_INVALID_OPTIONS;
  const ArxReturnCode rc = animation_detail::errorCode(animation::validateRotation(data_->animation, rotation));
  if (rc != ARX_OK) return rc;
  animation::applyRotation(data_->animation, rotation);
  invalidateGroupStateCache(data_->group_state_cache);
  return ARX_OK;
}

std::string_view Animation::name() const noexcept { return data_->animation.name; }

ArxReturnCode Animation::setName(std::string_view name_value) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = animation_detail::errorCode(animation::validateName(name_value));
    if (rc != ARX_OK) return rc;
    animation::setName(data_->animation, name_value);
    return ARX_OK;
  });
}

std::string_view Animation::resourcePath() const noexcept { return data_->resource.path; }

ArxReturnCode Animation::setResourcePath(std::string_view resource_path) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::string path;
    const ArxReturnCode rc = resourceError(resource::repairPath(ARX_RESOURCE_KIND_ANIMATION, resource_path, path));
    if (rc != ARX_OK) return rc;
    resource::setPath(data_->resource, std::move(path));
    return ARX_OK;
  });
}

std::uint32_t Animation::frameLength() const noexcept { return data_->animation.frame_length; }

std::size_t Animation::groupCount() const noexcept { return data_->animation.group_count; }

ArxReturnCode Animation::isGroupVoid(std::size_t group, bool& out) const noexcept {
  out = false;
  if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
  if (animation::isGroupClaimed(data_->animation, group)) return ARX_OK;
  if (!data_->group_state_cache.identity_known.test(group))
    cacheGroupIdentity(data_->group_state_cache, group, animation::isIdentityGroup(data_->animation, group));
  out = data_->group_state_cache.identity_value.test(group);
  return ARX_OK;
}

ArxReturnCode Animation::isGroupClaimed(std::size_t group, bool& out) const noexcept {
  out = false;
  if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
  out = animation::isGroupClaimed(data_->animation, group);
  return ARX_OK;
}

std::size_t Animation::keyframeCount() const noexcept { return data_->animation.keyframes.size(); }

std::size_t Animation::soundCount() const noexcept { return sounds::count(data_->sounds, SoundKind::kEffect); }

ArxReturnCode Animation::copySoundViews(std::size_t offset, std::size_t count, ArxSoundView* out_views) const noexcept {
  const ArxReturnCode rc = validateCopyRange(soundCount(), offset, count, out_views);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    const SoundHandle handle = sounds::effectHandle(static_cast<SoundIndex>(offset + index));
    const std::string_view source_path = sounds::path(data_->sounds, handle);
    const std::span<const std::uint8_t> source_audio = sounds::encodedAudio(data_->sounds, handle);
    out_views[index] = {borrowedString(source_path), {source_audio.data(), source_audio.size()}};
  }
  return ARX_OK;
}

ArxReturnCode Animation::copyKeyframes(std::size_t offset, std::size_t count,
                                       ArxAnimationKeyframe* out_keyframes) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->animation.keyframes.size(), offset, count, out_keyframes);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index)
    out_keyframes[index] = animation_detail::publicKeyframe(data_->animation.keyframes[offset + index]);
  return ARX_OK;
}

ArxReturnCode Animation::copyGroupTransforms(std::size_t keyframe, std::size_t offset, std::size_t count,
                                             ArxAnimationGroupTransform* out_transforms) const noexcept {
  if (keyframe >= data_->animation.keyframes.size()) return ARX_INDEX_OUT_OF_RANGE;
  ArxReturnCode rc = validateCopyRange(data_->animation.group_count, offset, count, out_transforms);
  if (rc != ARX_OK) return rc;
  const std::size_t first = keyframe * data_->animation.group_count + offset;
  for (std::size_t index = 0; index < count; ++index)
    out_transforms[index] = animation_detail::publicTransform(data_->animation.group_transforms[first + index]);
  return ARX_OK;
}

ArxReturnCode Animation::setFrameLength(std::uint32_t frame_length) noexcept {
  const ArxReturnCode rc = animation_detail::errorCode(animation::validateFrameLength(data_->animation, frame_length));
  if (rc != ARX_OK) return rc;
  animation::setFrameLength(data_->animation, frame_length);
  return ARX_OK;
}

ArxReturnCode Animation::setKeyframe(std::size_t index, const ArxAnimationKeyframeInput& input) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

ArxReturnCode Animation::addKeyframe(const ArxAnimationKeyframeInput& input, std::size_t& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = std::numeric_limits<std::size_t>::max();
    ArxReturnCode rc = animation_detail::errorCode(
        animation::validateKeyframeAppend(data_->animation, input.keyframe.frame, input.group_count));
    if (rc != ARX_OK) return rc;
    AnimationKeyframe keyframe;
    std::vector<AnimationGroupTransform> transforms;
    rc = prepareInput(input, keyframe, transforms);
    if (rc != ARX_OK) return rc;
    rc = animation_detail::errorCode(animation::validateKeyframe(keyframe, soundCount()));
    if (rc != ARX_OK) return rc;
    for (const AnimationGroupTransform& transform : transforms) {
      rc = animation_detail::errorCode(animation::validateTransform(transform));
      if (rc != ARX_OK) return rc;
    }
    const bool first = data_->animation.keyframes.empty();
    out_index = animation::addKeyframe(data_->animation, keyframe, transforms);
    updateAddCache(data_->group_state_cache, transforms, first);
    return ARX_OK;
  });
}

ArxReturnCode Animation::removeKeyframe(std::size_t index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (index >= data_->animation.keyframes.size()) return ARX_INDEX_OUT_OF_RANGE;
    const std::size_t first = index * data_->animation.group_count;
    updateRemoveCache(data_->group_state_cache,
                      std::span<const AnimationGroupTransform>(data_->animation.group_transforms)
                          .subspan(first, data_->animation.group_count),
                      data_->animation.keyframes.size() == 1U);
    animation::removeKeyframe(data_->animation, index);
    return ARX_OK;
  });
}

ArxReturnCode Animation::replaceKeyframes(std::uint32_t frame_length, const ArxAnimationKeyframeInput* keyframes_input,
                                          std::size_t keyframe_count) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
    animation::replaceKeyframes(data_->animation, frame_length, groups, std::move(keyframes), std::move(transforms));
    invalidateGroupStateCache(data_->group_state_cache);
    return ARX_OK;
  });
}

void Animation::clearKeyframes() noexcept {
  animation::clearKeyframes(data_->animation);
  invalidateGroupStateCache(data_->group_state_cache);
}

ArxReturnCode Animation::claimGroup(std::size_t group) noexcept {
  if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
  animation::claimGroup(data_->animation, group);
  return ARX_OK;
}

ArxReturnCode Animation::unclaimGroup(std::size_t group) noexcept {
  if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
  animation::unclaimGroup(data_->animation, group);
  return ARX_OK;
}

ArxReturnCode Animation::voidGroup(std::size_t group) noexcept {
  if (group >= data_->animation.group_count) return ARX_INDEX_OUT_OF_RANGE;
  animation::voidGroup(data_->animation, group);
  cacheGroupIdentity(data_->group_state_cache, group, true);
  return ARX_OK;
}

ArxReturnCode Animation::compactSounds(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (removed) *removed = 0;
    std::vector<std::uint8_t> used(soundCount(), 0);
    for (const AnimationKeyframe& keyframe : data_->animation.keyframes) {
      if (keyframe.sound == kNoSoundHandle) continue;
      SoundIndex index = kNoSound;
      if (soundHandleIndex(keyframe.sound, index) != ARX_OK || index >= used.size())
        return ARX_ANIMATION_BAD_KEYFRAME_SOUND;
      used[index] = 1;
    }
    std::vector<SoundIndex> remap;
    std::size_t removed_count = 0;
    const ArxReturnCode rc = animation_detail::soundErrorCode(
        sounds::compact(data_->sounds, SoundKind::kEffect, used, remap, removed_count));
    if (rc != ARX_OK) return rc;
    for (AnimationKeyframe& keyframe : data_->animation.keyframes) {
      if (keyframe.sound == kNoSoundHandle) continue;
      SoundIndex index = kNoSound;
      if (soundHandleIndex(keyframe.sound, index) != ARX_OK || index >= remap.size() || remap[index] == kNoSound)
        return ARX_INTERNAL_ERROR;
      keyframe.sound = sounds::effectHandle(remap[index]);
    }
    if (removed) *removed = removed_count;
    return ARX_OK;
  });
}

ArxReturnCode Animation::rebaseSoundPaths(std::string_view directory) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    sounds::PathRebaseInfo info;
    const ArxReturnCode rc = animation_detail::soundErrorCode(sounds::rebasePaths(data_->sounds, directory, &info));
    if (rc != ARX_OK) return rc;
    for (const sounds::PathRebaseInfo::Repair& repair : info.repairs)
      log(ARX_LOG_WARN, "Animation sound rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Animation::setSound(SoundIndex index, const ArxSoundView& sound) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
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
  });
}

ArxReturnCode Animation::addSound(const ArxSoundView& sound, SoundIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kNoSound;
    if (!validSoundView(sound)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc = animation_detail::soundErrorCode(sounds::validateSoundCount(soundCount() + 1U));
    if (rc != ARX_OK) return rc;
    Sound next = internalSound(sound);
    sounds::PathRepairInfo repairs;
    rc = animation_detail::soundErrorCode(sounds::repairPath(data_->sounds, next, kNoSound, &repairs));
    if (rc != ARX_OK) return rc;
    rc = animation_detail::soundErrorCode(sounds::validateSound(next));
    if (rc != ARX_OK) return rc;
    out_index = sounds::addSound(data_->sounds, std::move(next));
    for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
      log(ARX_LOG_WARN, "Animation sound path '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Animation::setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!encoded_audio.data && encoded_audio.size != 0) return ARX_INVALID_DATA_POINTER;
    if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
    std::vector<std::uint8_t> data;
    if (encoded_audio.size != 0) data.assign(encoded_audio.data, encoded_audio.data + encoded_audio.size);
    const ArxReturnCode rc = animation_detail::soundErrorCode(sounds::validateEncodedAudio(data));
    if (rc != ARX_OK) return rc;
    sounds::setEncodedAudio(data_->sounds, index, std::move(data));
    return ARX_OK;
  });
}

ArxReturnCode Animation::clearSoundData(SoundIndex index) noexcept {
  if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
  sounds::clearEncodedAudio(data_->sounds, index);
  return ARX_OK;
}

ArxReturnCode Animation::removeSound(SoundIndex index) noexcept {
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
}

}  // namespace pistoris
