// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"

#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "ambiance/data.h"
#include "ambiance/internal.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "modules/ambiance.h"
#include "modules/resource.h"
#include "modules/sounds.h"
#include "utils/log.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris {
namespace {

ArxReturnCode ambianceResourceError(resource::Error error) noexcept {
  switch (error) {
    case resource::Error::kNone:
      return ARX_OK;
    case resource::Error::kBadPath:
      return ARX_AMBIANCE_BAD_RESOURCE_PATH;
    case resource::Error::kBadKind:
      return ARX_INTERNAL_ERROR;
  }
  return ARX_INTERNAL_ERROR;
}

}  // namespace

namespace ambiance_detail {

ArxReturnCode soundErrorCode(sounds::Error error) noexcept {
  switch (error) {
    case sounds::Error::kNone:
      return ARX_OK;
    case sounds::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case sounds::Error::kTooManySounds:
      return ARX_AMBIANCE_TOO_MANY_SOUNDS;
    case sounds::Error::kBadPath:
      return ARX_AMBIANCE_BAD_SOUND_PATH;
    case sounds::Error::kBadAudio:
      return ARX_AMBIANCE_BAD_SOUND_DATA;
    case sounds::Error::kUnsupportedChannels:
      return ARX_AMBIANCE_UNSUPPORTED_SOUND_CHANNELS;
    case sounds::Error::kAudioTooLarge:
      return ARX_AMBIANCE_SOUND_TOO_LARGE;
    case sounds::Error::kDuplicatePath:
      return ARX_AMBIANCE_DUPLICATE_SOUND_PATH;
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

ArxReturnCode validateStructure(const AmbianceModules& modules) noexcept {
  ArxReturnCode rc = ambianceResourceError(resource::validate(modules.resource, ARX_RESOURCE_KIND_AMBIANCE));
  if (rc != ARX_OK) return rc;
  rc = soundErrorCode(sounds::validateStructure(modules.sounds));
  if (rc != ARX_OK) return rc;
  return errorCode(ambiance::validate(modules.ambiance, sounds::count(modules.sounds, SoundKind::kEffect)));
}

}  // namespace ambiance_detail

namespace {

template <class Input>
ArxReturnCode validateTrackInput(const Input& input) noexcept {
  if (!input.keys && input.key_count != 0) return ARX_INVALID_DATA_POINTER;
  return ambiance_detail::errorCode(ambiance::validateKeyCount(input.key_count));
}

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

template <class Input>
ArxReturnCode internalTrack(const Input& input, AmbianceTrack& out) {
  ArxReturnCode rc = validateTrackInput(input);
  if (rc != ARX_OK) return rc;
  return ambiance_detail::internalTrack(input, out) ? ARX_OK : ARX_AMBIANCE_BAD_AUTOMATION;
}

}  // namespace

namespace ambiance_detail {

ArxReturnCode errorCode(ambiance::Error error) noexcept {
  switch (error) {
    case ambiance::Error::kNone:
      return ARX_OK;
    case ambiance::Error::kNoTracks:
      return ARX_AMBIANCE_NO_TRACKS;
    case ambiance::Error::kTooManyTracks:
      return ARX_AMBIANCE_TOO_MANY_TRACKS;
    case ambiance::Error::kBadMasterTrack:
      return ARX_AMBIANCE_BAD_MASTER_TRACK;
    case ambiance::Error::kBadSound:
      return ARX_AMBIANCE_BAD_TRACK_SOUND;
    case ambiance::Error::kBadKeyCount:
      return ARX_AMBIANCE_BAD_KEY_COUNT;
    case ambiance::Error::kBadPlayCount:
      return ARX_AMBIANCE_BAD_PLAY_COUNT;
    case ambiance::Error::kBadKeyTiming:
      return ARX_AMBIANCE_BAD_KEY_TIMING;
    case ambiance::Error::kBadAutomation:
      return ARX_AMBIANCE_BAD_AUTOMATION;
    case ambiance::Error::kCannotFitDuration:
      return ARX_AMBIANCE_TRACK_CANNOT_FIT_MASTER;
    case ambiance::Error::kBadIndex:
      return ARX_INDEX_OUT_OF_RANGE;
  }
  return ARX_INTERNAL_ERROR;
}

bool internalAutomation(const ArxAmbianceAutomation& source, Automation& out) noexcept {
  if (source.mode == ARX_AMBIANCE_AUTOMATION_CONSTANT) {
    out = ConstantAutomation{source.first};
    return true;
  }

  DynamicAutomationMode mode;
  switch (source.mode) {
    case ARX_AMBIANCE_AUTOMATION_STEP:
      mode = DynamicAutomationMode::kStep;
      break;
    case ARX_AMBIANCE_AUTOMATION_RANDOM_STEP:
      mode = DynamicAutomationMode::kRandomStep;
      break;
    case ARX_AMBIANCE_AUTOMATION_INTERPOLATED:
      mode = DynamicAutomationMode::kInterpolated;
      break;
    case ARX_AMBIANCE_AUTOMATION_RANDOM_INTERPOLATED:
      mode = DynamicAutomationMode::kRandomInterpolated;
      break;
    default:
      return false;
  }
  out = DynamicAutomation{source.first, source.second, source.interval_ms, mode};
  return true;
}

ArxAmbianceAutomation publicAutomation(const Automation& source) noexcept {
  if (const auto* constant = std::get_if<ConstantAutomation>(&source))
    return {constant->value, constant->value, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT};

  const auto* dynamic = std::get_if<DynamicAutomation>(&source);
  if (!dynamic) return {};
  ArxAmbianceAutomationMode mode = ARX_AMBIANCE_AUTOMATION_STEP;
  switch (dynamic->mode) {
    case DynamicAutomationMode::kStep:
      mode = ARX_AMBIANCE_AUTOMATION_STEP;
      break;
    case DynamicAutomationMode::kRandomStep:
      mode = ARX_AMBIANCE_AUTOMATION_RANDOM_STEP;
      break;
    case DynamicAutomationMode::kInterpolated:
      mode = ARX_AMBIANCE_AUTOMATION_INTERPOLATED;
      break;
    case DynamicAutomationMode::kRandomInterpolated:
      mode = ARX_AMBIANCE_AUTOMATION_RANDOM_INTERPOLATED;
      break;
  }
  return {dynamic->first, dynamic->second, dynamic->interval_ms, mode};
}

bool internalTrack(const ArxAmbiancePannedTrackInput& source, AmbianceTrack& out) {
  if (source.key_count != 0 && !source.keys) return false;
  out.sound = sounds::effectHandle(source.sound);
  std::vector<PannedAmbianceKey> keys;
  keys.reserve(source.key_count);
  for (std::size_t index = 0; index < source.key_count; ++index) {
    const ArxAmbiancePannedKey& input = source.keys[index];
    PannedAmbianceKey key;
    key.start_delay_ms = input.start_delay_ms;
    key.play_count = input.play_count;
    key.delay_min_ms = input.delay_min_ms;
    key.delay_max_ms = input.delay_max_ms;
    if (!internalAutomation(input.volume, key.volume) || !internalAutomation(input.pitch, key.pitch) ||
        !internalAutomation(input.pan, key.pan))
      return false;
    keys.push_back(key);
  }
  out.keys = std::move(keys);
  return true;
}

bool internalTrack(const ArxAmbiancePositionedTrackInput& source, AmbianceTrack& out) {
  if (source.key_count != 0 && !source.keys) return false;
  out.sound = sounds::effectHandle(source.sound);
  std::vector<PositionedAmbianceKey> keys;
  keys.reserve(source.key_count);
  for (std::size_t index = 0; index < source.key_count; ++index) {
    const ArxAmbiancePositionedKey& input = source.keys[index];
    PositionedAmbianceKey key;
    key.start_delay_ms = input.start_delay_ms;
    key.play_count = input.play_count;
    key.delay_min_ms = input.delay_min_ms;
    key.delay_max_ms = input.delay_max_ms;
    if (!internalAutomation(input.volume, key.volume) || !internalAutomation(input.pitch, key.pitch) ||
        !internalAutomation(input.x, key.x) || !internalAutomation(input.y, key.y) ||
        !internalAutomation(input.z, key.z))
      return false;
    keys.push_back(key);
  }
  out.keys = std::move(keys);
  return true;
}

ArxAmbiancePannedKey publicKey(const PannedAmbianceKey& source) noexcept {
  return {
      source.start_delay_ms,
      source.play_count,
      source.delay_min_ms,
      source.delay_max_ms,
      publicAutomation(source.volume),
      publicAutomation(source.pitch),
      publicAutomation(source.pan),
  };
}

ArxAmbiancePositionedKey publicKey(const PositionedAmbianceKey& source) noexcept {
  return {
      source.start_delay_ms,
      source.play_count,
      source.delay_min_ms,
      source.delay_max_ms,
      publicAutomation(source.volume),
      publicAutomation(source.pitch),
      publicAutomation(source.x),
      publicAutomation(source.y),
      publicAutomation(source.z),
  };
}

}  // namespace ambiance_detail

Ambiance::Ambiance() : data_(std::make_unique<Data>()) {}

Ambiance::~Ambiance() = default;

Ambiance::Ambiance(const Ambiance& other) : data_(other.data_ ? std::make_unique<Data>(*other.data_) : nullptr) {}

Ambiance::Ambiance(Ambiance&& other) noexcept = default;

Ambiance& Ambiance::operator=(const Ambiance& other) {
  if (this == &other) return *this;
  Ambiance copy(other);
  swap(copy);
  return *this;
}

Ambiance& Ambiance::operator=(Ambiance&& other) noexcept = default;

void Ambiance::swap(Ambiance& other) noexcept { data_.swap(other.data_); }

AmbianceResult<void> Ambiance::reset() noexcept {
  return api_detail::ambianceBoundary(resourcePath(), [&]() -> AmbianceResult<void> {
    auto replacement = std::make_unique<Data>();
    data_.swap(replacement);
    return AmbianceResult<void>::success();
  });
}

AmbianceResult<void> Ambiance::validate() const noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource);
  if (!data_) return api_detail::ambianceFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::resourceValidationBoundary<AmbianceResult<void>>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        const ArxReturnCode rc = ambiance_detail::validateStructure(static_cast<const AmbianceModules&>(*data_));
        if (rc != ARX_OK) return rc;
        return ambiance_detail::soundErrorCode(sounds::validateAudio(data_->sounds));
      },
      location);
}

