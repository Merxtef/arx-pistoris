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

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

ArxStringView view(std::string_view value) { return {value.data(), value.size()}; }

ArxAmbianceAutomation constant(float value) { return {value, value, 0, ARX_AMBIANCE_AUTOMATION_CONSTANT}; }

pistoris::SoundIndex addSound(pistoris::Ambiance& ambiance, const ArxSoundView& sound) {
  auto result = ambiance.addSound(sound);
  REQUIRE(result);
  return *result;
}

pistoris::AmbianceTrackIndex addPannedTrack(pistoris::Ambiance& ambiance, const ArxAmbiancePannedTrackInput& track) {
  auto result = ambiance.addPannedTrack(track);
  REQUIRE(result);
  return *result;
}

pistoris::AmbianceTrackIndex addPositionedTrack(pistoris::Ambiance& ambiance,
                                                const ArxAmbiancePositionedTrackInput& track) {
  auto result = ambiance.addPositionedTrack(track);
  REQUIRE(result);
  return *result;
}

pistoris::NativeAmbianceBundle bakeNativeBundle(const pistoris::Ambiance& ambiance) {
  auto result = ambiance.bakeNativeBundle({});
  REQUIRE(result);
  return std::move(*result);
}

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
    const pistoris::SoundIndex sound = addSound(ambiance, {view("timing.wav"), {wav.data(), wav.size()}});

    ArxAmbiancePannedKey master{};
    master.play_count = 1;
    master.delay_min_ms = 1000;
    master.delay_max_ms = 1000;
    master.volume = constant(1.0f);
    master.pitch = constant(1.0f);
    master.pan = constant(0.0f);
    addPannedTrack(ambiance, {sound, &master, 1});

    ArxAmbiancePannedKey child = master;
    child.play_count = 2;
    addPannedTrack(ambiance, {sound, &child, 1});

    WarningCapture warnings;
    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
    REQUIRE(warnings.messages.size() == 1);
    CHECK(warnings.messages[0].find("maximum nominal duration") != std::string::npos);
  }

  TEST_CASE("Native timing warning skips path-only audio") {
    pistoris::Ambiance ambiance;
    const pistoris::SoundIndex sound = addSound(ambiance, {view("timing.wav"), {}});
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    addPannedTrack(ambiance, {sound, &key, 1});
    key.play_count = 2;
    addPannedTrack(ambiance, {sound, &key, 1});

    WarningCapture warnings;
    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
    CHECK(warnings.messages.empty());
  }

  TEST_CASE("Native baking emits stereo and mono variants from one source") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    pistoris::Ambiance ambiance;
    const ArxSoundView sound{view("custom/wind.ogg"), {wav.data(), wav.size()}};
    const pistoris::SoundIndex sound_index = addSound(ambiance, sound);

    ArxAmbiancePannedKey panned{};
    panned.play_count = 1;
    panned.volume = constant(1.0f);
    panned.pitch = constant(1.0f);
    panned.pan = constant(0.0f);
    addPannedTrack(ambiance, {sound_index, &panned, 1});

    ArxAmbiancePositionedKey positioned{};
    positioned.play_count = 1;
    positioned.volume = constant(1.0f);
    positioned.pitch = constant(1.0f);
    positioned.x = constant(0.0f);
    positioned.y = constant(0.0f);
    positioned.z = constant(0.0f);
    addPositionedTrack(ambiance, {sound_index, &positioned, 1});

    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
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
    const pistoris::SoundIndex converted = addSound(ambiance, {view("custom/wind.ogg"), {wav.data(), wav.size()}});
    const pistoris::SoundIndex existing = addSound(ambiance, {view("custom/wind.wav"), {wav.data(), wav.size()}});

    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    addPannedTrack(ambiance, {converted, &key, 1});
    addPannedTrack(ambiance, {existing, &key, 1});

    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
    REQUIRE(bundle.sound_files.size() == 2);
    CHECK(bundle.sound_files[0].path == "custom/wind_1.wav");
    CHECK(bundle.sound_files[1].path == "custom/wind.wav");
    CHECK(bundle.amb.tracks[0].sample_path == "custom/wind_1.wav");
    CHECK(bundle.amb.tracks[1].sample_path == "custom/wind.wav");
  }

  TEST_CASE("Converted sounds preserve later natural WAV names") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    pistoris::Ambiance ambiance;
    const pistoris::SoundIndex first = addSound(ambiance, {view("custom/foo.mp3"), {wav.data(), wav.size()}});
    const pistoris::SoundIndex duplicate = addSound(ambiance, {view("custom/foo.ogg"), {wav.data(), wav.size()}});
    const pistoris::SoundIndex suffixed = addSound(ambiance, {view("custom/foo_1.mp3"), {wav.data(), wav.size()}});

    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    addPannedTrack(ambiance, {first, &key, 1});
    addPannedTrack(ambiance, {duplicate, &key, 1});
    addPannedTrack(ambiance, {suffixed, &key, 1});

    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
    REQUIRE(bundle.sound_files.size() == 3);
    CHECK(bundle.sound_files[0].path == "custom/foo.wav");
    CHECK(bundle.sound_files[1].path == "custom/foo_2.wav");
    CHECK(bundle.sound_files[2].path == "custom/foo_1.wav");
  }

  TEST_CASE("Stereo WAV keeps its original alongside a spatial mono copy") {
    const std::vector<std::uint8_t> wav = makePcm16Wav(2);
    pistoris::Ambiance ambiance;
    const pistoris::SoundIndex sound = addSound(ambiance, {view("custom/spatial.wav"), {wav.data(), wav.size()}});

    ArxAmbiancePositionedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.x = constant(0.0f);
    key.y = constant(0.0f);
    key.z = constant(0.0f);
    addPositionedTrack(ambiance, {sound, &key, 1});

    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
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
    const pistoris::SoundIndex sound = addSound(ambiance, {view("custom/spatial.ogg"), {wav.data(), wav.size()}});

    ArxAmbiancePositionedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.x = constant(0.0f);
    key.y = constant(0.0f);
    key.z = constant(0.0f);
    addPositionedTrack(ambiance, {sound, &key, 1});

    const pistoris::NativeAmbianceBundle bundle = bakeNativeBundle(ambiance);
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
    const pistoris::SoundIndex sound_index = addSound(ambiance, {view("audio/test.wav"), {wav.data(), wav.size()}});
    [[maybe_unused]] const pistoris::SoundIndex unused_sound =
        addSound(ambiance, {view("audio/unused.wav"), {wav.data(), wav.size()}});
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    addPannedTrack(ambiance, {sound_index, &key, 1});

    auto bundle_result = ambiance.exportGlbBundle({}, nullptr);
    REQUIRE(bundle_result);
    const pistoris::AmbianceGlbBundle& bundle = *bundle_result;
    REQUIRE(bundle.sound_files.size() == 1);
    CHECK(bundle.sound_files[0].source_sound == sound_index);
    CHECK(bundle.sound_files[0].path == "audio/test.wav");
    CHECK(bundle.sound_files[0].encoded_audio == wav);

    std::vector<pistoris::SoundSourceReference> sources;
    auto import = pistoris::Ambiance::importGlb(bundle.glb, {}, &sources);
    REQUIRE(import);
    REQUIRE(sources.size() == 1);
    CHECK(sources[0].sound == 0);
    CHECK(sources[0].path == "audio/test.wav");
  }

  TEST_CASE("Compaction removes unused sounds and remaps tracks") {
    pistoris::Ambiance ambiance;
    const pistoris::SoundIndex unused = addSound(ambiance, {view("unused.wav"), {}});
    const pistoris::SoundIndex used = addSound(ambiance, {view("used.wav"), {}});
    CHECK(unused == 0);
    ArxAmbiancePannedKey key{};
    key.play_count = 1;
    key.volume = constant(1.0f);
    key.pitch = constant(1.0f);
    key.pan = constant(0.0f);
    addPannedTrack(ambiance, {used, &key, 1});
    CHECK(ambiance.removeSound(used).code() == ARX_AMBIANCE_SOUND_IN_USE);

    auto compact = ambiance.compactSounds();
    REQUIRE(compact);
    CHECK(*compact == 1);
    CHECK(ambiance.soundCount() == 1);
    REQUIRE(ambiance.tracks().size() == 1);
    CHECK(ambiance.tracks()[0].sound == 0);
  }
}
