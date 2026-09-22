// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/base/string_view.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/cinematic/types.h"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include "audio_helpers.h"
#include "io/path_location.h"
#include "io/policy.h"
#include "io/service.h"
#include "resources/cinematic_sound_io.h"
#include "resources/sound_io.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    static std::atomic<unsigned> sequence = 0;
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("arx-pistoris-cinematic-sound-" + std::to_string(timestamp) + "-" + std::to_string(sequence++));
    std::error_code error;
    REQUIRE(std::filesystem::create_directories(path_, error));
    REQUIRE_FALSE(error);
  }

  TemporaryDirectory(const TemporaryDirectory&) = delete;
  TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

  ~TemporaryDirectory() noexcept {
    try {
      std::error_code error;
      std::filesystem::remove_all(path_, error);
    } catch (...) {
      return;
    }
  }

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

void writeBytes(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  REQUIRE_FALSE(error);
  std::ofstream output(path, std::ios::binary);
  REQUIRE(output.good());
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  REQUIRE(output.good());
}

std::string_view stringView(ArxStringView value) { return {value.data, value.size}; }

cli::SoundInput looseInput(const TemporaryDirectory& temp) {
  return {.use_format_sources = true,
          .source_base = {.path = temp.path().string(), .address = cli::PathAddress::kAbsolute}};
}

cli::SoundInput mountedInput() { return {}; }

ArxCinematicSoundView soundView(const pistoris::Cinematic& cinematic, pistoris::SoundKind kind,
                                pistoris::SoundIndex index) {
  ArxCinematicSoundView result{};
  REQUIRE(cinematic.copySoundViews(kind, index, 1, &result) == ARX_OK);
  return result;
}

}  // namespace

