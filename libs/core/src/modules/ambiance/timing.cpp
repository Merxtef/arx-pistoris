// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/ambiance.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris::ambiance {
namespace {

constexpr long double kMinimumPitch = 0.1L;
constexpr long double kMaximumPitch = 2.0L;

std::pair<long double, long double> pitchBounds(const Automation& pitch) noexcept {
  if (const auto* constant = std::get_if<ConstantAutomation>(&pitch)) {
    const long double value = std::clamp(static_cast<long double>(constant->value), kMinimumPitch, kMaximumPitch);
    return {value, value};
  }

  const auto* dynamic = std::get_if<DynamicAutomation>(&pitch);
  assert(dynamic != nullptr);
  if (dynamic == nullptr) return {kMinimumPitch, kMinimumPitch};
  if (dynamic->mode == DynamicAutomationMode::kInterpolated ||
      dynamic->mode == DynamicAutomationMode::kRandomInterpolated)
    return {kMinimumPitch, kMaximumPitch};

  const long double first = std::clamp(static_cast<long double>(dynamic->first), kMinimumPitch, kMaximumPitch);
  const long double second = std::clamp(static_cast<long double>(dynamic->second), kMinimumPitch, kMaximumPitch);
  return std::minmax(first, second);
}

TrackTimingBounds keyTimingBounds(const AmbianceKeyCommon& key, long double sample_duration_ms) noexcept {
  const auto [minimum_pitch, maximum_pitch] = pitchBounds(key.pitch);
  const long double count = key.play_count;
  return {
      static_cast<long double>(key.start_delay_ms) +
          count * (static_cast<long double>(key.delay_min_ms) + sample_duration_ms / maximum_pitch),
      static_cast<long double>(key.start_delay_ms) +
          count * (static_cast<long double>(key.delay_max_ms) + sample_duration_ms / minimum_pitch),
  };
}

template <class Keys>
TrackTimingBounds trackTimingBounds(const Keys& keys, long double sample_duration_ms) noexcept {
  TrackTimingBounds result;
  for (const auto& key : keys) {
    const TrackTimingBounds bounds = keyTimingBounds(key, sample_duration_ms);
    result.minimum_ms += bounds.minimum_ms;
    result.maximum_ms += bounds.maximum_ms;
  }
  return result;
}

template <class Keys>
Error planTrackTrim(const Keys& keys, long double sample_duration_ms, long double maximum_duration_ms,
                    TrackTrimPlan& out) noexcept {
  TrackTrimPlan plan;
  long double elapsed_ms = 0.0L;
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const auto& key = keys[index];
    const long double minimum_pitch = pitchBounds(key.pitch).first;
    const long double play_duration_ms =
        static_cast<long double>(key.delay_max_ms) + sample_duration_ms / minimum_pitch;
    const long double start_ms = elapsed_ms + static_cast<long double>(key.start_delay_ms);
    const long double full_duration_ms = start_ms + static_cast<long double>(key.play_count) * play_duration_ms;
    if (full_duration_ms <= maximum_duration_ms) {
      elapsed_ms = full_duration_ms;
      plan.retained_key_count = index + 1U;
      plan.final_play_count = key.play_count;
      continue;
    }

    if (start_ms < maximum_duration_ms) {
      const long double available_ms = maximum_duration_ms - start_ms;
      std::uint32_t retained = static_cast<std::uint32_t>(available_ms / play_duration_ms);
      retained = std::min(retained, key.play_count);
      while (retained != 0 && start_ms + static_cast<long double>(retained) * play_duration_ms > maximum_duration_ms)
        --retained;
      while (retained < key.play_count &&
             start_ms + static_cast<long double>(retained + 1U) * play_duration_ms <= maximum_duration_ms)
        ++retained;
      if (retained != 0) {
        plan.retained_key_count = index + 1U;
        plan.final_play_count = retained;
      }
    }

    if (plan.retained_key_count == 0) return Error::kCannotFitDuration;
    out = plan;
    return Error::kNone;
  }

  assert(plan.retained_key_count != 0);
  out = plan;
  return Error::kNone;
}

template <class Keys>
bool applyTrackTrim(Keys& keys, const TrackTrimPlan& plan) noexcept {
  assert(plan.retained_key_count != 0 && plan.retained_key_count <= keys.size());
  assert(plan.final_play_count != 0 && plan.final_play_count <= keys[plan.retained_key_count - 1U].play_count);
  const bool changed =
      plan.retained_key_count != keys.size() || plan.final_play_count != keys[plan.retained_key_count - 1U].play_count;
  while (keys.size() > plan.retained_key_count) keys.pop_back();
  keys.back().play_count = plan.final_play_count;
  return changed;
}

}  // namespace

TrackTimingBounds trackTimingBounds(const AmbianceTrack& track, long double sample_duration_ms) noexcept {
  assert(sample_duration_ms > 0.0L && std::isfinite(sample_duration_ms));
  if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&track.keys))
    return trackTimingBounds(*keys, sample_duration_ms);
  if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&track.keys))
    return trackTimingBounds(*keys, sample_duration_ms);
  assert(false && "invalid ambiance key storage");
  return {};
}

Error planTrackTrim(const AmbianceTrack& track, long double sample_duration_ms, long double maximum_duration_ms,
                    TrackTrimPlan& out) noexcept {
  assert(sample_duration_ms > 0.0L && std::isfinite(sample_duration_ms));
  assert(maximum_duration_ms >= 0.0L && std::isfinite(maximum_duration_ms));
  if (const auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&track.keys))
    return planTrackTrim(*keys, sample_duration_ms, maximum_duration_ms, out);
  if (const auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&track.keys))
    return planTrackTrim(*keys, sample_duration_ms, maximum_duration_ms, out);
  assert(false && "invalid ambiance key storage");
  return Error::kBadKeyCount;
}

bool applyTrackTrim(AmbianceTrack& track, const TrackTrimPlan& plan) noexcept {
  if (auto* keys = std::get_if<std::vector<PannedAmbianceKey>>(&track.keys)) return applyTrackTrim(*keys, plan);
  if (auto* keys = std::get_if<std::vector<PositionedAmbianceKey>>(&track.keys)) return applyTrackTrim(*keys, plan);
  assert(false && "invalid ambiance key storage");
  return false;
}

}  // namespace pistoris::ambiance