std::string_view Ambiance::resourcePath() const noexcept {
  return data_ ? std::string_view(data_->resource.path) : std::string_view{};
}

AmbianceResult<void> Ambiance::setResourcePath(std::string_view resource_path) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource);
  if (!data_) return api_detail::ambianceFailure<void>(ARX_INVALID_STATE, location);
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        std::string path;
        const ArxReturnCode rc =
            ambianceResourceError(resource::repairPath(ARX_RESOURCE_KIND_AMBIANCE, resource_path, path));
        if (rc != ARX_OK) return rc;
        resource::setPath(data_->resource, std::move(path));
        return ARX_OK;
      },
      location);
}

std::size_t Ambiance::trackCount() const noexcept { return data_ ? data_->ambiance.tracks.size() : 0; }

std::size_t Ambiance::soundCount() const noexcept {
  return data_ ? sounds::count(data_->sounds, SoundKind::kEffect) : 0;
}

AmbianceTrackIndex Ambiance::masterTrack() const noexcept {
  return data_ ? data_->ambiance.master_track : kInvalidAmbianceTrackIndex;
}

ArxAmbianceTrack Ambiance::trackAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Ambiance*>(owner);
  const AmbianceTrack& source = self.data_->ambiance.tracks[index];
  ArxAmbianceTrackKind kind = ARX_AMBIANCE_TRACK_PANNED;
  std::size_t key_count = 0;
  if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&source.keys)) {
    key_count = keys->size();
  } else if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&source.keys)) {
    kind = ARX_AMBIANCE_TRACK_POSITIONED;
    key_count = keys->size();
  }
  SoundIndex sound = kNoSound;
  const ArxReturnCode rc = soundHandleIndex(source.sound, sound);
  assert(rc == ARX_OK);
  (void)rc;
  return {sound, kind, key_count};
}

