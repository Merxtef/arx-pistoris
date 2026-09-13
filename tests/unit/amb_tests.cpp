// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "amb_helpers.h"
#include "native/amb.h"
#include "utils/cursor.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct LogCapture {
  std::vector<std::string> warnings;
};

void captureLog(ArxLogLevel level, const char* message, void* userdata) {
  if (level == ARX_LOG_WARN && message) static_cast<LogCapture*>(userdata)->warnings.emplace_back(message);
}

struct LogReset {
  ~LogReset() { pistoris::setLogCallback(nullptr, nullptr); }
};

}  // namespace

TEST_SUITE("amb") {
  TEST_CASE("ReadsAllKnownVersionsIntoCanonicalData") {
    for (const std::uint32_t version :
         {pistoris::kAmbVersion1000, pistoris::kAmbVersion, pistoris::kAmbVersion1002, pistoris::kAmbVersion1003}) {
      CAPTURE(version);
      const std::vector<std::uint8_t> bytes = makeAmbBytes(version);
      pistoris::amb::Data data;
      pistoris::ReadCursor cursor(bytes.data(), bytes.size());
      REQUIRE(pistoris::loadAmb(&data, cursor) == ARX_OK);
      REQUIRE(data.tracks.size() == 1);
      CHECK(data.tracks.front().sample_path == "sfx/ambiance/test.wav");
      CHECK(data.tracks.front().flags == (pistoris::amb::kTrackMaster | pistoris::amb::kTrackPosition));
      REQUIRE(data.tracks.front().keys.size() == (version == pistoris::kAmbVersion1000 ? 1 : 2));
      CHECK(data.tracks.front().keys.front().start_ms == 100);
      if (version != pistoris::kAmbVersion1000) CHECK(data.tracks.front().keys.back().start_ms == 200);
    }
  }

  TEST_CASE("DiscardsUnusedSerializedFlags") {
    constexpr std::uint32_t kUnusedTrackFlags = 0xfffffffa;
    constexpr std::uint32_t kUnusedSettingFlags = 0xfffffffc;
    pistoris::amb::Data source = makeAmbData();
    source.tracks.front().flags |= kUnusedTrackFlags;
    for (pistoris::amb::Key& key : source.tracks.front().keys) {
      key.volume.flags |= kUnusedSettingFlags;
      key.pitch.flags |= kUnusedSettingFlags;
    }

    for (const std::uint32_t version :
         {pistoris::kAmbVersion1000, pistoris::kAmbVersion, pistoris::kAmbVersion1002, pistoris::kAmbVersion1003}) {
      CAPTURE(version);
      const std::vector<std::uint8_t> bytes = makeAmbBytes(source, version);
      pistoris::amb::Data data;
      pistoris::ReadCursor cursor(bytes.data(), bytes.size());
      REQUIRE(pistoris::loadAmb(&data, cursor) == ARX_OK);
      const pistoris::amb::Track& track = data.tracks.front();
      CHECK(track.flags == (pistoris::amb::kTrackMaster | pistoris::amb::kTrackPosition));
      CHECK(track.keys.front().volume.flags == pistoris::amb::kSettingInterpolate);
      CHECK(track.keys.front().pitch.flags == 0);
    }
  }

  TEST_CASE("WarnsForEveryDiscardedTrackName") {
    pistoris::amb::Data source = makeAmbData();
    pistoris::amb::Track second = source.tracks.front();
    second.sample_path = "sfx/ambiance/second.wav";
    second.flags &= ~pistoris::amb::kTrackMaster;
    source.tracks.push_back(std::move(second));
    constexpr std::string_view kTrackNames[] = {"first-track", "second-track"};
    const std::vector<std::uint8_t> bytes = makeAmbBytes(source, pistoris::kAmbVersion1002, kTrackNames);
    LogCapture capture;
    LogReset reset;
    pistoris::setLogCallback(captureLog, &capture);

    pistoris::amb::Data data;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    CHECK(pistoris::loadAmb(&data, cursor) == ARX_OK);
    REQUIRE(capture.warnings.size() == 2);
    CHECK(capture.warnings[0].find(kTrackNames[0]) != std::string::npos);
    CHECK(capture.warnings[1].find(kTrackNames[1]) != std::string::npos);
  }

  TEST_CASE("Version1003PreservesPhysicalTrackOrder") {
    pistoris::amb::Data source = makeAmbData();
    source.tracks.front().sample_path = "sfx/ambiance/first.wav";
    pistoris::amb::Track second = source.tracks.front();
    second.sample_path = "sfx/ambiance/second.wav";
    second.flags &= ~pistoris::amb::kTrackMaster;
    second.keys = {makeAmbKey(300), makeAmbKey(400)};
    source.tracks.push_back(std::move(second));

    const std::vector<std::uint8_t> bytes = makeAmbBytes(source, pistoris::kAmbVersion1003);
    pistoris::amb::Data result;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadAmb(&result, cursor) == ARX_OK);
    REQUIRE(result.tracks.size() == 2);
    CHECK(result.tracks[0].sample_path == "sfx/ambiance/first.wav");
    CHECK(result.tracks[1].sample_path == "sfx/ambiance/second.wav");
    REQUIRE(result.tracks[1].keys.size() == 2);
    CHECK(result.tracks[1].keys[0].start_ms == 300);
    CHECK(result.tracks[1].keys[1].start_ms == 400);
  }

  TEST_CASE("WritesCanonicalVersion1001") {
    const pistoris::amb::Data source = makeAmbData();
    pistoris::WriteCursor cursor;
    REQUIRE(pistoris::saveAmb(&source, cursor) == ARX_OK);
    const std::vector<std::uint8_t> bytes = cursor.take();
    REQUIRE(bytes.size() >= 12);

    std::uint32_t version = 0;
    std::memcpy(&version, bytes.data() + sizeof(std::uint32_t), sizeof(version));
    CHECK(version == pistoris::kAmbVersion);

    pistoris::amb::Data roundtrip;
    pistoris::ReadCursor read_cursor(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadAmb(&roundtrip, read_cursor) == ARX_OK);
    REQUIRE(roundtrip.tracks.size() == 1);
    REQUIRE(roundtrip.tracks.front().keys.size() == 2);
    CHECK(roundtrip.tracks.front().keys[0].start_ms == 100);
    CHECK(roundtrip.tracks.front().keys[1].start_ms == 200);
  }

  TEST_CASE("DoesNotWriteUnusedFlags") {
    constexpr std::uint32_t kUnusedTrackFlags = 0xfffffffa;
    constexpr std::uint32_t kUnusedSettingFlags = 0xfffffffc;
    pistoris::amb::Data source = makeAmbData();
    source.tracks.front().flags |= kUnusedTrackFlags;
    source.tracks.front().keys.front().volume.flags |= kUnusedSettingFlags;

    pistoris::WriteCursor cursor;
    REQUIRE(pistoris::saveAmb(&source, cursor) == ARX_OK);
    const std::vector<std::uint8_t> bytes = cursor.take();

    const std::size_t track_flags_offset = 12 + source.tracks.front().sample_path.size() + 1;
    REQUIRE(bytes.size() >= track_flags_offset + sizeof(std::uint32_t));
    std::uint32_t track_flags = 0;
    std::memcpy(&track_flags, bytes.data() + track_flags_offset, sizeof(track_flags));
    CHECK(track_flags == (pistoris::amb::kTrackMaster | pistoris::amb::kTrackPosition));

    constexpr std::size_t kVolumeFlagsOffsetWithinKey =
        5 * sizeof(std::uint32_t) + 2 * sizeof(float) + sizeof(std::uint32_t);
    const std::size_t volume_flags_offset =
        track_flags_offset + 2 * sizeof(std::uint32_t) + kVolumeFlagsOffsetWithinKey;
    REQUIRE(bytes.size() >= volume_flags_offset + sizeof(std::uint32_t));
    std::uint32_t setting_flags = 0;
    std::memcpy(&setting_flags, bytes.data() + volume_flags_offset, sizeof(setting_flags));
    CHECK(setting_flags == pistoris::amb::kSettingInterpolate);
  }

  TEST_CASE("RejectsUnknownVersion") {
    for (const std::uint32_t version : {pistoris::kAmbVersion1000 - 1, pistoris::kAmbVersion1003 + 1}) {
      std::vector<std::uint8_t> bytes = makeAmbBytes();
      std::memcpy(bytes.data() + sizeof(std::uint32_t), &version, sizeof(version));
      pistoris::amb::Data data;
      pistoris::ReadCursor cursor(bytes.data(), bytes.size());
      CHECK(pistoris::loadAmb(&data, cursor) == ARX_AMB_BAD_VERSION);
    }
  }

  TEST_CASE("RejectsSerializedTrackWithoutKeys") {
    std::vector<std::uint8_t> bytes = makeAmbBytes();
    constexpr std::string_view kSamplePath = "sfx/ambiance/test.wav";
    const std::size_t key_count_offset = 12 + kSamplePath.size() + 1 + sizeof(std::uint32_t);
    const std::uint32_t key_count = 0;
    std::memcpy(bytes.data() + key_count_offset, &key_count, sizeof(key_count));
    bytes.resize(key_count_offset + sizeof(key_count));

    pistoris::amb::Data data;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    CHECK(pistoris::loadAmb(&data, cursor) == ARX_AMB_BAD_KEY_COUNT);
  }

  TEST_CASE("ValidatesPlayableStructure") {
    pistoris::amb::Data data;
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_TRACK_COUNT);

    data = makeAmbData();
    data.tracks.front().flags &= ~pistoris::amb::kTrackMaster;
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_MASTER_COUNT);

    data = makeAmbData();
    data.tracks.push_back(data.tracks.front());
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_MASTER_COUNT);

    data = makeAmbData();
    data.tracks.front().sample_path.clear();
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SAMPLE_PATH);

    data = makeAmbData();
    data.tracks.front().keys.clear();
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_KEY_COUNT);

    data = makeAmbData();
    data.tracks.front().keys.front().delay_min_ms = 21;
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_KEY_TIMING);

    data = makeAmbData();
    data.tracks.front().keys.front().volume.min = std::numeric_limits<float>::quiet_NaN();
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);

    data = makeAmbData();
    data.tracks.front().keys.front().volume = makeAmbSetting(2.0f, 1.0f, 10, pistoris::amb::kSettingRandom);
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);

    data = makeAmbData();
    data.tracks.front().keys.front().volume = makeAmbSetting(1.0f, 2.0f, 0, pistoris::amb::kSettingInterpolate);
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);
  }

  TEST_CASE("RejectsUnusedInactiveSettingData") {
    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().keys.front().pan.min = std::numeric_limits<float>::quiet_NaN();
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_UNUSED_SETTING_DATA);

    data.tracks.front().flags &= ~pistoris::amb::kTrackPosition;
    data.tracks.front().keys.front().pan = makeAmbSetting(0.0f);
    data.tracks.front().keys.front().x.min = std::numeric_limits<float>::quiet_NaN();
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_UNUSED_SETTING_DATA);
  }

  TEST_CASE("LoadCanonicalizesUnusedSettings") {
    pistoris::amb::Data source = makeAmbData();
    pistoris::amb::Key& key = source.tracks.front().keys.front();
    key.pan = makeAmbSetting(std::numeric_limits<float>::quiet_NaN(), 7.0f, 41, 0xffffffff);
    key.pitch = makeAmbSetting(2.0f, 2.0f, 91, pistoris::amb::kSettingRandom);

    const std::vector<std::uint8_t> bytes = makeAmbBytes(source, pistoris::kAmbVersion);
    pistoris::amb::Data result;
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    REQUIRE(pistoris::loadAmb(&result, cursor) == ARX_OK);

    const pistoris::amb::Key& loaded = result.tracks.front().keys.front();
    CHECK(loaded.pan.min == 0.0f);
    CHECK(loaded.pan.max == 0.0f);
    CHECK(loaded.pan.interval_ms == 0);
    CHECK(loaded.pan.flags == 0);
    CHECK(loaded.pitch.min == 2.0f);
    CHECK(loaded.pitch.max == 2.0f);
    CHECK(loaded.pitch.interval_ms == 0);
    CHECK(loaded.pitch.flags == 0);
    CHECK(pistoris::validateAmb(&result) == ARX_OK);
  }

  TEST_CASE("RejectsUnusedConstantSettingData") {
    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().keys.front().pitch.interval_ms = 1;
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_UNUSED_SETTING_DATA);

    data = makeAmbData();
    data.tracks.front().keys.front().pitch.flags = pistoris::amb::kSettingInterpolate;
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_UNUSED_SETTING_DATA);
  }

  TEST_CASE("ValidatesInactiveRandomSettings") {
    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().keys.front().pan = makeAmbSetting(2.0f, 1.0f, 10, pistoris::amb::kSettingRandom);
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);

    data = makeAmbData();
    data.tracks.front().flags &= ~pistoris::amb::kTrackPosition;
    data.tracks.front().keys.front().x = makeAmbSetting(2.0f, 1.0f, 10, pistoris::amb::kSettingRandom);
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);
  }

  TEST_CASE("RejectsUnrepresentableDynamicRanges") {
    constexpr float kMax = std::numeric_limits<float>::max();

    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().keys.front().pan = makeAmbSetting(-kMax, kMax, 10, pistoris::amb::kSettingRandom);
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);

    data = makeAmbData();
    data.tracks.front().keys.front().volume = makeAmbSetting(-kMax, kMax, 10, pistoris::amb::kSettingInterpolate);
    CHECK(pistoris::validateAmb(&data) == ARX_AMB_BAD_SETTING);
  }

  TEST_CASE("RejectsTruncatedInputTransactionally") {
    std::vector<std::uint8_t> bytes = makeAmbBytes();
    bytes.pop_back();
    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().sample_path = "unchanged";
    pistoris::ReadCursor cursor(bytes.data(), bytes.size());
    CHECK(pistoris::loadAmb(&data, cursor) == ARX_UNEXPECTED_EOF);
    CHECK(data.tracks.front().sample_path == "unchanged");
  }
}
