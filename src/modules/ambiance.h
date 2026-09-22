// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/sound.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace pistoris {

struct ConstantAutomation {
  float value = 0.0f;
};

enum class DynamicAutomationMode : std::uint8_t {
  kStep,
  kRandomStep,
  kInterpolated,
  kRandomInterpolated,
};

struct DynamicAutomation {
  float first = 0.0f;
  float second = 0.0f;
  std::uint32_t interval_ms = 0;
  DynamicAutomationMode mode = DynamicAutomationMode::kStep;
};

using Automation = std::variant<ConstantAutomation, DynamicAutomation>;

struct AmbianceKeyCommon {
  std::uint32_t start_delay_ms = 0;
  std::uint32_t play_count = 1;
  std::uint32_t delay_min_ms = 0;
  std::uint32_t delay_max_ms = 0;
  Automation volume;
  Automation pitch;
};

struct PannedAmbianceKey : AmbianceKeyCommon {
  Automation pan;
};

struct PositionedAmbianceKey : AmbianceKeyCommon {
  Automation x;
  Automation y;
  Automation z;
};

using AmbianceTrackKeys = std::variant<std::vector<PannedAmbianceKey>, std::vector<PositionedAmbianceKey>>;

struct AmbianceTrack {
  SoundHandle sound = kNoSoundHandle;
  AmbianceTrackKeys keys;
};

struct AmbianceData {
  std::vector<AmbianceTrack> tracks;
  AmbianceTrackIndex master_track = 0;
};

namespace ambiance {

enum class Error : std::uint8_t {
  kNone,
  kNoTracks,
  kTooManyTracks,
  kBadMasterTrack,
  kBadSound,
  kBadKeyCount,
  kBadPlayCount,
  kBadKeyTiming,
  kBadAutomation,
  kCannotFitDuration,
  kBadIndex,
};

struct TrackTimingBounds {
  long double minimum_ms = 0.0L;
  long double maximum_ms = 0.0L;
};

struct TrackTrimPlan {
  std::size_t retained_key_count = 0;
  std::uint32_t final_play_count = 0;
};

// --- Validation ---

Error validateAutomation(const Automation& automation) noexcept;
Error validateKeyCount(std::size_t key_count) noexcept;
Error validateTrack(const AmbianceTrack& track, std::size_t sound_count) noexcept;
Error validateTrackAppend(const AmbianceData& ambiance) noexcept;
Error validate(const AmbianceData& ambiance, std::size_t sound_count) noexcept;

// --- Queries ---

TrackTimingBounds trackTimingBounds(const AmbianceTrack& track, long double sample_duration_ms) noexcept;

// --- Mutation ---

void setTrack(AmbianceData& ambiance, AmbianceTrackIndex index, AmbianceTrack track) noexcept;
AmbianceTrackIndex addTrack(AmbianceData& ambiance, AmbianceTrack track);
void removeTrack(AmbianceData& ambiance, AmbianceTrackIndex index) noexcept;
void clearTracks(AmbianceData& ambiance) noexcept;
void setMasterTrack(AmbianceData& ambiance, AmbianceTrackIndex index) noexcept;

// --- Repair ---

Error planTrackTrim(const AmbianceTrack& track, long double sample_duration_ms, long double maximum_duration_ms,
                    TrackTrimPlan& out) noexcept;
bool applyTrackTrim(AmbianceTrack& track, const TrackTrimPlan& plan) noexcept;

}  // namespace ambiance
}  // namespace pistoris