ArxSoundView Ambiance::soundAt(const void* owner, std::size_t, std::size_t index) noexcept {
  const auto& self = *static_cast<const Ambiance*>(owner);
  const SoundHandle handle = sounds::effectHandle(static_cast<SoundIndex>(index));
  const std::string_view source_path = sounds::path(self.data_->sounds, handle);
  const std::span<const std::uint8_t> source_audio = sounds::encodedAudio(self.data_->sounds, handle);
  return {borrowedString(source_path), {source_audio.data(), source_audio.size()}};
}

ArxAmbiancePannedKey Ambiance::pannedKeyAt(const void* owner, std::size_t track, std::size_t index) noexcept {
  const auto& self = *static_cast<const Ambiance*>(owner);
  const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&self.data_->ambiance.tracks[track].keys);
  assert(keys);
  return ambiance_detail::publicKey((*keys)[index]);
}

ArxAmbiancePositionedKey Ambiance::positionedKeyAt(const void* owner, std::size_t track, std::size_t index) noexcept {
  const auto& self = *static_cast<const Ambiance*>(owner);
  const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&self.data_->ambiance.tracks[track].keys);
  assert(keys);
  return ambiance_detail::publicKey((*keys)[index]);
}

Ambiance::TracksView Ambiance::tracks() const noexcept {
  return data_ ? TracksView(this, 0, trackCount(), &Ambiance::trackAt) : TracksView{};
}