TEST_SUITE("CLI Cinematic sound IO") {
  TEST_CASE("Native Cinematic audio uses physical effect and language directories") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    writeBytes(temp.path() / "sfx" / "effects" / "hit.wav", wav);
    writeBytes(temp.path() / "speech" / "english" / "hero" / "line.wav", wav);
    writeBytes(temp.path() / "speech" / "unused" / "other.wav", wav);

    pistoris::Cinematic cinematic;
    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effects/hit", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line", speech) == ARX_OK);
    std::vector<pistoris::CinematicSoundSourceReference> sources = {
        {effect, "effects/hit"},
        {speech, "hero/line"},
    };
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    REQUIRE(cli::prepareCinematicSounds(
        cinematic, io, looseInput(temp), sources, cli::CinematicSoundSourceFormat::kCin, true));
    CHECK(cinematic.soundEncodingCount() == 2);
    REQUIRE(cinematic.languageCount() == 1);
    ArxCinematicLanguageView language{};
    REQUIRE(cinematic.copyLanguages(0, 1, &language) == ARX_OK);
    CHECK(stringView(language.name) == "english");
  }

  TEST_CASE("Loose native Cinematic audio uses WAV MP3 Ogg priority") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> preferred = makePcm16Wav(1);
    const std::vector<std::uint8_t> fallback = makePcm16Wav(2);
    writeBytes(temp.path() / "sfx" / "effects" / "hit", fallback);
    writeBytes(temp.path() / "sfx" / "effects" / "hit.mp3", preferred);
    writeBytes(temp.path() / "sfx" / "effects" / "hit.ogg", fallback);
    writeBytes(temp.path() / "speech" / "english" / "hero" / "line.mp3", preferred);
    writeBytes(temp.path() / "speech" / "english" / "hero" / "line.ogg", fallback);

    pistoris::Cinematic cinematic;
    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effects/hit", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line", speech) == ARX_OK);
    std::vector<pistoris::CinematicSoundSourceReference> sources = {
        {effect, "effects/hit"},
        {speech, "hero/line"},
    };
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    REQUIRE(cli::prepareCinematicSounds(
        cinematic, io, looseInput(temp), sources, cli::CinematicSoundSourceFormat::kCin, true));
    REQUIRE(cinematic.soundEncodingCount() == 2);
    ArxCinematicSoundEncodingView encodings[2]{};
    REQUIRE(cinematic.copySoundEncodings(0, 2, encodings) == ARX_OK);
    CHECK(encodings[0].encoded_audio.size == preferred.size());
    CHECK(encodings[1].encoded_audio.size == preferred.size());
  }

  TEST_CASE("Mounted native Cinematic audio only accepts WAV") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    writeBytes(temp.path() / "sfx" / "effects" / "hit.mp3", wav);
    writeBytes(temp.path() / "speech" / "english" / "hero" / "line.ogg", wav);

    pistoris::Cinematic cinematic;
    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effects/hit", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line", speech) == ARX_OK);
    std::vector<pistoris::CinematicSoundSourceReference> sources = {
        {effect, "effects/hit"},
        {speech, "hero/line"},
    };
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {temp.path().string()});

    REQUIRE(cli::prepareCinematicSounds(
        cinematic, io, mountedInput(), sources, cli::CinematicSoundSourceFormat::kCin, true));
    CHECK(cinematic.soundEncodingCount() == 0);
    CHECK(cinematic.languageCount() == 0);
  }

  TEST_CASE("Direct native Cinematic sidecars preserve selected encoded audio") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    writeBytes(temp.path() / "sfx" / "effects" / "hit.mp3", wav);
    writeBytes(temp.path() / "speech" / "english" / "hero" / "line.ogg", wav);

    pistoris::Cin cinematic;
    cinematic.sounds = {{"effects/hit", false}, {"hero/line", true}};
    cinematic.keyframes.resize(2);
    cinematic.keyframes[0].sound = 0;
    cinematic.keyframes[1].sound = 1;
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});
    std::vector<pistoris::SoundFile> files;

    cli::loadNativeCinematicSoundFiles(cinematic, pistoris::NativeTextMode::kUtf8, io, looseInput(temp), files);
    REQUIRE(files.size() == 2);
    const std::unordered_set<std::string_view> paths = {files[0].path, files[1].path};
    const std::unordered_set<std::string_view> expected = {
        "sfx/effects/hit.wav",
        "speech/english/hero/line.wav",
    };
    CHECK(paths == expected);
    CHECK(files[0].encoded_audio == wav);
    CHECK(files[1].encoded_audio == wav);
  }

  TEST_CASE("GLB audio strips leftover language labels and discovers all translations") {
    TemporaryDirectory temp;
    const std::vector<std::uint8_t> wav = makePcm16Wav(1);
    writeBytes(temp.path() / "effects" / "hit.wav", wav);
    writeBytes(temp.path() / "hero" / "line[english].wav", wav);
    writeBytes(temp.path() / "hero" / "line[french].wav", wav);

    pistoris::Cinematic cinematic;
    pistoris::SoundHandle effect = pistoris::kNoSoundHandle;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "effects/hit.mp3", effect) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line[English].mp3", speech) == ARX_OK);
    std::vector<pistoris::CinematicSoundSourceReference> sources = {
        {effect, "effects/hit.mp3"},
        {speech, "hero/line[English].mp3"},
    };
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    REQUIRE(cli::prepareCinematicSounds(
        cinematic, io, looseInput(temp), sources, cli::CinematicSoundSourceFormat::kGlb, true));
    CHECK(stringView(soundView(cinematic, pistoris::SoundKind::kEffect, 0).path) == "effects/hit");
    CHECK(stringView(soundView(cinematic, pistoris::SoundKind::kSpeech, 0).path) == "hero/line");
    CHECK(sources[0].path == "effects/hit.mp3");
    CHECK(sources[1].path == "hero/line[English].mp3");
    CHECK(cinematic.soundEncodingCount() == 3);
    REQUIRE(cinematic.languageCount() == 2);
    ArxCinematicLanguageView languages[2]{};
    REQUIRE(cinematic.copyLanguages(0, 2, languages) == ARX_OK);
    const std::unordered_set<std::string_view> names = {stringView(languages[0].name), stringView(languages[1].name)};
    const std::unordered_set<std::string_view> expected = {"english", "french"};
    CHECK(names == expected);
  }

  TEST_CASE("Skipping GLB audio still canonicalizes logical paths") {
    pistoris::Cinematic cinematic;
    pistoris::SoundHandle speech = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kSpeech, "hero/line[english].wav", speech) == ARX_OK);
    std::vector<pistoris::CinematicSoundSourceReference> sources = {{speech, "hero/line[english].wav"}};
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    REQUIRE(cli::prepareCinematicSounds(cinematic, io, {}, sources, cli::CinematicSoundSourceFormat::kGlb, false));
    CHECK(stringView(soundView(cinematic, pistoris::SoundKind::kSpeech, 0).path) == "hero/line");
    CHECK(cinematic.languageCount() == 0);
    CHECK(cinematic.soundEncodingCount() == 0);
  }

  TEST_CASE("Distinct GLB sounds cannot collapse to one logical identity") {
    pistoris::Cinematic cinematic;
    pistoris::SoundHandle first = pistoris::kNoSoundHandle;
    pistoris::SoundHandle second = pistoris::kNoSoundHandle;
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "same.wav", first) == ARX_OK);
    REQUIRE(cinematic.addSound(pistoris::SoundKind::kEffect, "same.mp3", second) == ARX_OK);
    std::vector<pistoris::CinematicSoundSourceReference> sources = {
        {first, "same.wav"},
        {second, "same.mp3"},
    };
    cli::IoService io(cli::OverwriteMode::kAlwaysYes, false, {});

    CHECK_FALSE(cli::prepareCinematicSounds(cinematic, io, {}, sources, cli::CinematicSoundSourceFormat::kGlb, false));
    CHECK(stringView(soundView(cinematic, pistoris::SoundKind::kEffect, 0).path) == "same.wav");
    CHECK(stringView(soundView(cinematic, pistoris::SoundKind::kEffect, 1).path) == "same.mp3");
  }
}
