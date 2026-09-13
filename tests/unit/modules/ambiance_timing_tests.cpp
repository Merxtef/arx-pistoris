// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "modules/ambiance.h"

#include <cstdint>
#include <vector>

namespace {

pistoris::AmbianceKeyCommon key(std::uint32_t start, std::uint32_t count, std::uint32_t delay_min,
                                std::uint32_t delay_max, pistoris::Automation pitch) {
  pistoris::AmbianceKeyCommon result;
  result.start_delay_ms = start;
  result.play_count = count;
  result.delay_min_ms = delay_min;
  result.delay_max_ms = delay_max;
  result.pitch = pitch;
  return result;
}

pistoris::PannedAmbianceKey pannedKey(const pistoris::AmbianceKeyCommon& common) {
  pistoris::PannedAmbianceKey result;
  static_cast<pistoris::AmbianceKeyCommon&>(result) = common;
  return result;
}

pistoris::PositionedAmbianceKey positionedKey(const pistoris::AmbianceKeyCommon& common) {
  pistoris::PositionedAmbianceKey result;
  static_cast<pistoris::AmbianceKeyCommon&>(result) = common;
  return result;
}

}  // namespace

TEST_SUITE("ambiance::timing") {
  TEST_CASE("Combines key timing with clamped pitch bounds") {
    pistoris::AmbianceTrack track;
    track.keys =
        std::vector<pistoris::PannedAmbianceKey>{pannedKey(key(50, 2, 100, 200, pistoris::ConstantAutomation{2.0f}))};
    pistoris::ambiance::TrackTimingBounds bounds = pistoris::ambiance::trackTimingBounds(track, 1000.0L);
    CHECK(static_cast<double>(bounds.minimum_ms) == doctest::Approx(1250.0));
    CHECK(static_cast<double>(bounds.maximum_ms) == doctest::Approx(1450.0));

    track.keys = std::vector<pistoris::PannedAmbianceKey>{pannedKey(
        key(0, 1, 0, 0, pistoris::DynamicAutomation{0.5f, 2.0f, 100, pistoris::DynamicAutomationMode::kStep}))};
    bounds = pistoris::ambiance::trackTimingBounds(track, 1000.0L);
    CHECK(static_cast<double>(bounds.minimum_ms) == doctest::Approx(500.0));
    CHECK(static_cast<double>(bounds.maximum_ms) == doctest::Approx(2000.0));

    track.keys = std::vector<pistoris::PannedAmbianceKey>{pannedKey(
        key(0, 1, 0, 0, pistoris::DynamicAutomation{0.5f, 1.5f, 100, pistoris::DynamicAutomationMode::kInterpolated}))};
    bounds = pistoris::ambiance::trackTimingBounds(track, 1000.0L);
    CHECK(static_cast<double>(bounds.minimum_ms) == doctest::Approx(500.0));
    CHECK(static_cast<double>(bounds.maximum_ms) == doctest::Approx(10000.0));
  }

  TEST_CASE("Plans partial plays and trailing key removal without expanding repetitions") {
    pistoris::AmbianceTrack track;
    track.keys =
        std::vector<pistoris::PannedAmbianceKey>{pannedKey(key(100, 3, 100, 100, pistoris::ConstantAutomation{1.0f})),
                                                 pannedKey(key(200, 3, 100, 100, pistoris::ConstantAutomation{1.0f}))};

    pistoris::ambiance::TrackTrimPlan plan;
    REQUIRE(pistoris::ambiance::planTrackTrim(track, 900.0L, 5500.0L, plan) == pistoris::ambiance::Error::kNone);
    CHECK(plan.retained_key_count == 2);
    CHECK(plan.final_play_count == 2);
    CHECK(pistoris::ambiance::applyTrackTrim(track, plan));
    const auto& keys = std::get<std::vector<pistoris::PannedAmbianceKey>>(track.keys);
    REQUIRE(keys.size() == 2);
    CHECK(keys.back().play_count == 2);

    pistoris::AmbianceTrack whole_key_track;
    whole_key_track.keys =
        std::vector<pistoris::PannedAmbianceKey>{pannedKey(key(100, 3, 100, 100, pistoris::ConstantAutomation{1.0f})),
                                                 pannedKey(key(200, 3, 100, 100, pistoris::ConstantAutomation{1.0f}))};
    REQUIRE(pistoris::ambiance::planTrackTrim(whole_key_track, 900.0L, 3200.0L, plan) ==
            pistoris::ambiance::Error::kNone);
    CHECK(plan.retained_key_count == 1);
    CHECK(plan.final_play_count == 3);
  }

  TEST_CASE("Accepts equality and rejects a limit shorter than the first play") {
    pistoris::AmbianceTrack track;
    track.keys =
        std::vector<pistoris::PannedAmbianceKey>{pannedKey(key(100, 1, 100, 100, pistoris::ConstantAutomation{1.0f}))};
    pistoris::ambiance::TrackTrimPlan plan;
    REQUIRE(pistoris::ambiance::planTrackTrim(track, 900.0L, 1100.0L, plan) == pistoris::ambiance::Error::kNone);
    CHECK(plan.retained_key_count == 1);
    CHECK(plan.final_play_count == 1);
    CHECK_FALSE(pistoris::ambiance::applyTrackTrim(track, plan));
    CHECK(pistoris::ambiance::planTrackTrim(track, 900.0L, 1099.0L, plan) ==
          pistoris::ambiance::Error::kCannotFitDuration);
  }

  TEST_CASE("Panned and positioned tracks use identical common timing") {
    const pistoris::AmbianceKeyCommon common = key(250, 4, 50, 75, pistoris::ConstantAutomation{1.25f});
    pistoris::AmbianceTrack panned;
    panned.keys = std::vector<pistoris::PannedAmbianceKey>{pannedKey(common)};
    pistoris::AmbianceTrack positioned;
    positioned.keys = std::vector<pistoris::PositionedAmbianceKey>{positionedKey(common)};
    const pistoris::ambiance::TrackTimingBounds panned_bounds = pistoris::ambiance::trackTimingBounds(panned, 500.0L);
    const pistoris::ambiance::TrackTimingBounds positioned_bounds =
        pistoris::ambiance::trackTimingBounds(positioned, 500.0L);
    CHECK(panned_bounds.minimum_ms == positioned_bounds.minimum_ms);
    CHECK(panned_bounds.maximum_ms == positioned_bounds.maximum_ms);
  }
}