Ambiance::SoundsView Ambiance::sounds() const noexcept {
  return data_ ? SoundsView(this, 0, soundCount(), &Ambiance::soundAt) : SoundsView{};
}

AmbianceResult<Ambiance::PannedKeysView> Ambiance::pannedKeys(AmbianceTrackIndex track) const noexcept {
  if (!data_)
    return api_detail::ambianceFailure<PannedKeysView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, track);
  if (track >= trackCount()) return api_detail::ambianceFailure<PannedKeysView>(ARX_INDEX_OUT_OF_RANGE, location);
  const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&data_->ambiance.tracks[track].keys);
  if (!keys) return api_detail::ambianceFailure<PannedKeysView>(ARX_INVALID_OPTIONS, location);
  return AmbianceResult<PannedKeysView>::success(PannedKeysView(this, track, keys->size(), &Ambiance::pannedKeyAt));
}

AmbianceResult<Ambiance::PositionedKeysView> Ambiance::positionedKeys(AmbianceTrackIndex track) const noexcept {
  if (!data_)
    return api_detail::ambianceFailure<PositionedKeysView>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, track);
  if (track >= trackCount()) return api_detail::ambianceFailure<PositionedKeysView>(ARX_INDEX_OUT_OF_RANGE, location);
  const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&data_->ambiance.tracks[track].keys);
  if (!keys) return api_detail::ambianceFailure<PositionedKeysView>(ARX_INVALID_OPTIONS, location);
  return AmbianceResult<PositionedKeysView>::success(
      PositionedKeysView(this, track, keys->size(), &Ambiance::positionedKeyAt));
}

AmbianceResult<void> Ambiance::setPannedTrack(AmbianceTrackIndex index,
                                              const ArxAmbiancePannedTrackInput& track) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
        AmbianceTrack value;
        ArxReturnCode rc = internalTrack(track, value);
        if (rc != ARX_OK) return rc;
        rc = ambiance_detail::errorCode(ambiance::validateTrack(value, soundCount()));
        if (rc != ARX_OK) return rc;
        ambiance::setTrack(data_->ambiance, index, std::move(value));
        return ARX_OK;
      },
      location);
}

AmbianceResult<AmbianceTrackIndex> Ambiance::addPannedTrack(const ArxAmbiancePannedTrackInput& track) noexcept {
  if (!data_)
    return api_detail::ambianceFailure<AmbianceTrackIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  const AmbianceLocation location =
      api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, data_->ambiance.tracks.size());
  return api_detail::ambianceBoundary(resourcePath(), [&]() -> AmbianceResult<AmbianceTrackIndex> {
    ArxReturnCode rc = ambiance_detail::errorCode(ambiance::validateTrackAppend(data_->ambiance));
    if (rc != ARX_OK) return api_detail::ambianceFailure<AmbianceTrackIndex>(rc, location);
    AmbianceTrack value;
    rc = internalTrack(track, value);
    if (rc != ARX_OK) return api_detail::ambianceFailure<AmbianceTrackIndex>(rc, location);
    rc = ambiance_detail::errorCode(ambiance::validateTrack(value, soundCount()));
    if (rc != ARX_OK) return api_detail::ambianceFailure<AmbianceTrackIndex>(rc, location);
    return AmbianceResult<AmbianceTrackIndex>::success(ambiance::addTrack(data_->ambiance, std::move(value)));
  });
}

AmbianceResult<void> Ambiance::setPositionedTrack(AmbianceTrackIndex index,
                                                  const ArxAmbiancePositionedTrackInput& track) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
        AmbianceTrack value;
        ArxReturnCode rc = internalTrack(track, value);
        if (rc != ARX_OK) return rc;
        rc = ambiance_detail::errorCode(ambiance::validateTrack(value, soundCount()));
        if (rc != ARX_OK) return rc;
        ambiance::setTrack(data_->ambiance, index, std::move(value));
        return ARX_OK;
      },
      location);
}

