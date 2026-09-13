// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"

#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"

#include "ambiance/data.h"
#include "ambiance/internal.h"
#include "api/status_boundary.h"
#include "modules/ambiance.h"
#include "modules/resource.h"
#include "modules/sounds.h"
#include "utils/log.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris {
namespace {

template <class T>
ArxReturnCode validateCopyRange(std::size_t size, std::size_t offset, std::size_t count, T* out) noexcept {
  if (offset > size || count > size - offset) return ARX_INDEX_OUT_OF_RANGE;
  if (count != 0 && out == nullptr) return ARX_INVALID_DATA_POINTER;
  return ARX_OK;
}

ArxStringView borrowedString(const std::string& value) noexcept { return {value.data(), value.size()}; }

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
  rc = soundErrorCode(sounds::validateStructure(modules.sounds.sounds));
  if (rc != ARX_OK) return rc;
  return errorCode(ambiance::validate(modules.ambiance, modules.sounds.sounds.size()));
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
  out.sound = source.sound;
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
  out.sound = source.sound;
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

Ambiance::Ambiance(const Ambiance& other) : data_(std::make_unique<Data>(*other.data_)) {}

Ambiance& Ambiance::operator=(const Ambiance& other) {
  if (this == &other) return *this;
  Ambiance copy(other);
  swap(copy);
  return *this;
}

void Ambiance::swap(Ambiance& other) noexcept { data_.swap(other.data_); }

void Ambiance::reset() { data_ = std::make_unique<Data>(); }

ArxReturnCode Ambiance::validate() const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    const ArxReturnCode rc = ambiance_detail::validateStructure(static_cast<const AmbianceModules&>(*data_));
    if (rc != ARX_OK) return rc;
    return ambiance_detail::soundErrorCode(sounds::validateAudio(data_->sounds.sounds));
  });
}

std::string_view Ambiance::resourcePath() const noexcept { return data_->resource.path; }

ArxReturnCode Ambiance::setResourcePath(std::string_view resource_path) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    std::string path;
    const ArxReturnCode rc =
        ambianceResourceError(resource::repairPath(ARX_RESOURCE_KIND_AMBIANCE, resource_path, path));
    if (rc != ARX_OK) return rc;
    resource::setPath(data_->resource, std::move(path));
    return ARX_OK;
  });
}

std::size_t Ambiance::trackCount() const noexcept { return data_->ambiance.tracks.size(); }

std::size_t Ambiance::soundCount() const noexcept { return data_->sounds.sounds.size(); }

AmbianceTrackIndex Ambiance::masterTrack() const noexcept { return data_->ambiance.master_track; }

ArxReturnCode Ambiance::copyTracks(std::size_t offset, std::size_t count, ArxAmbianceTrack* out_tracks) const noexcept {
  ArxReturnCode rc = validateCopyRange(data_->ambiance.tracks.size(), offset, count, out_tracks);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    const AmbianceTrack& source = data_->ambiance.tracks[offset + index];
    ArxAmbianceTrackKind kind = ARX_AMBIANCE_TRACK_PANNED;
    std::size_t key_count = 0;
    if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&source.keys)) {
      key_count = keys->size();
    } else if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&source.keys)) {
      kind = ARX_AMBIANCE_TRACK_POSITIONED;
      key_count = keys->size();
    } else {
      return ARX_INTERNAL_ERROR;
    }
    out_tracks[index] = {
        source.sound,
        kind,
        key_count,
    };
  }
  return ARX_OK;
}

ArxReturnCode Ambiance::copySoundViews(std::size_t offset, std::size_t count, ArxSoundView* out_sounds) const noexcept {
  const ArxReturnCode rc = validateCopyRange(data_->sounds.sounds.size(), offset, count, out_sounds);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index) {
    const Sound& source = data_->sounds.sounds[offset + index];
    out_sounds[index] = {borrowedString(source.path), {source.encoded_audio.data(), source.encoded_audio.size()}};
  }
  return ARX_OK;
}

