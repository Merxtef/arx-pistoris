// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"

#include "ambiance/internal.h"
#include "modules/ambiance.h"
#include "modules/sounds.h"

#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {

pistoris::PannedAmbianceKey validKey() {
  pistoris::PannedAmbianceKey key;
  key.volume = pistoris::ConstantAutomation{0.5f};
  key.pitch = pistoris::ConstantAutomation{1.0f};
  key.pan = pistoris::DynamicAutomation{-1.0f, 1.0f, 100, pistoris::DynamicAutomationMode::kInterpolated};
  return key;
}

pistoris::AmbianceData validAmbiance() {
  pistoris::AmbianceTrack track;
  track.sound = 0;
  track.keys = std::vector{validKey()};
  return {{std::move(track)}, pistoris::AmbianceTrackIndex{0}};
}

}  // namespace

TEST_SUITE("ambiance module validation") {
  TEST_CASE("Module errors map to focused return codes") {
    using pistoris::ambiance_detail::errorCode;
    using pistoris::ambiance_detail::soundErrorCode;

    CHECK(errorCode(pistoris::ambiance::Error::kNone) == ARX_OK);
    CHECK(errorCode(pistoris::ambiance::Error::kNoTracks) == ARX_AMBIANCE_NO_TRACKS);
    CHECK(errorCode(pistoris::ambiance::Error::kTooManyTracks) == ARX_AMBIANCE_TOO_MANY_TRACKS);
    CHECK(errorCode(pistoris::ambiance::Error::kBadMasterTrack) == ARX_AMBIANCE_BAD_MASTER_TRACK);
    CHECK(errorCode(pistoris::ambiance::Error::kBadSound) == ARX_AMBIANCE_BAD_TRACK_SOUND);
    CHECK(errorCode(pistoris::ambiance::Error::kBadKeyCount) == ARX_AMBIANCE_BAD_KEY_COUNT);
    CHECK(errorCode(pistoris::ambiance::Error::kBadPlayCount) == ARX_AMBIANCE_BAD_PLAY_COUNT);
    CHECK(errorCode(pistoris::ambiance::Error::kBadKeyTiming) == ARX_AMBIANCE_BAD_KEY_TIMING);
    CHECK(errorCode(pistoris::ambiance::Error::kBadAutomation) == ARX_AMBIANCE_BAD_AUTOMATION);
    CHECK(errorCode(pistoris::ambiance::Error::kCannotFitDuration) == ARX_AMBIANCE_TRACK_CANNOT_FIT_MASTER);
    CHECK(errorCode(pistoris::ambiance::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);

    CHECK(soundErrorCode(pistoris::sounds::Error::kNone) == ARX_OK);
    CHECK(soundErrorCode(pistoris::sounds::Error::kInvalidOptions) == ARX_INVALID_OPTIONS);
    CHECK(soundErrorCode(pistoris::sounds::Error::kTooManySounds) == ARX_AMBIANCE_TOO_MANY_SOUNDS);
    CHECK(soundErrorCode(pistoris::sounds::Error::kBadPath) == ARX_AMBIANCE_BAD_SOUND_PATH);
    CHECK(soundErrorCode(pistoris::sounds::Error::kBadAudio) == ARX_AMBIANCE_BAD_SOUND_DATA);
    CHECK(soundErrorCode(pistoris::sounds::Error::kUnsupportedChannels) == ARX_AMBIANCE_UNSUPPORTED_SOUND_CHANNELS);
    CHECK(soundErrorCode(pistoris::sounds::Error::kAudioTooLarge) == ARX_AMBIANCE_SOUND_TOO_LARGE);
    CHECK(soundErrorCode(pistoris::sounds::Error::kDuplicatePath) == ARX_AMBIANCE_DUPLICATE_SOUND_PATH);
    CHECK(soundErrorCode(pistoris::sounds::Error::kBadKind) == ARX_INTERNAL_ERROR);
    CHECK(soundErrorCode(pistoris::sounds::Error::kBadLanguage) == ARX_INTERNAL_ERROR);
    CHECK(soundErrorCode(pistoris::sounds::Error::kDuplicateEncoding) == ARX_INTERNAL_ERROR);
    CHECK(soundErrorCode(pistoris::sounds::Error::kBadIndex) == ARX_INDEX_OUT_OF_RANGE);
    CHECK(soundErrorCode(pistoris::sounds::Error::kOutOfMemory) == ARX_BAD_ALLOC);
  }

  TEST_CASE("Requires playable track and master structure") {
    pistoris::AmbianceData data;
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kNoTracks);

    data.master_track = 1;
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kBadMasterTrack);

    data = validAmbiance();
    data.master_track = 1;
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kBadMasterTrack);

    data = validAmbiance();
    data.tracks.front().sound = 1;
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kBadSound);

    data = validAmbiance();
    std::get<std::vector<pistoris::PannedAmbianceKey>>(data.tracks.front().keys).clear();
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kBadKeyCount);
  }

  TEST_CASE("Validates semantic automation modes") {
    pistoris::Automation automation = pistoris::ConstantAutomation{std::numeric_limits<float>::quiet_NaN()};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kBadAutomation);

    automation = pistoris::DynamicAutomation{1.0f, 1.0f, 100, pistoris::DynamicAutomationMode::kRandomInterpolated};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kBadAutomation);

    automation = pistoris::DynamicAutomation{2.0f, 1.0f, 100, pistoris::DynamicAutomationMode::kRandomStep};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kBadAutomation);

    automation = pistoris::DynamicAutomation{-std::numeric_limits<float>::max(),
                                             std::numeric_limits<float>::max(),
                                             100,
                                             pistoris::DynamicAutomationMode::kRandomStep};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kBadAutomation);

    automation = pistoris::DynamicAutomation{0.0f, 1.0f, 0, pistoris::DynamicAutomationMode::kInterpolated};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kBadAutomation);

    automation = pistoris::DynamicAutomation{0.0f, 1.0f, 0, pistoris::DynamicAutomationMode::kStep};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kNone);

    automation = pistoris::DynamicAutomation{0.0f, 1.0f, 100, static_cast<pistoris::DynamicAutomationMode>(UINT8_MAX)};
    CHECK(pistoris::ambiance::validateAutomation(automation) == pistoris::ambiance::Error::kBadAutomation);
  }

  TEST_CASE("Validates key delay ranges") {
    pistoris::AmbianceData data = validAmbiance();
    auto& key = std::get<std::vector<pistoris::PannedAmbianceKey>>(data.tracks.front().keys).front();
    key.delay_min_ms = 2;
    key.delay_max_ms = 1;
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kBadKeyTiming);
  }

  TEST_CASE("Requires a positive play count") {
    pistoris::AmbianceData data = validAmbiance();
    std::get<std::vector<pistoris::PannedAmbianceKey>>(data.tracks.front().keys).front().play_count = 0;
    CHECK(pistoris::ambiance::validate(data, 1) == pistoris::ambiance::Error::kBadPlayCount);
  }

  TEST_CASE("Track edits keep the master index coherent") {
    pistoris::AmbianceData data;
    pistoris::AmbianceTrack first = validAmbiance().tracks.front();
    pistoris::AmbianceTrack second = first;
    second.sound = 1;

    REQUIRE(pistoris::ambiance::validateTrackAppend(data) == pistoris::ambiance::Error::kNone);
    REQUIRE(pistoris::ambiance::validateTrack(first, 2) == pistoris::ambiance::Error::kNone);
    pistoris::AmbianceTrackIndex index = pistoris::ambiance::addTrack(data, std::move(first));
    CHECK(index == 0);
    CHECK(data.tracks[0].sound == 0);
    REQUIRE(pistoris::ambiance::validateTrackAppend(data) == pistoris::ambiance::Error::kNone);
    REQUIRE(pistoris::ambiance::validateTrack(second, 2) == pistoris::ambiance::Error::kNone);
    index = pistoris::ambiance::addTrack(data, std::move(second));
    CHECK(index == 1);
    pistoris::ambiance::setMasterTrack(data, 1);
    pistoris::ambiance::removeTrack(data, 0);
    CHECK(data.master_track == 0);
    CHECK(data.tracks[0].sound == 1);

    pistoris::ambiance::clearTracks(data);
    CHECK(data.tracks.empty());
    CHECK(data.master_track == 0);
  }

  TEST_CASE("Invalid track replacement can be rejected before mutation") {
    pistoris::AmbianceData data = validAmbiance();
    pistoris::AmbianceTrack invalid = data.tracks.front();
    invalid.sound = 1;

    CHECK(pistoris::ambiance::validateTrack(invalid, 1) == pistoris::ambiance::Error::kBadSound);
    CHECK(data.tracks.front().sound == 0);
  }
}