AmbianceResult<AmbianceTrackIndex> Ambiance::addPositionedTrack(const ArxAmbiancePositionedTrackInput& track) noexcept {
  if (!data_)
    return api_detail::ambianceFailure<AmbianceTrackIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  const AmbianceLocation location =
      api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, data_->ambiance.tracks.size());
  return api_detail::ambianceBoundary(resourcePath(), [&]() -> AmbianceResult<AmbianceTrackIndex> {
    ArxReturnCode rc = ambiance_detail::errorCode(ambiance::validateTrackAppend(data_->ambiance));
    if (rc != ARX_OK) return api_detail::ambianceFailure<AmbianceTrackIndex>(rc, location);
    AmbianceTrack value;
    rc = internalTrack(track, value);
    if (rc != ARX_OK) return api_detail::ambianceFailure<AmbianceTrackIndex>(rc, location);
    rc = ambiance_detail::errorCode(ambiance::validateTrack(value, soundCount()));
    if (rc != ARX_OK) return api_detail::ambianceFailure<AmbianceTrackIndex>(rc, location);
    return AmbianceResult<AmbianceTrackIndex>::success(ambiance::addTrack(data_->ambiance, std::move(value)));
  });
}

AmbianceResult<void> Ambiance::removeTrack(AmbianceTrackIndex index) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
        ambiance::removeTrack(data_->ambiance, index);
        return ARX_OK;
      },
      location);
}

void Ambiance::clearTracks() noexcept {
  if (data_) ambiance::clearTracks(data_->ambiance);
}

AmbianceResult<void> Ambiance::setMasterTrack(AmbianceTrackIndex index) noexcept {
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
        ambiance::setMasterTrack(data_->ambiance, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index));
}

AmbianceResult<std::size_t> Ambiance::compactSounds() noexcept {
  if (!data_)
    return api_detail::ambianceFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceBoundary(resourcePath(), [&]() -> AmbianceResult<std::size_t> {
    std::vector<std::uint8_t> used(soundCount(), 0);
    for (std::size_t index = 0; index < data_->ambiance.tracks.size(); ++index) {
      const AmbianceTrack& track = data_->ambiance.tracks[index];
      SoundIndex sound = kNoSound;
      if (soundHandleIndex(track.sound, sound) != ARX_OK || sound >= used.size())
        return api_detail::ambianceFailure<std::size_t>(
            ARX_AMBIANCE_BAD_TRACK_SOUND, api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index));
      used[sound] = 1;
    }
    std::vector<SoundIndex> remap;
    std::size_t removed_count = 0;
    const ArxReturnCode rc =
        ambiance_detail::soundErrorCode(sounds::compact(data_->sounds, SoundKind::kEffect, used, remap, removed_count));
    if (rc != ARX_OK)
      return api_detail::ambianceFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound));
    for (std::size_t index = 0; index < data_->ambiance.tracks.size(); ++index) {
      AmbianceTrack& track = data_->ambiance.tracks[index];
      SoundIndex sound = kNoSound;
      if (soundHandleIndex(track.sound, sound) != ARX_OK || sound >= remap.size() || remap[sound] == kNoSound)
        return api_detail::ambianceFailure<std::size_t>(
            ARX_INTERNAL_ERROR, api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index));
      track.sound = sounds::effectHandle(remap[sound]);
    }
    return AmbianceResult<std::size_t>::success(removed_count);
  });
}

AmbianceResult<void> Ambiance::rebaseSoundPaths(std::string_view directory) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        sounds::PathRebaseInfo info;
        const ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::rebasePaths(data_->sounds, directory, &info));
        if (rc != ARX_OK) return rc;
        for (const sounds::PathRebaseInfo::Repair& repair : info.repairs)
          log(ARX_LOG_WARN, "Ambiance sound rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      location);
}

AmbianceResult<void> Ambiance::setSound(SoundIndex index, const ArxSoundView& sound) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound, index);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!validSoundView(sound)) return ARX_INVALID_DATA_POINTER;
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        Sound next = internalSound(sound);
        sounds::PathRepairInfo repairs;
        ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::repairPath(data_->sounds, next, index, &repairs));
        if (rc != ARX_OK) return rc;
        rc = ambiance_detail::soundErrorCode(sounds::validateSound(next));
        if (rc != ARX_OK) return rc;
        sounds::setSound(data_->sounds, index, std::move(next));
        for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
          log(ARX_LOG_WARN, "Ambiance sound path '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      location);
}

