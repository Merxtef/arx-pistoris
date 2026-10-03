// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "ambiance/data.h"
#include "ambiance/internal.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "modules/ambiance.h"
#include "modules/sounds.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace pistoris {
namespace {

struct SoundDurationEntry {
  bool queried = false;
  ArxReturnCode status = ARX_OK;
  long double milliseconds = 0.0L;
};

ArxReturnCode soundDuration(const AmbianceModules& modules, SoundHandle handle, std::vector<SoundDurationEntry>& cache,
                            long double& out) noexcept {
  SoundIndex sound = kNoSound;
  if (!sounds::effectIndex(handle, sound) || static_cast<std::size_t>(sound) >= cache.size())
    return ARX_AMBIANCE_BAD_TRACK_SOUND;
  SoundDurationEntry& entry = cache[sound];
  if (!entry.queried) {
    entry.queried = true;
    const std::span<const std::uint8_t> encoded_audio = sounds::encodedAudio(modules.sounds, handle);
    if (encoded_audio.empty()) {
      entry.status = ARX_AMBIANCE_SOUND_DATA_REQUIRED;
    } else {
      sounds::AudioInfo info;
      entry.status = ambiance_detail::soundErrorCode(sounds::inspectEncodedAudio(encoded_audio, info));
      if (entry.status == ARX_OK)
        entry.milliseconds = static_cast<long double>(info.frame_count) * 1000.0L / info.sample_rate;
    }
  }
  if (entry.status == ARX_OK) out = entry.milliseconds;
  return entry.status;
}

}  // namespace

namespace ambiance_detail {

void warnAboutMasterTiming(const AmbianceModules& modules) noexcept {
  if (log_fn == nullptr || modules.ambiance.tracks.size() <= 1U) return;
  try {
    std::vector<SoundDurationEntry> cache(sounds::count(modules.sounds, SoundKind::kEffect));
    const AmbianceTrack& master = modules.ambiance.tracks[modules.ambiance.master_track];
    long double master_sample_ms = 0.0L;
    if (soundDuration(modules, master.sound, cache, master_sample_ms) != ARX_OK) return;
    const ambiance::TrackTimingBounds master_bounds = ambiance::trackTimingBounds(master, master_sample_ms);

    for (std::size_t index = 0; index < modules.ambiance.tracks.size(); ++index) {
      if (index == modules.ambiance.master_track) continue;
      const AmbianceTrack& track = modules.ambiance.tracks[index];
      long double sample_ms = 0.0L;
      if (soundDuration(modules, track.sound, cache, sample_ms) != ARX_OK) continue;
      const ambiance::TrackTimingBounds bounds = ambiance::trackTimingBounds(track, sample_ms);
      if (bounds.maximum_ms <= master_bounds.minimum_ms) continue;
      log(ARX_LOG_WARN,
          "Ambiance -> AMB: track {} sound '{}' maximum nominal duration {:.3f} s exceeds master track {} minimum "
          "{:.3f} s",
          index,
          sounds::path(modules.sounds, track.sound),
          static_cast<double>(bounds.maximum_ms / 1000.0L),
          modules.ambiance.master_track,
          static_cast<double>(master_bounds.minimum_ms / 1000.0L));
    }
  } catch (...) {
    return;
  }
}

}  // namespace ambiance_detail

AmbianceResult<std::size_t> Ambiance::trimTracksToMaster() noexcept {
  if (!data_)
    return api_detail::ambianceFailure<std::size_t>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
  return api_detail::ambianceBoundary(resourcePath(), [&]() -> AmbianceResult<std::size_t> {
    AmbianceModules& modules = static_cast<AmbianceModules&>(*data_);
    ArxReturnCode rc = ambiance_detail::validateStructure(modules);
    if (rc != ARX_OK)
      return api_detail::ambianceFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), AmbianceElement::kResource));
    if (modules.ambiance.tracks.size() <= 1U) return AmbianceResult<std::size_t>::success(0);

    std::vector<SoundDurationEntry> cache(sounds::count(modules.sounds, SoundKind::kEffect));
    const AmbianceTrack& master = modules.ambiance.tracks[modules.ambiance.master_track];
    long double master_sample_ms = 0.0L;
    rc = soundDuration(modules, master.sound, cache, master_sample_ms);
    if (rc != ARX_OK)
      return api_detail::ambianceFailure<std::size_t>(
          rc, api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, modules.ambiance.master_track));
    const long double maximum_duration_ms = ambiance::trackTimingBounds(master, master_sample_ms).minimum_ms;

    std::vector<ambiance::TrackTrimPlan> plans(modules.ambiance.tracks.size());
    for (std::size_t index = 0; index < modules.ambiance.tracks.size(); ++index) {
      if (index == modules.ambiance.master_track) continue;
      const AmbianceTrack& track = modules.ambiance.tracks[index];
      long double sample_ms = 0.0L;
      rc = soundDuration(modules, track.sound, cache, sample_ms);
      if (rc != ARX_OK)
        return api_detail::ambianceFailure<std::size_t>(
            rc, api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index));
      rc = ambiance_detail::errorCode(ambiance::planTrackTrim(track, sample_ms, maximum_duration_ms, plans[index]));
      if (rc != ARX_OK)
        return api_detail::ambianceFailure<std::size_t>(
            rc, api_detail::resourceLocation(resourcePath(), AmbianceElement::kTrack, index));
    }

    std::size_t trimmed = 0;
    for (std::size_t index = 0; index < modules.ambiance.tracks.size(); ++index) {
      if (index == modules.ambiance.master_track) continue;
      if (ambiance::applyTrackTrim(modules.ambiance.tracks[index], plans[index])) ++trimmed;
    }
    return AmbianceResult<std::size_t>::success(trimmed);
  });
}

}  // namespace pistoris
