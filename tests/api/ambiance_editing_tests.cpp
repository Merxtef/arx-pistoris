// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/arx_pistoris.h"

#include "audio_helpers.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

ArxStringView ambianceStringView(std::string_view value) { return {value.data(), value.size()}; }

ArxAmbianceAutomation constantAutomation(float value) { return {value, value, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT}; }

}  // namespace

TEST_SUITE("C Ambiance editing") {
  TEST_CASE("Positioned tracks and sounds support their complete edit lifecycle") {
    ArxAmbiance* ambiance = nullptr;
    REQUIRE(arx_pistoris_ambiance_create(&ambiance) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    ArxSoundView sound{ambianceStringView("sfx/positioned.wav"), {wav.data(), wav.size()}};
    ArxSoundView unused{ambianceStringView("sfx/unused.wav"), {wav.data(), wav.size()}};
    ArxSoundIndex sound_index = ARX_NO_SOUND;
    ArxSoundIndex unused_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_ambiance_add_sound(ambiance, &sound, &sound_index) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_add_sound(ambiance, &unused, &unused_index) == ARX_OK);

    ArxAmbiancePositionedKey positioned{};
    positioned.play_count = 1;
    positioned.volume = constantAutomation(0.75f);
    positioned.pitch = constantAutomation(1.0f);
    positioned.x = constantAutomation(10.0f);
    positioned.y = constantAutomation(20.0f);
    positioned.z = constantAutomation(30.0f);
    ArxAmbiancePositionedTrackInput positioned_track{sound_index, &positioned, 1};
    ArxAmbianceTrackIndex positioned_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_ambiance_add_positioned_track(ambiance, &positioned_track, &positioned_index) == ARX_OK);

    positioned.x = constantAutomation(40.0f);
    REQUIRE(arx_pistoris_ambiance_set_positioned_track(ambiance, positioned_index, &positioned_track) == ARX_OK);
    ArxAmbiancePositionedKey copied{};
    REQUIRE(arx_pistoris_ambiance_copy_positioned_keys(ambiance, positioned_index, 0, 1, &copied) == ARX_OK);
    CHECK(copied.x.first == doctest::Approx(40.0f));

    ArxAmbiancePannedKey panned{};
    panned.play_count = 1;
    panned.volume = constantAutomation(1.0f);
    panned.pitch = constantAutomation(1.0f);
    panned.pan = constantAutomation(0.0f);
    const ArxAmbiancePannedTrackInput panned_track{sound_index, &panned, 1};
    ArxAmbianceTrackIndex panned_index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_ambiance_add_panned_track(ambiance, &panned_track, &panned_index) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_set_master_track(ambiance, panned_index) == ARX_OK);
    ArxAmbianceTrackIndex master_track = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_ambiance_master_track(ambiance, &master_track) == ARX_OK);
    CHECK(master_track == panned_index);
    REQUIRE(arx_pistoris_ambiance_remove_track(ambiance, positioned_index) == ARX_OK);
    std::size_t count = 0;
    REQUIRE(arx_pistoris_ambiance_track_count(ambiance, &count) == ARX_OK);
    CHECK(count == 1);
    REQUIRE(arx_pistoris_ambiance_master_track(ambiance, &master_track) == ARX_OK);
    CHECK(master_track == 0);

    sound.path = ambianceStringView("sfx/renamed.wav");
    sound.encoded_audio = {};
    REQUIRE(arx_pistoris_ambiance_set_sound(ambiance, sound_index, &sound) == ARX_OK);
    ArxSoundView copied_sound{};
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, sound_index, 1, &copied_sound) == ARX_OK);
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "sfx/renamed.wav"));
    REQUIRE(arx_pistoris_ambiance_set_sound_data(ambiance, sound_index, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, sound_index, 1, &copied_sound) == ARX_OK);
    REQUIRE(copied_sound.encoded_audio.size == wav.size());
    CHECK(std::equal(wav.begin(), wav.end(), copied_sound.encoded_audio.data));
    REQUIRE(arx_pistoris_ambiance_clear_sound_data(ambiance, sound_index) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, sound_index, 1, &copied_sound) == ARX_OK);
    CHECK(copied_sound.encoded_audio.size == 0);
    REQUIRE(arx_pistoris_ambiance_set_sound_data(ambiance, sound_index, {wav.data(), wav.size()}) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, sound_index, 1, &copied_sound) == ARX_OK);
    REQUIRE(copied_sound.encoded_audio.size == wav.size());
    CHECK(std::equal(wav.begin(), wav.end(), copied_sound.encoded_audio.data));
    REQUIRE(arx_pistoris_ambiance_rebase_sound_paths(ambiance, ambianceStringView("custom")) == ARX_OK);

    std::size_t removed = 0;
    REQUIRE(arx_pistoris_ambiance_compact_sounds(ambiance, &removed) == ARX_OK);
    CHECK(removed == 1);
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, 0, 1, &copied_sound) == ARX_OK);
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "custom/renamed.wav"));

    REQUIRE(arx_pistoris_ambiance_remove_track(ambiance, 0) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_track_count(ambiance, &count) == ARX_OK);
    CHECK(count == 0);
    REQUIRE(arx_pistoris_ambiance_remove_sound(ambiance, 0) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_sound_count(ambiance, &count) == ARX_OK);
    CHECK(count == 0);
    arx_pistoris_ambiance_destroy(ambiance);
  }
}