ArxReturnCode Ambiance::copyPannedKeys(AmbianceTrackIndex track, std::size_t offset, std::size_t count,
                                       ArxAmbiancePannedKey* out_keys) const noexcept {
  if (track >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
  const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&data_->ambiance.tracks[track].keys);
  if (!keys) return ARX_INVALID_OPTIONS;
  ArxReturnCode rc = validateCopyRange(keys->size(), offset, count, out_keys);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index)
    out_keys[index] = ambiance_detail::publicKey((*keys)[offset + index]);
  return ARX_OK;
}

ArxReturnCode Ambiance::copyPositionedKeys(AmbianceTrackIndex track, std::size_t offset, std::size_t count,
                                           ArxAmbiancePositionedKey* out_keys) const noexcept {
  if (track >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
  const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&data_->ambiance.tracks[track].keys);
  if (!keys) return ARX_INVALID_OPTIONS;
  ArxReturnCode rc = validateCopyRange(keys->size(), offset, count, out_keys);
  if (rc != ARX_OK) return rc;
  for (std::size_t index = 0; index < count; ++index)
    out_keys[index] = ambiance_detail::publicKey((*keys)[offset + index]);
  return ARX_OK;
}

ArxReturnCode Ambiance::setPannedTrack(AmbianceTrackIndex index, const ArxAmbiancePannedTrackInput& track) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
    AmbianceTrack value;
    ArxReturnCode rc = internalTrack(track, value);
    if (rc != ARX_OK) return rc;
    rc = ambiance_detail::errorCode(ambiance::validateTrack(value, data_->sounds.sounds.size()));
    if (rc != ARX_OK) return rc;
    ambiance::setTrack(data_->ambiance, index, std::move(value));
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::addPannedTrack(const ArxAmbiancePannedTrackInput& track,
                                       AmbianceTrackIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidAmbianceTrackIndex;
    ArxReturnCode rc = ambiance_detail::errorCode(ambiance::validateTrackAppend(data_->ambiance));
    if (rc != ARX_OK) return rc;
    AmbianceTrack value;
    rc = internalTrack(track, value);
    if (rc != ARX_OK) return rc;
    rc = ambiance_detail::errorCode(ambiance::validateTrack(value, data_->sounds.sounds.size()));
    if (rc != ARX_OK) return rc;
    out_index = ambiance::addTrack(data_->ambiance, std::move(value));
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::setPositionedTrack(AmbianceTrackIndex index,
                                           const ArxAmbiancePositionedTrackInput& track) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
    AmbianceTrack value;
    ArxReturnCode rc = internalTrack(track, value);
    if (rc != ARX_OK) return rc;
    rc = ambiance_detail::errorCode(ambiance::validateTrack(value, data_->sounds.sounds.size()));
    if (rc != ARX_OK) return rc;
    ambiance::setTrack(data_->ambiance, index, std::move(value));
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::addPositionedTrack(const ArxAmbiancePositionedTrackInput& track,
                                           AmbianceTrackIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kInvalidAmbianceTrackIndex;
    ArxReturnCode rc = ambiance_detail::errorCode(ambiance::validateTrackAppend(data_->ambiance));
    if (rc != ARX_OK) return rc;
    AmbianceTrack value;
    rc = internalTrack(track, value);
    if (rc != ARX_OK) return rc;
    rc = ambiance_detail::errorCode(ambiance::validateTrack(value, data_->sounds.sounds.size()));
    if (rc != ARX_OK) return rc;
    out_index = ambiance::addTrack(data_->ambiance, std::move(value));
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::removeTrack(AmbianceTrackIndex index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
    ambiance::removeTrack(data_->ambiance, index);
    return ARX_OK;
  });
}

void Ambiance::clearTracks() noexcept { ambiance::clearTracks(data_->ambiance); }

ArxReturnCode Ambiance::setMasterTrack(AmbianceTrackIndex index) noexcept {
  if (index >= data_->ambiance.tracks.size()) return ARX_INDEX_OUT_OF_RANGE;
  ambiance::setMasterTrack(data_->ambiance, index);
  return ARX_OK;
}