AmbianceResult<SoundIndex> Ambiance::addSound(const ArxSoundView& sound) noexcept {
  if (!data_)
    return api_detail::ambianceFailure<SoundIndex>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound, soundCount());
  return api_detail::ambianceBoundary(resourcePath(), [&]() -> AmbianceResult<SoundIndex> {
    if (!validSoundView(sound)) return api_detail::ambianceFailure<SoundIndex>(ARX_INVALID_DATA_POINTER, location);
    ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::validateSoundCount(soundCount() + 1U));
    if (rc != ARX_OK) return api_detail::ambianceFailure<SoundIndex>(rc, location);
    Sound next = internalSound(sound);
    sounds::PathRepairInfo repairs;
    rc = ambiance_detail::soundErrorCode(sounds::repairPath(data_->sounds, next, kNoSound, &repairs));
    if (rc != ARX_OK) return api_detail::ambianceFailure<SoundIndex>(rc, location);
    rc = ambiance_detail::soundErrorCode(sounds::validateSound(next));
    if (rc != ARX_OK) return api_detail::ambianceFailure<SoundIndex>(rc, location);
    const SoundIndex index = sounds::addSound(data_->sounds, std::move(next));
    for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
      log(ARX_LOG_WARN, "Ambiance sound path '{}' normalized to '{}'", repair.original, repair.repaired);
    return AmbianceResult<SoundIndex>::success(index);
  });
}

AmbianceResult<void> Ambiance::setSoundPath(SoundIndex index, std::string_view requested) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound, index);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        Sound candidate{std::string(requested), {}};
        sounds::PathRepairInfo repairs;
        ArxReturnCode rc =
            ambiance_detail::soundErrorCode(sounds::repairPath(data_->sounds, candidate, index, &repairs));
        if (rc != ARX_OK) return rc;
        rc = ambiance_detail::soundErrorCode(sounds::validateSound(candidate));
        if (rc != ARX_OK) return rc;
        sounds::setPath(data_->sounds, index, std::move(candidate.path));
        for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
          log(ARX_LOG_WARN, "Ambiance sound path '{}' normalized to '{}'", repair.original, repair.repaired);
        return ARX_OK;
      },
      location);
}

AmbianceResult<void> Ambiance::setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept {
  const AmbianceLocation location = api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound, index);
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (!encoded_audio.data && encoded_audio.size != 0) return ARX_INVALID_DATA_POINTER;
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        std::vector<std::uint8_t> data;
        if (encoded_audio.size != 0) data.assign(encoded_audio.data, encoded_audio.data + encoded_audio.size);
        const ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::validateEncodedAudio(data));
        if (rc != ARX_OK) return rc;
        sounds::setEncodedAudio(data_->sounds, index, std::move(data));
        return ARX_OK;
      },
      location);
}

AmbianceResult<void> Ambiance::clearSoundData(SoundIndex index) noexcept {
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (static_cast<std::size_t>(index) >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        sounds::clearEncodedAudio(data_->sounds, index);
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound, index));
}

AmbianceResult<void> Ambiance::removeSound(SoundIndex index) noexcept {
  if (!data_)
    return api_detail::ambianceFailure<void>(ARX_INVALID_STATE,
                                             api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceStatusBoundary<void>(
      resourcePath(),
      [&]() -> ArxReturnCode {
        if (index >= soundCount()) return ARX_INDEX_OUT_OF_RANGE;
        const SoundHandle removed_handle = sounds::effectHandle(index);
        if (std::ranges::any_of(data_->ambiance.tracks,
                                [removed_handle](const AmbianceTrack& track) { return track.sound == removed_handle; }))
          return ARX_AMBIANCE_SOUND_IN_USE;
        sounds::removeSound(data_->sounds, index);
        for (AmbianceTrack& track : data_->ambiance.tracks) {
          SoundIndex current = kNoSound;
          if (soundHandleIndex(track.sound, current) == ARX_OK && current > index)
            track.sound = sounds::effectHandle(current - 1U);
        }
        return ARX_OK;
      },
      api_detail::resourceLocation(resourcePath(), AmbianceElement::kSound, index));
}

}  // namespace pistoris
