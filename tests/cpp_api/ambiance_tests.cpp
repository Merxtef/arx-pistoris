// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/sound.h"

#include "amb_helpers.h"
#include "audio_helpers.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxSoundView soundView(std::string_view path) { return {view(path), {nullptr, 0}}; }

ArxSoundView soundView(std::string_view path, const std::vector<std::uint8_t>& encoded_audio) {
  return {view(path), {encoded_audio.data(), encoded_audio.size()}};
}

pistoris::SoundIndex addSound(pistoris::Ambiance& ambiance, std::string_view path) {
  pistoris::AmbianceResult<pistoris::SoundIndex> result = ambiance.addSound(soundView(path));
  REQUIRE(result);
  return *result;
}

pistoris::Ambiance importAmbiance(const pistoris::Amb& native) {
  pistoris::AmbResult<pistoris::Ambiance> result = pistoris::Ambiance::importNative(native);
  REQUIRE(result);
  return std::move(*result);
}

ArxAmbianceAutomation constant(float value) { return {value, value, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT}; }

ArxAmbiancePannedKey pannedKey() {
  ArxAmbiancePannedKey key;
  key.start_delay_ms = 15;
  key.play_count = 3;
  key.delay_min_ms = 10;
  key.delay_max_ms = 20;
  key.volume = constant(0.5f);
  key.pitch = constant(1.0f);
  key.pan = {-1.0f, 1.0f, 100, ARX_AMBIANCE_AUTOMATION_INTERPOLATED};
  return key;
}

}  // namespace

