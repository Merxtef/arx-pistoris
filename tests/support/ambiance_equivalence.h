// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"

#include "support/sound_equivalence.h"

#include <algorithm>
#include <cstddef>

namespace test_support {
namespace ambiance_equivalence_detail {

inline void checkAutomationEqual(const ArxAmbianceAutomation& lhs, const ArxAmbianceAutomation& rhs) {
  CHECK(lhs.first == doctest::Approx(rhs.first).epsilon(1e-5));
  CHECK(lhs.second == doctest::Approx(rhs.second).epsilon(1e-5));
  CHECK(lhs.interval_ms == rhs.interval_ms);
  CHECK(lhs.mode == rhs.mode);
}

template <class Key>
void checkCommonKeyEqual(const Key& lhs, const Key& rhs) {
  CHECK(lhs.start_delay_ms == rhs.start_delay_ms);
  CHECK(lhs.play_count == rhs.play_count);
  CHECK(lhs.delay_min_ms == rhs.delay_min_ms);
  CHECK(lhs.delay_max_ms == rhs.delay_max_ms);
  checkAutomationEqual(lhs.volume, rhs.volume);
  checkAutomationEqual(lhs.pitch, rhs.pitch);
}

}  // namespace ambiance_equivalence_detail

struct AmbianceEquivalenceOptions {
  SoundEquivalenceOptions sounds;
};

inline void checkAmbiancesEquivalent(const pistoris::Ambiance& lhs, const pistoris::Ambiance& rhs,
                                     AmbianceEquivalenceOptions options = {}) {
  CHECK(lhs.resourcePath() == rhs.resourcePath());
  CHECK(lhs.trackCount() == rhs.trackCount());
  CHECK(lhs.masterTrack() == rhs.masterTrack());

  const auto lhs_tracks = lhs.tracks();
  const auto rhs_tracks = rhs.tracks();
  for (std::size_t track = 0; track < std::min(lhs_tracks.size(), rhs_tracks.size()); ++track) {
    const ArxAmbianceTrack lhs_track = lhs_tracks[track];
    const ArxAmbianceTrack rhs_track = rhs_tracks[track];
    CHECK(lhs_track.sound == rhs_track.sound);
    CHECK(lhs_track.kind == rhs_track.kind);
    CHECK(lhs_track.key_count == rhs_track.key_count);
    if (lhs_track.kind != rhs_track.kind) continue;

    if (lhs_track.kind == ARX_AMBIANCE_TRACK_PANNED) {
      const auto lhs_keys_result = lhs.pannedKeys(track);
      const auto rhs_keys_result = rhs.pannedKeys(track);
      CHECK(lhs_keys_result);
      CHECK(rhs_keys_result);
      if (!lhs_keys_result || !rhs_keys_result) continue;
      const auto lhs_keys = *lhs_keys_result;
      const auto rhs_keys = *rhs_keys_result;
      for (std::size_t key = 0; key < std::min(lhs_keys.size(), rhs_keys.size()); ++key) {
        ambiance_equivalence_detail::checkCommonKeyEqual(lhs_keys[key], rhs_keys[key]);
        ambiance_equivalence_detail::checkAutomationEqual(lhs_keys[key].pan, rhs_keys[key].pan);
      }
      continue;
    }

    const auto lhs_keys_result = lhs.positionedKeys(track);
    const auto rhs_keys_result = rhs.positionedKeys(track);
    CHECK(lhs_keys_result);
    CHECK(rhs_keys_result);
    if (!lhs_keys_result || !rhs_keys_result) continue;
    const auto lhs_keys = *lhs_keys_result;
    const auto rhs_keys = *rhs_keys_result;
    for (std::size_t key = 0; key < std::min(lhs_keys.size(), rhs_keys.size()); ++key) {
      ambiance_equivalence_detail::checkCommonKeyEqual(lhs_keys[key], rhs_keys[key]);
      ambiance_equivalence_detail::checkAutomationEqual(lhs_keys[key].x, rhs_keys[key].x);
      ambiance_equivalence_detail::checkAutomationEqual(lhs_keys[key].y, rhs_keys[key].y);
      ambiance_equivalence_detail::checkAutomationEqual(lhs_keys[key].z, rhs_keys[key].z);
    }
  }

  checkSoundsEquivalent(lhs, rhs, options.sounds);
}

}  // namespace test_support
