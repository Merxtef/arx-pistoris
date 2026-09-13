// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"

#include "modules/ambiance.h"

#include <cassert>
#include <cstddef>
#include <utility>

namespace pistoris::ambiance {

void setTrack(AmbianceData& ambiance, AmbianceTrackIndex index, AmbianceTrack track) noexcept {
  assert(index < ambiance.tracks.size());
  ambiance.tracks[index] = std::move(track);
}

AmbianceTrackIndex addTrack(AmbianceData& ambiance, AmbianceTrack track) {
  assert(ambiance.tracks.size() < static_cast<std::size_t>(kInvalidAmbianceTrackIndex));
  ambiance.tracks.push_back(std::move(track));
  const AmbianceTrackIndex index = static_cast<AmbianceTrackIndex>(ambiance.tracks.size() - 1U);
  if (ambiance.tracks.size() == 1U) ambiance.master_track = 0;
  return index;
}

void removeTrack(AmbianceData& ambiance, AmbianceTrackIndex index) noexcept {
  assert(index < ambiance.tracks.size());
  const bool removed_master = ambiance.master_track == index;
  ambiance.tracks.erase(ambiance.tracks.begin() + index);
  if (ambiance.tracks.empty() || removed_master) {
    ambiance.master_track = 0;
  } else if (ambiance.master_track > index) {
    --ambiance.master_track;
  }
}

void clearTracks(AmbianceData& ambiance) noexcept {
  ambiance.tracks.clear();
  ambiance.master_track = 0;
}

void setMasterTrack(AmbianceData& ambiance, AmbianceTrackIndex index) noexcept {
  assert(index < ambiance.tracks.size());
  ambiance.master_track = index;
}

}  // namespace pistoris::ambiance