TEST_SUITE("C++ Ambiance API") {
  TEST_CASE("Native import locates an invalid AMB key") {
    pistoris::Amb native = makeAmbData();
    native.tracks[0].keys[1].delay_min_ms = native.tracks[0].keys[1].delay_max_ms + 1;

    const pistoris::AmbResult<pistoris::Ambiance> result = pistoris::Ambiance::importNative(native);

    REQUIRE_FALSE(result);
    CHECK(result.code() == ARX_AMB_BAD_KEY_TIMING);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::AmbElement::kKey);
    CHECK(result.error()->location()->index == 0);
    CHECK(result.error()->location()->subindex == 1);
  }

  TEST_CASE("Converts native data losslessly through semantic keys") {
    pistoris::Amb native = makeAmbData();
    native.tracks.front().sample_path = "sfx/ambiance/test.wav";
    pistoris::Ambiance ambiance = importAmbiance(native);
    CHECK(ambiance.resourcePath().empty());
    REQUIRE(ambiance.setResourcePath(R"(AMBIANCE:CAVE\MY__AMBIANCE)"));
    CHECK((ambiance.resourcePath() == "sfx/ambiance/cave/my__ambiance.amb"));
    REQUIRE(ambiance.setResourcePath(R"(Custom\Caves\DEEP.AMB)"));
    CHECK((ambiance.resourcePath() == "custom/caves/deep.amb"));
    CHECK(ambiance.trackCount() == 1);
    CHECK(ambiance.soundCount() == 1);
    CHECK(ambiance.masterTrack() == 0);

    const ArxAmbianceTrack track = ambiance.tracks()[0];
    CHECK(track.sound == 0);
    const ArxSoundView sound = ambiance.sounds()[0];
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/ambiance/test.wav"));
    CHECK(track.kind == ARX_AMBIANCE_TRACK_POSITIONED);
    CHECK(track.key_count == 2);

    const auto keys = ambiance.positionedKeys(0);
    REQUIRE(keys);
    CHECK((*keys)[0].play_count == 2);
    CHECK((*keys)[0].volume.mode == ARX_AMBIANCE_AUTOMATION_INTERPOLATED);
    CHECK((*keys)[0].x.mode == ARX_AMBIANCE_AUTOMATION_CONSTANT);

    pistoris::AmbianceResult<pistoris::Amb> baked_result = ambiance.bakeNative();
    REQUIRE(baked_result);
    pistoris::Amb baked = std::move(*baked_result);
    REQUIRE(baked.tracks.size() == 1);
    CHECK(baked.tracks.front().keys.front().loop_minus_one == 1);
    CHECK(baked.tracks.front().keys.front().volume.flags == pistoris::amb::kSettingInterpolate);
    const pistoris::amb::Setting& pan = baked.tracks.front().keys.front().pan;
    CHECK(pan.min == 0.0f);
    CHECK(pan.max == 0.0f);
    CHECK(pan.interval_ms == 0);
    CHECK(pan.flags == 0);
  }

  TEST_CASE("Owns edited tracks and preserves master coherence") {
    pistoris::Ambiance ambiance;
    CHECK(ambiance.masterTrack() == 0);
    REQUIRE(ambiance.setResourcePath("ambiance:custom"));
    CHECK((ambiance.resourcePath() == "sfx/ambiance/custom.amb"));

    char sample[] = "sfx/custom.wav";
    const std::vector<std::uint8_t> audio = makePcm16Wav(1);
    const auto first_sound_result = ambiance.addSound(soundView(sample, audio));
    REQUIRE(first_sound_result);
    const pistoris::SoundIndex first_sound = *first_sound_result;
    const ArxAmbiancePannedKey key = pannedKey();
    const ArxAmbiancePannedTrackInput panned{first_sound, &key, 1};
    const auto first_result = ambiance.addPannedTrack(panned);
    REQUIRE(first_result);
    const pistoris::AmbianceTrackIndex first = *first_result;
    CHECK(first == 0);
    CHECK(ambiance.masterTrack() == first);
    sample[0] = 'x';

    const ArxAmbianceTrack copied = ambiance.tracks()[0];
    CHECK(copied.sound == first_sound);
    const ArxSoundView copied_sound = ambiance.sounds()[first_sound];
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "sfx/custom.wav"));
    REQUIRE(ambiance.setSoundPath(first_sound, "sfx/renamed.wav"));
    const ArxSoundView renamed_sound = ambiance.sounds()[first_sound];
    CHECK((std::string_view(renamed_sound.path.data, renamed_sound.path.size) == "sfx/renamed.wav"));
    REQUIRE(renamed_sound.encoded_audio.size == audio.size());
    CHECK(std::equal(audio.begin(), audio.end(), renamed_sound.encoded_audio.data));

    ArxAmbiancePositionedKey positioned{};
    positioned.volume = constant(0.75f);
    positioned.pitch = constant(1.0f);
    positioned.x = constant(1.0f);
    positioned.y = constant(2.0f);
    positioned.z = constant(3.0f);
    const pistoris::SoundIndex second_sound = addSound(ambiance, "sfx/positioned.wav");
    const ArxAmbiancePositionedTrackInput second_input{second_sound, &positioned, 1};
    const auto second_result = ambiance.addPositionedTrack(second_input);
    REQUIRE(second_result);
    REQUIRE(ambiance.removeTrack(first));
    CHECK(ambiance.masterTrack() == 0);

    const auto third_result = ambiance.addPannedTrack(panned);
    REQUIRE(third_result);
    const pistoris::AmbianceTrackIndex third = *third_result;
    REQUIRE(ambiance.setMasterTrack(third));
    REQUIRE(ambiance.removeTrack(0));
    CHECK(ambiance.masterTrack() == 0);
    CHECK(ambiance.validate());

    ambiance.clearTracks();
    CHECK(ambiance.trackCount() == 0);
    CHECK(ambiance.masterTrack() == 0);
    CHECK((ambiance.resourcePath() == "sfx/ambiance/custom.amb"));
    REQUIRE(ambiance.setResourcePath({}));
    CHECK(ambiance.resourcePath().empty());
  }

  TEST_CASE("Trims non-master tracks to the master timing envelope transactionally") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    const auto sound_result = ambiance.addSound(soundView("sfx/timing.wav", wav));
    REQUIRE(sound_result);
    const pistoris::SoundIndex sound = *sound_result;

    ArxAmbiancePannedKey master = pannedKey();
    master.start_delay_ms = 0;
    master.play_count = 1;
    master.delay_min_ms = 5000;
    master.delay_max_ms = 5000;
    master.pitch = constant(1.0f);
    REQUIRE(ambiance.addPannedTrack({sound, &master, 1}));

    std::array<ArxAmbiancePannedKey, 2> child{};
    for (ArxAmbiancePannedKey& key : child) {
      key = pannedKey();
      key.start_delay_ms = 0;
      key.play_count = 3;
      key.delay_min_ms = 1000;
      key.delay_max_ms = 1000;
      key.pitch = constant(1.0f);
    }
    const auto child_track_result = ambiance.addPannedTrack({sound, child.data(), child.size()});
    REQUIRE(child_track_result);
    const pistoris::AmbianceTrackIndex child_track = *child_track_result;

    const auto trimmed = ambiance.trimTracksToMaster();
    REQUIRE(trimmed);
    CHECK(*trimmed == 1);
    const auto copied = ambiance.pannedKeys(child_track);
    REQUIRE(copied);
    CHECK((*copied)[0].play_count == 3);
    CHECK((*copied)[1].play_count == 1);
  }

  TEST_CASE("Leaves tracks unchanged when master trimming cannot be completed") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    const auto encoded_result = ambiance.addSound(soundView("sfx/master.wav", wav));
    const auto path_only_result = ambiance.addSound(soundView("sfx/path-only.wav"));
    REQUIRE(encoded_result);
    REQUIRE(path_only_result);
    const pistoris::SoundIndex encoded = *encoded_result;
    const pistoris::SoundIndex path_only = *path_only_result;

    ArxAmbiancePannedKey master = pannedKey();
    master.start_delay_ms = 0;
    master.play_count = 1;
    master.delay_min_ms = 100;
    master.delay_max_ms = 100;
    master.pitch = constant(1.0f);
    REQUIRE(ambiance.addPannedTrack({encoded, &master, 1}));

    ArxAmbiancePannedKey trimmable = master;
    trimmable.play_count = 2;
    trimmable.delay_min_ms = 60;
    trimmable.delay_max_ms = 60;
    REQUIRE(ambiance.addPannedTrack({encoded, &trimmable, 1}));

    ArxAmbiancePannedKey child = master;
    child.delay_min_ms = 200;
    child.delay_max_ms = 200;
    REQUIRE(ambiance.addPannedTrack({encoded, &child, 1}));
    CHECK(ambiance.trimTracksToMaster().code() == ARX_AMBIANCE_TRACK_CANNOT_FIT_MASTER);
    auto copied = ambiance.pannedKeys(1);
    REQUIRE(copied);
    CHECK((*copied)[0].play_count == 2);
    copied = ambiance.pannedKeys(2);
    REQUIRE(copied);
    CHECK((*copied)[0].delay_max_ms == 200);

    REQUIRE(ambiance.setPannedTrack(2, {path_only, &child, 1}));
    CHECK(ambiance.trimTracksToMaster().code() == ARX_AMBIANCE_SOUND_DATA_REQUIRED);
    copied = ambiance.pannedKeys(1);
    REQUIRE(copied);
    CHECK((*copied)[0].play_count == 2);
  }

  TEST_CASE("Rejects invalid paths, buffers, and automation") {
    pistoris::Ambiance ambiance;
    CHECK(ambiance.setResourcePath("cave").code() == ARX_AMBIANCE_BAD_RESOURCE_PATH);

    ArxAmbiancePannedKey key = pannedKey();
    const pistoris::SoundIndex sound = addSound(ambiance, "sfx/test.wav");
    key.pan.mode = 99;
    const ArxAmbiancePannedTrackInput input{sound, &key, 1};
    CHECK(ambiance.addPannedTrack(input).code() == ARX_AMBIANCE_BAD_AUTOMATION);
    CHECK(ambiance.setPannedTrack(0, input).code() == ARX_INDEX_OUT_OF_RANGE);

    key = pannedKey();
    key.play_count = 0;
    const ArxAmbiancePannedTrackInput zero_play_count{sound, &key, 1};
    CHECK(ambiance.addPannedTrack(zero_play_count).code() == ARX_AMBIANCE_BAD_PLAY_COUNT);

    ArxAmbiancePositionedKey positioned{};
    positioned.volume.mode = 99;
    const ArxAmbiancePositionedTrackInput positioned_input{sound, &positioned, 1};
    CHECK(ambiance.addPositionedTrack(positioned_input).code() == ARX_AMBIANCE_BAD_AUTOMATION);
    CHECK(ambiance.setPositionedTrack(0, positioned_input).code() == ARX_INDEX_OUT_OF_RANGE);

    const ArxAmbiancePannedTrackInput bad_pointer{sound, nullptr, 1};
    CHECK(ambiance.addPannedTrack(bad_pointer).code() == ARX_INVALID_DATA_POINTER);

    const ArxAmbiancePannedTrackInput no_sound{pistoris::kNoSound, &key, 1};
    CHECK(ambiance.addPannedTrack(no_sound).code() == ARX_AMBIANCE_BAD_TRACK_SOUND);

    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
      const ArxAmbiancePannedTrackInput too_many{
          sound,
          &key,
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1,
      };
      CHECK(ambiance.addPannedTrack(too_many).code() == ARX_AMBIANCE_BAD_KEY_COUNT);
    }
  }

  TEST_CASE("Round-trips standalone GLB automation and master selection") {
    pistoris::Ambiance source;
    ArxAmbiancePannedKey panned_key = pannedKey();
    panned_key.pan = {-3.0f, -1.0f, 250, ARX_AMBIANCE_AUTOMATION_INTERPOLATED};
    const pistoris::SoundIndex panned_sound = addSound(source, "sfx/sample__wide.wav");
    const ArxAmbiancePannedTrackInput panned{panned_sound, &panned_key, 1};
    REQUIRE(source.addPannedTrack(panned));

    ArxAmbiancePositionedKey positioned_key{};
    positioned_key.play_count = 1;
    positioned_key.volume = constant(0.75f);
    positioned_key.pitch = constant(1.0f);
    positioned_key.x = {10.0f, 30.0f, 500, ARX_AMBIANCE_AUTOMATION_RANDOM_STEP};
    positioned_key.y = {5.0f, 15.0f, 1000, ARX_AMBIANCE_AUTOMATION_RANDOM_INTERPOLATED};
    positioned_key.z = {40.0f, 20.0f, 0, ARX_AMBIANCE_AUTOMATION_STEP};
    const pistoris::SoundIndex positioned_sound = addSound(source, "sfx/positioned.wav");
    const ArxAmbiancePositionedTrackInput positioned{positioned_sound, &positioned_key, 1};
    const auto positioned_index = source.addPositionedTrack(positioned);
    REQUIRE(positioned_index);
    REQUIRE(source.setMasterTrack(*positioned_index));

    pistoris::Ambiance::GlbExportOptions export_options;
    export_options.arx_units_per_glb_unit = 25.0f;
    auto encoded_result = source.exportGlb(export_options);
    REQUIRE(encoded_result);
    std::vector<std::uint8_t> encoded = std::move(*encoded_result);
    const std::string_view bytes(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    CHECK(bytes.find("arx_ambiance__MASTER_1__ambiance") != std::string_view::npos);
    CHECK(bytes.find("PAN__VAL_-2") != std::string_view::npos);
    CHECK(bytes.find("Y__RANGE_") != std::string_view::npos);
    CHECK(bytes.find("__RANDOM_INTERPOLATED__y") != std::string_view::npos);
    CHECK(bytes.find("Z__RANGE_-") != std::string_view::npos);

    pistoris::Ambiance::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = 25.0f;
    pistoris::GlbResult<pistoris::Ambiance> imported_result = pistoris::Ambiance::importGlb(encoded, import_options);
    REQUIRE(imported_result);
    pistoris::Ambiance imported = std::move(*imported_result);
    CHECK(imported.resourcePath().empty());
    CHECK(imported.trackCount() == 2);
    CHECK(imported.masterTrack() == 1);

    const auto panned_keys = imported.pannedKeys(0);
    REQUIRE(panned_keys);
    const ArxAmbiancePannedKey copied_panned = (*panned_keys)[0];
    CHECK(copied_panned.pan.first == doctest::Approx(-3.0f));
    CHECK(copied_panned.pan.second == doctest::Approx(-1.0f));
    CHECK(copied_panned.pan.interval_ms == 250);

    const auto positioned_keys = imported.positionedKeys(1);
    REQUIRE(positioned_keys);
    const ArxAmbiancePositionedKey copied_positioned = (*positioned_keys)[0];
    CHECK(copied_positioned.x.first == doctest::Approx(10.0f));
    CHECK(copied_positioned.x.second == doctest::Approx(30.0f));
    CHECK(copied_positioned.y.first == doctest::Approx(5.0f));
    CHECK(copied_positioned.y.second == doctest::Approx(15.0f));
    CHECK(copied_positioned.z.first == doctest::Approx(40.0f));
    CHECK(copied_positioned.z.second == doctest::Approx(20.0f));

    pistoris::Ambiance::GlbExportOptions bad_options;
    bad_options.arx_units_per_glb_unit = 0.0f;
    CHECK(source.exportGlb(bad_options).code() == ARX_INVALID_OPTIONS);
  }
}
