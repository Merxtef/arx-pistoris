// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/sound.h"

#include "amb_helpers.h"
#include "audio_helpers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxSoundView soundView(std::string_view path) { return {view(path), {nullptr, 0}}; }

ArxSoundView soundView(std::string_view path, const std::vector<std::uint8_t>& encoded_audio) {
  return {view(path), {encoded_audio.data(), encoded_audio.size()}};
}

pistoris::SoundIndex addSound(pistoris::Ambiance& ambiance, std::string_view path) {
  pistoris::SoundIndex result = pistoris::kNoSound;
  REQUIRE(ambiance.addSound(soundView(path), result) == ARX_OK);
  return result;
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
  TEST_CASE("Converts native data losslessly through semantic keys") {
    pistoris::Amb native = makeAmbData();
    native.tracks.front().sample_path = "sfx/ambiance/test.wav";
    pistoris::Ambiance ambiance;
    REQUIRE(pistoris::Ambiance::importNative(ambiance, native) == ARX_OK);
    CHECK(ambiance.resourcePath().empty());
    REQUIRE(ambiance.setResourcePath(R"(AMBIANCE:CAVE\MY__AMBIANCE)") == ARX_OK);
    CHECK((ambiance.resourcePath() == "sfx/ambiance/cave/my__ambiance.amb"));
    REQUIRE(ambiance.setResourcePath(R"(Custom\Caves\DEEP.AMB)") == ARX_OK);
    CHECK((ambiance.resourcePath() == "custom/caves/deep.amb"));
    CHECK(ambiance.trackCount() == 1);
    CHECK(ambiance.soundCount() == 1);
    CHECK(ambiance.masterTrack() == 0);

    ArxAmbianceTrack track{};
    REQUIRE(ambiance.copyTracks(0, 1, &track) == ARX_OK);
    CHECK(track.sound == 0);
    ArxSoundView sound{};
    REQUIRE(ambiance.copySoundViews(0, 1, &sound) == ARX_OK);
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/ambiance/test.wav"));
    CHECK(track.kind == ARX_AMBIANCE_TRACK_POSITIONED);
    CHECK(track.key_count == 2);

    std::array<ArxAmbiancePositionedKey, 2> keys{};
    REQUIRE(ambiance.copyPositionedKeys(0, 0, keys.size(), keys.data()) == ARX_OK);
    CHECK(keys[0].play_count == 2);
    CHECK(keys[0].volume.mode == ARX_AMBIANCE_AUTOMATION_INTERPOLATED);
    CHECK(keys[0].x.mode == ARX_AMBIANCE_AUTOMATION_CONSTANT);

    pistoris::Amb baked;
    REQUIRE(ambiance.bakeNative(baked) == ARX_OK);
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
    REQUIRE(ambiance.setResourcePath("ambiance:custom") == ARX_OK);
    CHECK((ambiance.resourcePath() == "sfx/ambiance/custom.amb"));

    char sample[] = "sfx/custom.wav";
    const pistoris::SoundIndex first_sound = addSound(ambiance, sample);
    const ArxAmbiancePannedKey key = pannedKey();
    const ArxAmbiancePannedTrackInput panned{first_sound, &key, 1};
    pistoris::AmbianceTrackIndex first = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack(panned, first) == ARX_OK);
    CHECK(first == 0);
    CHECK(ambiance.masterTrack() == first);
    sample[0] = 'x';

    ArxAmbianceTrack copied{};
    REQUIRE(ambiance.copyTracks(0, 1, &copied) == ARX_OK);
    CHECK(copied.sound == first_sound);
    ArxSoundView copied_sound{};
    REQUIRE(ambiance.copySoundViews(first_sound, 1, &copied_sound) == ARX_OK);
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "sfx/custom.wav"));

    ArxAmbiancePositionedKey positioned{};
    positioned.volume = constant(0.75f);
    positioned.pitch = constant(1.0f);
    positioned.x = constant(1.0f);
    positioned.y = constant(2.0f);
    positioned.z = constant(3.0f);
    const pistoris::SoundIndex second_sound = addSound(ambiance, "sfx/positioned.wav");
    const ArxAmbiancePositionedTrackInput second_input{second_sound, &positioned, 1};
    pistoris::AmbianceTrackIndex second = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPositionedTrack(second_input, second) == ARX_OK);
    REQUIRE(ambiance.removeTrack(first) == ARX_OK);
    CHECK(ambiance.masterTrack() == 0);

    pistoris::AmbianceTrackIndex third = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack(panned, third) == ARX_OK);
    REQUIRE(ambiance.setMasterTrack(third) == ARX_OK);
    REQUIRE(ambiance.removeTrack(0) == ARX_OK);
    CHECK(ambiance.masterTrack() == 0);
    CHECK(ambiance.validate() == ARX_OK);

    ambiance.clearTracks();
    CHECK(ambiance.trackCount() == 0);
    CHECK(ambiance.masterTrack() == 0);
    CHECK((ambiance.resourcePath() == "sfx/ambiance/custom.amb"));
    REQUIRE(ambiance.setResourcePath({}) == ARX_OK);
    CHECK(ambiance.resourcePath().empty());
  }

  TEST_CASE("Trims non-master tracks to the master timing envelope transactionally") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex sound = pistoris::kNoSound;
    REQUIRE(ambiance.addSound(soundView("sfx/timing.wav", wav), sound) == ARX_OK);

    ArxAmbiancePannedKey master = pannedKey();
    master.start_delay_ms = 0;
    master.play_count = 1;
    master.delay_min_ms = 5000;
    master.delay_max_ms = 5000;
    master.pitch = constant(1.0f);
    pistoris::AmbianceTrackIndex master_track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({sound, &master, 1}, master_track) == ARX_OK);

    std::array<ArxAmbiancePannedKey, 2> child{};
    for (ArxAmbiancePannedKey& key : child) {
      key = pannedKey();
      key.start_delay_ms = 0;
      key.play_count = 3;
      key.delay_min_ms = 1000;
      key.delay_max_ms = 1000;
      key.pitch = constant(1.0f);
    }
    pistoris::AmbianceTrackIndex child_track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({sound, child.data(), child.size()}, child_track) == ARX_OK);

    std::size_t trimmed = 0;
    REQUIRE(ambiance.trimTracksToMaster(&trimmed) == ARX_OK);
    CHECK(trimmed == 1);
    std::array<ArxAmbiancePannedKey, 2> copied{};
    REQUIRE(ambiance.copyPannedKeys(child_track, 0, copied.size(), copied.data()) == ARX_OK);
    CHECK(copied[0].play_count == 3);
    CHECK(copied[1].play_count == 1);
  }

  TEST_CASE("Leaves tracks unchanged when master trimming cannot be completed") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex encoded = pistoris::kNoSound;
    pistoris::SoundIndex path_only = pistoris::kNoSound;
    REQUIRE(ambiance.addSound(soundView("sfx/master.wav", wav), encoded) == ARX_OK);
    REQUIRE(ambiance.addSound(soundView("sfx/path-only.wav"), path_only) == ARX_OK);

    ArxAmbiancePannedKey master = pannedKey();
    master.start_delay_ms = 0;
    master.play_count = 1;
    master.delay_min_ms = 100;
    master.delay_max_ms = 100;
    master.pitch = constant(1.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({encoded, &master, 1}, track) == ARX_OK);

    ArxAmbiancePannedKey trimmable = master;
    trimmable.play_count = 2;
    trimmable.delay_min_ms = 60;
    trimmable.delay_max_ms = 60;
    REQUIRE(ambiance.addPannedTrack({encoded, &trimmable, 1}, track) == ARX_OK);

    ArxAmbiancePannedKey child = master;
    child.delay_min_ms = 200;
    child.delay_max_ms = 200;
    REQUIRE(ambiance.addPannedTrack({encoded, &child, 1}, track) == ARX_OK);
    CHECK(ambiance.trimTracksToMaster() == ARX_AMBIANCE_TRACK_CANNOT_FIT_MASTER);
    ArxAmbiancePannedKey copied{};
    REQUIRE(ambiance.copyPannedKeys(1, 0, 1, &copied) == ARX_OK);
    CHECK(copied.play_count == 2);
    REQUIRE(ambiance.copyPannedKeys(2, 0, 1, &copied) == ARX_OK);
    CHECK(copied.delay_max_ms == 200);

    REQUIRE(ambiance.setPannedTrack(2, {path_only, &child, 1}) == ARX_OK);
    CHECK(ambiance.trimTracksToMaster() == ARX_AMBIANCE_SOUND_DATA_REQUIRED);
    REQUIRE(ambiance.copyPannedKeys(1, 0, 1, &copied) == ARX_OK);
    CHECK(copied.play_count == 2);
  }

  TEST_CASE("Rejects invalid paths, buffers, and automation") {
    pistoris::Ambiance ambiance;
    CHECK(ambiance.setResourcePath("cave") == ARX_AMBIANCE_BAD_RESOURCE_PATH);

    ArxAmbiancePannedKey key = pannedKey();
    const pistoris::SoundIndex sound = addSound(ambiance, "sfx/test.wav");
    key.pan.mode = 99;
    const ArxAmbiancePannedTrackInput input{sound, &key, 1};
    pistoris::AmbianceTrackIndex index = 0;
    CHECK(ambiance.addPannedTrack(input, index) == ARX_AMBIANCE_BAD_AUTOMATION);
    CHECK(index == pistoris::kInvalidAmbianceTrackIndex);
    CHECK(ambiance.setPannedTrack(0, input) == ARX_INDEX_OUT_OF_RANGE);

    key = pannedKey();
    key.play_count = 0;
    const ArxAmbiancePannedTrackInput zero_play_count{sound, &key, 1};
    CHECK(ambiance.addPannedTrack(zero_play_count, index) == ARX_AMBIANCE_BAD_PLAY_COUNT);
    CHECK(index == pistoris::kInvalidAmbianceTrackIndex);

    ArxAmbiancePositionedKey positioned{};
    positioned.volume.mode = 99;
    const ArxAmbiancePositionedTrackInput positioned_input{sound, &positioned, 1};
    index = 0;
    CHECK(ambiance.addPositionedTrack(positioned_input, index) == ARX_AMBIANCE_BAD_AUTOMATION);
    CHECK(index == pistoris::kInvalidAmbianceTrackIndex);
    CHECK(ambiance.setPositionedTrack(0, positioned_input) == ARX_INDEX_OUT_OF_RANGE);

    const ArxAmbiancePannedTrackInput bad_pointer{sound, nullptr, 1};
    CHECK(ambiance.addPannedTrack(bad_pointer, index) == ARX_INVALID_DATA_POINTER);
    CHECK(ambiance.copyTracks(1, 0, nullptr) == ARX_INDEX_OUT_OF_RANGE);

    const ArxAmbiancePannedTrackInput no_sound{pistoris::kNoSound, &key, 1};
    CHECK(ambiance.addPannedTrack(no_sound, index) == ARX_AMBIANCE_BAD_TRACK_SOUND);
    CHECK(index == pistoris::kInvalidAmbianceTrackIndex);

    if constexpr (sizeof(std::size_t) > sizeof(std::uint32_t)) {
      const ArxAmbiancePannedTrackInput too_many{
          sound,
          &key,
          static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1,
      };
      CHECK(ambiance.addPannedTrack(too_many, index) == ARX_AMBIANCE_BAD_KEY_COUNT);
      CHECK(index == pistoris::kInvalidAmbianceTrackIndex);
    }
  }

  TEST_CASE("Round-trips standalone GLB automation and master selection") {
    pistoris::Ambiance source;
    ArxAmbiancePannedKey panned_key = pannedKey();
    panned_key.pan = {-3.0f, -1.0f, 250, ARX_AMBIANCE_AUTOMATION_INTERPOLATED};
    const pistoris::SoundIndex panned_sound = addSound(source, "sfx/sample__wide.wav");
    const ArxAmbiancePannedTrackInput panned{panned_sound, &panned_key, 1};
    pistoris::AmbianceTrackIndex panned_index = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(source.addPannedTrack(panned, panned_index) == ARX_OK);

    ArxAmbiancePositionedKey positioned_key{};
    positioned_key.play_count = 1;
    positioned_key.volume = constant(0.75f);
    positioned_key.pitch = constant(1.0f);
    positioned_key.x = {10.0f, 30.0f, 500, ARX_AMBIANCE_AUTOMATION_RANDOM_STEP};
    positioned_key.y = {5.0f, 15.0f, 1000, ARX_AMBIANCE_AUTOMATION_RANDOM_INTERPOLATED};
    positioned_key.z = {40.0f, 20.0f, 0, ARX_AMBIANCE_AUTOMATION_STEP};
    const pistoris::SoundIndex positioned_sound = addSound(source, "sfx/positioned.wav");
    const ArxAmbiancePositionedTrackInput positioned{positioned_sound, &positioned_key, 1};
    pistoris::AmbianceTrackIndex positioned_index = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(source.addPositionedTrack(positioned, positioned_index) == ARX_OK);
    REQUIRE(source.setMasterTrack(positioned_index) == ARX_OK);

    pistoris::Ambiance::GlbExportOptions export_options;
    export_options.arx_units_per_glb_unit = 25.0f;
    std::vector<std::uint8_t> encoded;
    REQUIRE(source.exportGlb(encoded, export_options) == ARX_OK);
    const std::string_view bytes(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    CHECK(bytes.find("arx_ambiance__MASTER_1__ambiance") != std::string_view::npos);
    CHECK(bytes.find("PAN__VAL_-2") != std::string_view::npos);
    CHECK(bytes.find("Y__RANGE_") != std::string_view::npos);
    CHECK(bytes.find("__RANDOM_INTERPOLATED__y") != std::string_view::npos);
    CHECK(bytes.find("Z__RANGE_-") != std::string_view::npos);

    pistoris::Ambiance imported;
    pistoris::Ambiance::GlbImportOptions import_options;
    import_options.arx_units_per_glb_unit = 25.0f;
    REQUIRE(pistoris::Ambiance::importGlb(imported, encoded, import_options) == ARX_OK);
    CHECK(imported.resourcePath().empty());
    CHECK(imported.trackCount() == 2);
    CHECK(imported.masterTrack() == 1);

    ArxAmbiancePannedKey copied_panned{};
    REQUIRE(imported.copyPannedKeys(0, 0, 1, &copied_panned) == ARX_OK);
    CHECK(copied_panned.pan.first == doctest::Approx(-3.0f));
    CHECK(copied_panned.pan.second == doctest::Approx(-1.0f));
    CHECK(copied_panned.pan.interval_ms == 250);

    ArxAmbiancePositionedKey copied_positioned{};
    REQUIRE(imported.copyPositionedKeys(1, 0, 1, &copied_positioned) == ARX_OK);
    CHECK(copied_positioned.x.first == doctest::Approx(10.0f));
    CHECK(copied_positioned.x.second == doctest::Approx(30.0f));
    CHECK(copied_positioned.y.first == doctest::Approx(5.0f));
    CHECK(copied_positioned.y.second == doctest::Approx(15.0f));
    CHECK(copied_positioned.z.first == doctest::Approx(40.0f));
    CHECK(copied_positioned.z.second == doctest::Approx(20.0f));

    pistoris::Ambiance::GlbExportOptions bad_options;
    bad_options.arx_units_per_glb_unit = 0.0f;
    CHECK(source.exportGlb(encoded, bad_options) == ARX_INVALID_OPTIONS);
  }
}