ArxReturnCode Ambiance::compactSounds(std::size_t* removed) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (removed) *removed = 0;
    std::vector<std::uint8_t> used(data_->sounds.sounds.size(), 0);
    for (const AmbianceTrack& track : data_->ambiance.tracks) {
      if (track.sound >= used.size()) return ARX_AMBIANCE_BAD_TRACK_SOUND;
      used[track.sound] = 1;
    }
    std::vector<SoundIndex> remap(used.size(), kNoSound);
    SoundIndex next = 0;
    for (std::size_t index = 0; index < used.size(); ++index)
      if (used[index] != 0) remap[index] = next++;

    std::size_t target = 0;
    for (std::size_t source = 0; source < used.size(); ++source) {
      if (used[source] == 0) continue;
      if (source != target) data_->sounds.sounds[target] = std::move(data_->sounds.sounds[source]);
      ++target;
    }
    data_->sounds.sounds.resize(target);
    for (AmbianceTrack& track : data_->ambiance.tracks) track.sound = remap[track.sound];
    if (removed) *removed = used.size() - target;
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::rebaseSoundPaths(std::string_view directory) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    sounds::PathRebaseInfo info;
    const ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::rebasePaths(data_->sounds, directory, &info));
    if (rc != ARX_OK) return rc;
    for (const sounds::PathRebaseInfo::Repair& repair : info.repairs)
      log(ARX_LOG_WARN, "Ambiance sound rebase: '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::setSound(SoundIndex index, const ArxSoundView& sound) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!validSoundView(sound)) return ARX_INVALID_DATA_POINTER;
    if (static_cast<std::size_t>(index) >= data_->sounds.sounds.size()) return ARX_INDEX_OUT_OF_RANGE;
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
  });
}

ArxReturnCode Ambiance::addSound(const ArxSoundView& sound, SoundIndex& out_index) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    out_index = kNoSound;
    if (!validSoundView(sound)) return ARX_INVALID_DATA_POINTER;
    ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::validateSoundCount(data_->sounds.sounds.size() + 1U));
    if (rc != ARX_OK) return rc;
    Sound next = internalSound(sound);
    sounds::PathRepairInfo repairs;
    rc = ambiance_detail::soundErrorCode(sounds::repairPath(data_->sounds, next, kNoSound, &repairs));
    if (rc != ARX_OK) return rc;
    rc = ambiance_detail::soundErrorCode(sounds::validateSound(next));
    if (rc != ARX_OK) return rc;
    out_index = sounds::addSound(data_->sounds, std::move(next));
    for (const sounds::PathRepairInfo::Repair& repair : repairs.repairs)
      log(ARX_LOG_WARN, "Ambiance sound path '{}' normalized to '{}'", repair.original, repair.repaired);
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::setSoundData(SoundIndex index, ArxEncodedAudioView encoded_audio) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    if (!encoded_audio.data && encoded_audio.size != 0) return ARX_INVALID_DATA_POINTER;
    if (static_cast<std::size_t>(index) >= data_->sounds.sounds.size()) return ARX_INDEX_OUT_OF_RANGE;
    std::vector<std::uint8_t> data;
    if (encoded_audio.size != 0) data.assign(encoded_audio.data, encoded_audio.data + encoded_audio.size);
    const ArxReturnCode rc = ambiance_detail::soundErrorCode(sounds::validateEncodedAudio(data));
    if (rc != ARX_OK) return rc;
    sounds::setEncodedAudio(data_->sounds, index, std::move(data));
    return ARX_OK;
  });
}

ArxReturnCode Ambiance::clearSoundData(SoundIndex index) noexcept {
  if (static_cast<std::size_t>(index) >= data_->sounds.sounds.size()) return ARX_INDEX_OUT_OF_RANGE;
  sounds::clearEncodedAudio(data_->sounds, index);
  return ARX_OK;
}

ArxReturnCode Ambiance::removeSound(SoundIndex index) noexcept {
  if (index >= data_->sounds.sounds.size()) return ARX_INDEX_OUT_OF_RANGE;
  if (std::ranges::any_of(data_->ambiance.tracks, [index](const AmbianceTrack& track) { return track.sound == index; }))
    return ARX_AMBIANCE_SOUND_IN_USE;
  sounds::removeSound(data_->sounds, index);
  for (AmbianceTrack& track : data_->ambiance.tracks)
    if (track.sound > index) --track.sound;
  return ARX_OK;
}

}  // namespace pistoris
