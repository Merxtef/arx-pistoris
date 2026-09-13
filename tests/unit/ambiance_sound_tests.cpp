// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/ambiance/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "audio_helpers.h"
#include "utils/audio.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxAmbianceAutomation constant(float value) { return {value, value, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT}; }

struct WarningCapture {
  std::vector<std::string> messages;

  WarningCapture() {
    pistoris::setLogCallback(
        [](ArxLogLevel level, const char* message, void* userdata) {
          if (level == ARX_LOG_WARN && message) static_cast<WarningCapture*>(userdata)->messages.emplace_back(message);
        },
        this);
  }

  ~WarningCapture() { pistoris::setLogCallback(nullptr, nullptr); }
};

}  // namespace

TEST_SUITE("Ambiance sounds") {
  TEST_CASE("Native baking warns when a track can outlast the master") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex sound = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("timing.wav"), {wav.data(), wav.size()}}, sound) == ARX_OK);

    ArxAmbiancePannedKey master{};
    master.play_count = 1;
    master.delay_min_ms = 1000;
    master.delay_max_ms = 1000;
    master.volume = constant(1.0f);
    master.pitch = constant(1.0f);
    master.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({sound, &master, 1}, track) == ARX_OK);

    ArxAmbiancePannedKey child = master;
    child.play_count = 2;
    REQUIRE(ambiance.addPannedTrack({sound, &child, 1}, track) == ARX_OK);

    WarningCapture warnings;
    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("maximum nominal duration") != std::string::npos);
  }

  TEST_CASE("Native timing warning skips path-only audio") {
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex sound = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("timing.wav"), {}}, sound) == ARX_OK);
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({sound, &key, 1}, track) == ARX_OK);
    key.play_count = 2;
    REQUIRE(ambiance.addPannedTrack({sound, &key, 1}, track) == ARX_OK);

    WarningCapture warnings;
    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    CHECK(warnings.messages.empty());
  }

  TEST_CASE("Native baking emits stereo and mono variants from one source") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    pistoris::Ambiance ambiance;
    const ArxSoundView sound{view("custom/wind.ogg"), {wav.data(), wav.size()}};
    pistoris::SoundIndex sound_index = pistoris::kNoSound;
    REQUIRE(ambiance.addSound(sound, sound_index) == ARX_OK);

    ArxAmbiancePannedKey panned{};
    panned.play_count = 1;
    panned.volume = constant(1.0f);
    panned.pitch = constant(1.0f);
    panned.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({sound_index, &panned, 1}, track) == ARX_OK);

    ArxAmbiancePositionedKey positioned{};
    positioned.play_count = 1;
    positioned.volume = constant(1.0f);
    positioned.pitch = constant(1.0f);
    positioned.x = constant(0.0f);
    positioned.y = constant(0.0f);
    positioned.z = constant(0.0f);
    REQUIRE(ambiance.addPositionedTrack({sound_index, &positioned, 1}, track) == ARX_OK);

    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].source_sound == sound_index);
    CHECK(bundle.sound_files[1].source_sound == sound_index);
    CHECK(bundle.sound_files[0].path == "custom/wind.wav");
    CHECK(bundle.sound_files[1].path == "custom/wind_1.wav");
    CHECK(bundle.amb.tracks[0].sample_path == bundle.sound_files[0].path);
    CHECK(bundle.amb.tracks[1].sample_path == bundle.sound_files[1].path);

    pistoris::audio::Info stereo_info;
    pistoris::audio::Info mono_info;
    REQUIRE(pistoris::audio::inspect(bundle.sound_files[0].encoded_audio, &stereo_info) ==
            pistoris::audio::Error::kNone);
    REQUIRE(pistoris::audio::inspect(bundle.sound_files[1].encoded_audio, &mono_info) == pistoris::audio::Error::kNone);
    CHECK(stereo_info.channels == 2);
    CHECK(mono_info.channels == 1);
  }

  TEST_CASE("Existing WAV paths are protected from converted output names") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex converted = pistoris::kNoSound;
    pistoris::SoundIndex existing = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("custom/wind.ogg"), {wav.data(), wav.size()}}, converted) == ARX_OK);
    REQUIRE(ambiance.addSound({view("custom/wind.wav"), {wav.data(), wav.size()}}, existing) == ARX_OK);

    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({converted, &key, 1}, track) == ARX_OK);
    REQUIRE(ambiance.addPannedTrack({existing, &key, 1}, track) == ARX_OK);

    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].path == "custom/wind_1.wav");
    CHECK(bundle.sound_files[1].path == "custom/wind.wav");
    CHECK(bundle.amb.tracks[0].sample_path == "custom/wind_1.wav");
    CHECK(bundle.amb.tracks[1].sample_path == "custom/wind.wav");
  }

  TEST_CASE("Converted sounds preserve later natural WAV names") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex first = pistoris::kNoSound;
    pistoris::SoundIndex duplicate = pistoris::kNoSound;
    pistoris::SoundIndex suffixed = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("custom/foo.mp3"), {wav.data(), wav.size()}}, first) == ARX_OK);
    REQUIRE(ambiance.addSound({view("custom/foo.ogg"), {wav.data(), wav.size()}}, duplicate) == ARX_OK);
    REQUIRE(ambiance.addSound({view("custom/foo_1.mp3"), {wav.data(), wav.size()}}, suffixed) == ARX_OK);

    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({first, &key, 1}, track) == ARX_OK);
    REQUIRE(ambiance.addPannedTrack({duplicate, &key, 1}, track) == ARX_OK);
    REQUIRE(ambiance.addPannedTrack({suffixed, &key, 1}, track) == ARX_OK);

    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.sound_files.size() == 3);
    CHECK(bundle.sound_files[0].path == "custom/foo.wav");
    CHECK(bundle.sound_files[1].path == "custom/foo_2.wav");
    CHECK(bundle.sound_files[2].path == "custom/foo_1.wav");
  }

  TEST_CASE("Stereo WAV keeps its original alongside a spatial mono copy") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex sound = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("custom/spatial.wav"), {wav.data(), wav.size()}}, sound) == ARX_OK);

    ArxAmbiancePositionedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.x = constant(0.0f);
    key.y = constant(0.0f);
    key.z = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPositionedTrack({sound, &key, 1}, track) == ARX_OK);

    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].path == "custom/spatial.wav");
    CHECK(bundle.sound_files[1].path == "custom/spatial_1.wav");
    CHECK(bundle.amb.tracks[0].sample_path == "custom/spatial_1.wav");

    pistoris::audio::Info original_info;
    pistoris::audio::Info mono_info;
    REQUIRE(pistoris::audio::inspect(bundle.sound_files[0].encoded_audio, &original_info) ==
            pistoris::audio::Error::kNone);
    REQUIRE(pistoris::audio::inspect(bundle.sound_files[1].encoded_audio, &mono_info) == pistoris::audio::Error::kNone);
    CHECK(original_info.channels == 2);
    CHECK(mono_info.channels == 1);
  }

  TEST_CASE("Stereo non-WAV used only spatially keeps the natural converted path") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex sound = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("custom/spatial.ogg"), {wav.data(), wav.size()}}, sound) == ARX_OK);

    ArxAmbiancePositionedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.x = constant(0.0f);
    key.y = constant(0.0f);
    key.z = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPositionedTrack({sound, &key, 1}, track) == ARX_OK);

    pistoris::NativeAmbianceBundle bundle;
    REQUIRE(ambiance.bakeNativeBundle({}, bundle) == ARX_OK);
    REQUIRE(bundle.sound_files.size() == 1);
    CHECK(bundle.sound_files[0].path == "custom/spatial.wav");
    CHECK(bundle.amb.tracks[0].sample_path == "custom/spatial.wav");

    pistoris::audio::Info info;
    REQUIRE(pistoris::audio::inspect(bundle.sound_files[0].encoded_audio, &info) == pistoris::audio::Error::kNone);
    CHECK(info.channels == 1);
  }

  TEST_CASE("GLB bundles preserve encoded sidecar bytes") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex sound_index = pistoris::kNoSound;
    pistoris::SoundIndex unused_sound = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("audio/test.wav"), {wav.data(), wav.size()}}, sound_index) == ARX_OK);
    REQUIRE(ambiance.addSound({view("audio/unused.wav"), {wav.data(), wav.size()}}, unused_sound) == ARX_OK);
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({sound_index, &key, 1}, track) == ARX_OK);

    pistoris::AmbianceGlbBundle bundle;
    REQUIRE(ambiance.exportGlbBundle({}, nullptr, bundle) == ARX_OK);
    REQUIRE(bundle.sound_files.size() == 1);
    CHECK(bundle.sound_files[0].source_sound == sound_index);
    CHECK(bundle.sound_files[0].path == "audio/test.wav");
    CHECK(bundle.sound_files[0].encoded_audio == wav);

    pistoris::Ambiance imported;
    std::vector<pistoris::SoundSourceReference> sources;
    REQUIRE(pistoris::Ambiance::importGlb(imported, bundle.glb, {}, &sources) == ARX_OK);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0].sound == 0);
    CHECK(sources[0].path == "audio/test.wav");
  }

  TEST_CASE("Compaction removes unused sounds and remaps tracks") {
    pistoris::Ambiance ambiance;
    pistoris::SoundIndex unused = pistoris::kNoSound;
    pistoris::SoundIndex used = pistoris::kNoSound;
    REQUIRE(ambiance.addSound({view("unused.wav"), {}}, unused) == ARX_OK);
    REQUIRE(ambiance.addSound({view("used.wav"), {}}, used) == ARX_OK);
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    pistoris::AmbianceTrackIndex track_index = pistoris::kInvalidAmbianceTrackIndex;
    REQUIRE(ambiance.addPannedTrack({used, &key, 1}, track_index) == ARX_OK);
    CHECK(ambiance.removeSound(used) == ARX_AMBIANCE_SOUND_IN_USE);

    std::size_t removed = 0;
    REQUIRE(ambiance.compactSounds(&removed) == ARX_OK);
    CHECK(removed == 1);
    CHECK(ambiance.soundCount() == 1);
    ArxAmbianceTrack track{};
    REQUIRE(ambiance.copyTracks(0, 1, &track) == ARX_OK);
    CHECK(track.sound == 0);
  }
}
