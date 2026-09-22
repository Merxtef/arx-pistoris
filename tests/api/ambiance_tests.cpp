// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/native/text.h"

#include "amb_helpers.h"
#include "audio_helpers.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxSoundView soundView(std::string_view path) { return {view(path), {nullptr, 0}}; }

ArxAmbianceAutomation constant(float value) { return {value, value, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT}; }

}  // namespace

TEST_SUITE("C Ambiance API") {
  TEST_CASE("Converts native handles and exposes copied values") {
    const std::vector<std::uint8_t> bytes = makeAmbBytes();
    ArxAmb* native = nullptr;
    REQUIRE(arx_pistoris_amb_read(bytes.data(), bytes.size(), &native) == ARX_OK);

    ArxAmbiance* ambiance = nullptr;
    ArxSoundSourceReferences* sound_sources = nullptr;
    REQUIRE(arx_pistoris_ambiance_import_native(native, &ambiance, &sound_sources, ARX_NATIVE_TEXT_AUTO) == ARX_OK);
    REQUIRE(ambiance != nullptr);
    REQUIRE(sound_sources != nullptr);
    CHECK(arx_pistoris_ambiance_validate(ambiance) == ARX_OK);

    size_t source_count = 0;
    REQUIRE(arx_pistoris_sound_source_references_count(sound_sources, &source_count) == ARX_OK);
    CHECK(source_count == 1);
    ArxSoundSourceReference source{};
    REQUIRE(arx_pistoris_sound_source_references_get(sound_sources, 0, &source) == ARX_OK);
    CHECK(source.sound == 0);
    CHECK((std::string_view(source.path.data, source.path.size) == "sfx/ambiance/test.wav"));

    ArxStringView path{};
    REQUIRE(arx_pistoris_ambiance_resource_path(ambiance, &path) == ARX_OK);
    CHECK(path.size == 0);
    REQUIRE(arx_pistoris_ambiance_set_resource_path(ambiance, view("ambiance:cave")) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_resource_path(ambiance, &path) == ARX_OK);
    CHECK((std::string_view(path.data, path.size) == "sfx/ambiance/cave.amb"));
    REQUIRE(arx_pistoris_ambiance_set_resource_path(ambiance, {nullptr, 0}) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_resource_path(ambiance, &path) == ARX_OK);
    CHECK(path.size == 0);

    size_t track_count = 0;
    REQUIRE(arx_pistoris_ambiance_track_count(ambiance, &track_count) == ARX_OK);
    CHECK(track_count == 1);
    ArxAmbianceTrack track{};
    REQUIRE(arx_pistoris_ambiance_copy_tracks(ambiance, 0, 1, &track) == ARX_OK);
    CHECK(track.sound == 0);
    CHECK(track.kind == ARX_AMBIANCE_TRACK_POSITIONED);
    size_t sound_count = 0;
    REQUIRE(arx_pistoris_ambiance_sound_count(ambiance, &sound_count) == ARX_OK);
    CHECK(sound_count == 1);
    ArxSoundView sound{};
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, 0, 1, &sound) == ARX_OK);
    CHECK((std::string_view(sound.path.data, sound.path.size) == "sfx/ambiance/test.wav"));

    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    REQUIRE(arx_pistoris_ambiance_set_sound_data(ambiance, 0, {wav.data(), wav.size()}) == ARX_OK);
    ArxAmb* baked = nullptr;
    ArxSoundFiles* baked_sounds = nullptr;
    ArxNativeAmbianceBakeOptions bake_options = ARX_NATIVE_AMBIANCE_BAKE_OPTIONS_INIT;
    REQUIRE(arx_pistoris_ambiance_bake_native(ambiance, &bake_options, &baked, &baked_sounds) == ARX_OK);
    CHECK(arx_pistoris_amb_validate(baked) == ARX_OK);
    size_t baked_sound_count = 0;
    REQUIRE(arx_pistoris_sound_files_count(baked_sounds, &baked_sound_count) == ARX_OK);
    CHECK(baked_sound_count == 1);
    ArxSoundFile baked_sound{};
    REQUIRE(arx_pistoris_sound_files_get(baked_sounds, 0, &baked_sound) == ARX_OK);
    CHECK(baked_sound.source_sound == 0);
    CHECK(baked_sound.encoded_audio.size != 0);

    arx_pistoris_sound_files_destroy(baked_sounds);
    arx_pistoris_amb_destroy(baked);
    arx_pistoris_sound_source_references_destroy(sound_sources);
    arx_pistoris_ambiance_destroy(ambiance);
    arx_pistoris_amb_destroy(native);
  }

  TEST_CASE("Copies inputs and keeps master selection coherent") {
    ArxAmbiance* ambiance = nullptr;
    REQUIRE(arx_pistoris_ambiance_create(&ambiance) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_set_resource_path(ambiance, view("sfx/ambiance/custom.amb")) == ARX_OK);

    ArxAmbianceTrackIndex master = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_ambiance_master_track(ambiance, &master) == ARX_OK);
    CHECK(master == 0);

    char sample[] = "sfx/test.wav";
    ArxSoundView sound = soundView({sample, sizeof(sample) - 1});
    ArxSoundIndex sound_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_ambiance_add_sound(ambiance, &sound, &sound_index) == ARX_OK);
    CHECK(sound_index == 0);
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(0.5f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    const ArxAmbiancePannedTrackInput input{sound_index, &key, 1};
    ArxAmbianceTrackIndex index = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_ambiance_add_panned_track(ambiance, &input, &index) == ARX_OK);
    CHECK(index == 0);
    sample[0] = 'x';

    ArxAmbianceTrack copied{};
    REQUIRE(arx_pistoris_ambiance_copy_tracks(ambiance, 0, 1, &copied) == ARX_OK);
    CHECK(copied.sound == sound_index);
    ArxSoundView copied_sound{};
    REQUIRE(arx_pistoris_ambiance_copy_sound_views(ambiance, 0, 1, &copied_sound) == ARX_OK);
    CHECK((std::string_view(copied_sound.path.data, copied_sound.path.size) == "sfx/test.wav"));

    master = ARX_INVALID_INDEX;
    REQUIRE(arx_pistoris_ambiance_master_track(ambiance, &master) == ARX_OK);
    CHECK(master == 0);

    ArxAmbiance* clone = nullptr;
    REQUIRE(arx_pistoris_ambiance_clone(ambiance, &clone) == ARX_OK);
    size_t track_count = 0;
    ArxStringView path{};
    REQUIRE(arx_pistoris_ambiance_clear_tracks(ambiance) == ARX_OK);
    REQUIRE(arx_pistoris_ambiance_track_count(ambiance, &track_count) == ARX_OK);
    CHECK(track_count == 0);
    REQUIRE(arx_pistoris_ambiance_resource_path(ambiance, &path) == ARX_OK);
    CHECK((std::string_view(path.data, path.size) == "sfx/ambiance/custom.amb"));

    REQUIRE(arx_pistoris_ambiance_reset(ambiance) == ARX_OK);
    CHECK(arx_pistoris_ambiance_validate(ambiance) == ARX_AMBIANCE_NO_TRACKS);
    CHECK(arx_pistoris_ambiance_validate(clone) == ARX_OK);

    arx_pistoris_ambiance_destroy(clone);
    arx_pistoris_ambiance_destroy(ambiance);
  }

  TEST_CASE("Validates opaque handles and submitted pointers") {
    CHECK(arx_pistoris_ambiance_create(nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_ambiance_validate(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_ambiance_clone(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_ambiance_clear_tracks(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_ambiance_trim_tracks_to_master(nullptr, nullptr) == ARX_INVALID_HANDLE);

    ArxAmbiance* ambiance = nullptr;
    REQUIRE(arx_pistoris_ambiance_create(&ambiance) == ARX_OK);
    CHECK(arx_pistoris_ambiance_resource_path(ambiance, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_ambiance_track_count(ambiance, nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_ambiance_copy_tracks(ambiance, 0, 1, nullptr) == ARX_INDEX_OUT_OF_RANGE);

    ArxSoundView sound = soundView("sfx/test.wav");
    ArxSoundIndex sound_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_ambiance_add_sound(ambiance, &sound, &sound_index) == ARX_OK);
    const ArxAmbiancePannedTrackInput invalid{sound_index, nullptr, 1};
    ArxAmbianceTrackIndex index = 0;
    CHECK(arx_pistoris_ambiance_add_panned_track(ambiance, &invalid, &index) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_ambiance_set_panned_track(ambiance, 0, &invalid) == ARX_INVALID_DATA_POINTER);

    ArxAmbiancePannedKey invalid_key{};
    invalid_key.volume = constant(1.0f);
    invalid_key.pitch = constant(1.0f);
    invalid_key.pan.mode = 99;
    const ArxAmbiancePannedTrackInput invalid_automation{sound_index, &invalid_key, 1};
    index = 0;
    CHECK(arx_pistoris_ambiance_add_panned_track(ambiance, &invalid_automation, &index) == ARX_AMBIANCE_BAD_AUTOMATION);
    CHECK(index == ARX_INVALID_INDEX);
    CHECK(arx_pistoris_ambiance_set_panned_track(ambiance, 0, &invalid_automation) == ARX_INDEX_OUT_OF_RANGE);
    arx_pistoris_ambiance_destroy(ambiance);
  }

  TEST_CASE("Trims tracks through the C boundary") {
    ArxAmbiance* ambiance = nullptr;
    REQUIRE(arx_pistoris_ambiance_create(&ambiance) == ARX_OK);
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    const ArxSoundView sound{view("sfx/timing.wav"), {wav.data(), wav.size()}};
    ArxSoundIndex sound_index = ARX_NO_SOUND;
    REQUIRE(arx_pistoris_ambiance_add_sound(ambiance, &sound, &sound_index) == ARX_OK);

    ArxAmbiancePannedKey master{};
    master.play_count = 1;
    master.delay_min_ms = 3000;
    master.delay_max_ms = 3000;
    master.volume = constant(1.0f);
    master.pitch = constant(1.0f);
    master.pan = constant(0.0f);
    ArxAmbianceTrackIndex track = ARX_INVALID_INDEX;
    const ArxAmbiancePannedTrackInput master_input{sound_index, &master, 1};
    REQUIRE(arx_pistoris_ambiance_add_panned_track(ambiance, &master_input, &track) == ARX_OK);

    ArxAmbiancePannedKey child = master;
    child.play_count = 4;
    child.delay_min_ms = 1000;
    child.delay_max_ms = 1000;
    const ArxAmbiancePannedTrackInput child_input{sound_index, &child, 1};
    REQUIRE(arx_pistoris_ambiance_add_panned_track(ambiance, &child_input, &track) == ARX_OK);

    size_t trimmed = 0;
    REQUIRE(arx_pistoris_ambiance_trim_tracks_to_master(ambiance, &trimmed) == ARX_OK);
    CHECK(trimmed == 1);
    ArxAmbiancePannedKey copied{};
    REQUIRE(arx_pistoris_ambiance_copy_panned_keys(ambiance, 1, 0, 1, &copied) == ARX_OK);
    CHECK(copied.play_count == 2);
    CHECK((std::string_view(arx_pistoris_strerror(ARX_AMBIANCE_SOUND_DATA_REQUIRED)) ==
           "Ambiance: encoded sound data is required"));
    CHECK((std::string_view(arx_pistoris_strerror(ARX_AMBIANCE_TRACK_CANNOT_FIT_MASTER)) ==
           "Ambiance: track cannot fit within the master duration"));

    arx_pistoris_ambiance_destroy(ambiance);
  }

  TEST_CASE("Exports and imports standalone GLB through the C boundary") {
    const std::vector<std::uint8_t> bytes = makeAmbBytes();
    ArxAmb* native = nullptr;
    REQUIRE(arx_pistoris_amb_read(bytes.data(), bytes.size(), &native) == ARX_OK);
    ArxAmbiance* ambiance = nullptr;
    REQUIRE(arx_pistoris_ambiance_import_native(native, &ambiance, nullptr, ARX_NATIVE_TEXT_AUTO) == ARX_OK);

    ArxAmbianceGlbExportOptions export_options = ARX_AMBIANCE_GLB_EXPORT_OPTIONS_INIT;
    uint8_t* encoded = nullptr;
    size_t encoded_size = 0;
    constexpr std::string_view kObj = R"(# arx_action view_attach 10 20 30
v 0 0 0
v 1 0 0
v 0 1 0
f 1 2 3
)";
    ArxModel* model = nullptr;
    REQUIRE(arx_pistoris_model_import_obj(
                reinterpret_cast<const std::uint8_t*>(kObj.data()), kObj.size(), nullptr, 0, &model, nullptr) ==
            ARX_OK);
    REQUIRE(arx_pistoris_ambiance_export_glb(ambiance, &export_options, model, &encoded, &encoded_size, nullptr) ==
            ARX_OK);
    REQUIRE(encoded != nullptr);
    REQUIRE(encoded_size != 0);

    ArxAmbianceGlbImportOptions import_options = ARX_AMBIANCE_GLB_IMPORT_OPTIONS_INIT;
    ArxAmbiance* imported = nullptr;
    REQUIRE(arx_pistoris_ambiance_import_glb(encoded, encoded_size, &import_options, &imported, nullptr) == ARX_OK);
    size_t track_count = 0;
    REQUIRE(arx_pistoris_ambiance_track_count(imported, &track_count) == ARX_OK);
    CHECK(track_count == 1);

    arx_pistoris_ambiance_destroy(imported);
    arx_pistoris_free_bytes(encoded);
    arx_pistoris_model_destroy(model);
    arx_pistoris_ambiance_destroy(ambiance);
    arx_pistoris_amb_destroy(native);
  }
}
