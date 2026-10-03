// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "modules/ambiance.h"
#include "utils/log.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace pistoris::ambiance {
namespace {

bool spanFitsFloat(float first, float second) noexcept {
  const double span = std::abs(static_cast<double>(second) - static_cast<double>(first));
  return span <= std::numeric_limits<float>::max();
}

bool hasRandomMode(DynamicAutomationMode mode) noexcept {
  return mode == DynamicAutomationMode::kRandomStep || mode == DynamicAutomationMode::kRandomInterpolated;
}

bool hasInterpolation(DynamicAutomationMode mode) noexcept {
  return mode == DynamicAutomationMode::kInterpolated || mode == DynamicAutomationMode::kRandomInterpolated;
}

bool validDynamicMode(DynamicAutomationMode mode) noexcept {
  switch (mode) {
    case DynamicAutomationMode::kStep:
    case DynamicAutomationMode::kRandomStep:
    case DynamicAutomationMode::kInterpolated:
    case DynamicAutomationMode::kRandomInterpolated:
      return true;
  }
  return false;
}

Error validateCommon(const AmbianceKeyCommon& key) noexcept {
  if (key.play_count == 0) return Error::kBadPlayCount;
  if (key.delay_min_ms > key.delay_max_ms) return Error::kBadKeyTiming;
  Error error = validateAutomation(key.volume);
  if (error != Error::kNone) return error;
  return validateAutomation(key.pitch);
}

template <class Key>
Error validateKey(const Key& key) noexcept {
  Error error = validateCommon(key);
  if (error != Error::kNone) return error;

  if constexpr (std::is_same_v<Key, PannedAmbianceKey>) {
    return validateAutomation(key.pan);
  } else {
    error = validateAutomation(key.x);
    if (error != Error::kNone) return error;
    error = validateAutomation(key.y);
    if (error != Error::kNone) return error;
    return validateAutomation(key.z);
  }
}

}  // namespace

Error validateAutomation(const Automation& automation) noexcept {
  if (const auto* constant = std::get_if<ConstantAutomation>(&automation))
    return std::isfinite(constant->value) ? Error::kNone : Error::kBadAutomation;

  const auto* dynamic = std::get_if<DynamicAutomation>(&automation);
  if (!dynamic) return Error::kBadAutomation;
  if (!validDynamicMode(dynamic->mode)) return Error::kBadAutomation;
  if (!std::isfinite(dynamic->first) || !std::isfinite(dynamic->second) || dynamic->first == dynamic->second)
    return Error::kBadAutomation;
  if (hasRandomMode(dynamic->mode) && dynamic->first > dynamic->second) return Error::kBadAutomation;
  if ((hasRandomMode(dynamic->mode) || hasInterpolation(dynamic->mode)) &&
      !spanFitsFloat(dynamic->first, dynamic->second))
    return Error::kBadAutomation;
  if (hasInterpolation(dynamic->mode) && dynamic->interval_ms == 0) return Error::kBadAutomation;
  return Error::kNone;
}

Error validateKeyCount(std::size_t key_count) noexcept {
  return key_count != 0 && key_count <= static_cast<std::size_t>(UINT32_MAX) ? Error::kNone : Error::kBadKeyCount;
}

Error validateTrack(const AmbianceTrack& track, std::size_t sound_count) noexcept {
  SoundKind kind = SoundKind::kEffect;
  SoundIndex index = kNoSound;
  if (soundHandleKind(track.sound, kind) != ARX_OK || soundHandleIndex(track.sound, index) != ARX_OK ||
      kind != SoundKind::kEffect || static_cast<std::size_t>(index) >= sound_count) {
    log(ARX_LOG_DEBUG, "Ambiance validation: track references sound {} with sound count {}", track.sound, sound_count);
    return Error::kBadSound;
  }

  const auto validate_keys = [&track](const auto& keys, std::string_view kind) {
    Error error = validateKeyCount(keys.size());
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG, "Ambiance validation: sound {} {} key count {} is invalid", track.sound, kind, keys.size());
      return error;
    }
    for (std::size_t index = 0; index < keys.size(); ++index) {
      error = validateKey(keys[index]);
      if (error != Error::kNone) {
        log(ARX_LOG_DEBUG,
            "Ambiance validation: sound {} {} key {} is invalid: error {}",
            track.sound,
            kind,
            index,
            static_cast<int>(error));
        return error;
      }
    }
    return Error::kNone;
  };
  if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&track.keys))
    return validate_keys(*keys, "panned");
  if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&track.keys))
    return validate_keys(*keys, "positioned");
  log(ARX_LOG_DEBUG, "Ambiance validation: sound {} has invalid key storage", track.sound);
  return Error::kBadKeyCount;
}

Error validateTrackAppend(const AmbianceData& ambiance) noexcept {
  return ambiance.tracks.size() < static_cast<std::size_t>(kInvalidAmbianceTrackIndex) ? Error::kNone
                                                                                       : Error::kTooManyTracks;
}

Error validate(const AmbianceData& ambiance, std::size_t sound_count) noexcept {
  if (ambiance.tracks.empty()) return ambiance.master_track == 0 ? Error::kNoTracks : Error::kBadMasterTrack;
  if (ambiance.tracks.size() > static_cast<std::size_t>(kInvalidAmbianceTrackIndex)) return Error::kTooManyTracks;
  if (ambiance.master_track >= ambiance.tracks.size()) return Error::kBadMasterTrack;

  for (const AmbianceTrack& track : ambiance.tracks) {
    Error error = validateTrack(track, sound_count);
    if (error != Error::kNone) return error;
  }
  return Error::kNone;
}

}  // namespace pistoris::ambiance
