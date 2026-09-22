// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/sound.hpp"

#include "audio_helpers.h"
#include "modules/sounds.h"
#include "utils/audio.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {

pistoris::SoundsData effectSounds(std::vector<pistoris::Sound> sounds) {
  pistoris::SoundsData result;
  pistoris::sounds::replaceSounds(result, std::move(sounds));
  return result;
}

std::span<const std::uint8_t> effectAudio(const pistoris::SoundsData& sounds, pistoris::SoundIndex index) {
  return pistoris::sounds::encodedAudio(sounds, pistoris::sounds::effectHandle(index));
}

}  // namespace

TEST_SUITE("sounds module") {
  TEST_CASE("Inspects encoded audio through module metadata") {
    const std::vector<std::uint8_t> source = makePcm16Wav(2);
    pistoris::sounds::AudioInfo info;
    REQUIRE(pistoris::sounds::inspectEncodedAudio(source, info) == pistoris::sounds::Error::kNone);
    CHECK(info.format == pistoris::sounds::AudioFormat::kWav);
    CHECK(info.channels == 2);
    CHECK(info.sample_rate == 8000);
    CHECK(info.frame_count == 4);
    CHECK(pistoris::sounds::inspectEncodedAudio({}, info) == pistoris::sounds::Error::kBadAudio);
  }

  TEST_CASE("Normalizes paths and enforces case-insensitive identity") {
    pistoris::SoundsData data;
    pistoris::Sound first{R"(sfx\ambiance\Wind.wav)", {}};
    REQUIRE(pistoris::sounds::repairPath(data, first, pistoris::kNoSound) == pistoris::sounds::Error::kNone);
    REQUIRE(pistoris::sounds::validateSound(first) == pistoris::sounds::Error::kNone);
    const pistoris::SoundIndex index = pistoris::sounds::addSound(data, std::move(first));
    CHECK(index == 0);
    CHECK(data.effect_paths[0] == "sfx/ambiance/wind.wav");

    pistoris::Sound duplicate{"SFX/AMBIANCE/wind.WAV", {}};
    REQUIRE(pistoris::sounds::repairPath(data, duplicate, pistoris::kNoSound) == pistoris::sounds::Error::kNone);
    CHECK(duplicate.path == "sfx/ambiance/wind_1.wav");
  }

  TEST_CASE("Normalizes replacement paths before committing") {
    pistoris::SoundsData data = effectSounds({{"old.wav", {}}});
    std::vector<pistoris::Sound> replacement{{R"(SFX\Ambiance\WIND.WAV)", {}}};
    REQUIRE(pistoris::sounds::repairPaths(replacement) == pistoris::sounds::Error::kNone);
    pistoris::sounds::replaceSounds(data, std::move(replacement));
    REQUIRE(pistoris::sounds::validate(data) == pistoris::sounds::Error::kNone);
    CHECK(data.effect_paths[0] == "sfx/ambiance/wind.wav");
  }

  TEST_CASE("Validates encoded audio before mutation") {
    pistoris::SoundsData data;
    const pistoris::SoundIndex index = pistoris::sounds::addSound(data, {"sfx/test.wav", makePcm16Wav(2)});
    const std::vector<std::uint8_t> invalid = {1, 2, 3};
    CHECK(pistoris::sounds::validateEncodedAudio(invalid) == pistoris::sounds::Error::kBadAudio);
    CHECK(effectAudio(data, index).size() == makePcm16Wav(2).size());
  }

  TEST_CASE("Prepares requested sound variants in request order") {
    pistoris::SoundsData data = effectSounds({{"sfx/test.wav", makePcm16Wav(2)}});
    const pistoris::SoundHandle sound = pistoris::sounds::effectHandle(0);
    const std::vector<pistoris::sounds::AudioPreparationRequest> requests = {
        {sound,
         pistoris::kSoundEffects,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kMono}},
        {sound,
         pistoris::kSoundEffects,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kPreserve}},
        {sound,
         pistoris::kSoundEffects,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kMono,
          false}},
    };
    std::vector<pistoris::sounds::PreparedAudio> prepared;
    REQUIRE(pistoris::sounds::prepareAudio(data, requests, prepared) == pistoris::sounds::Error::kNone);

    REQUIRE(prepared.size() == requests.size());
    CHECK(prepared[0].source.channels == 2);
    CHECK(prepared[0].output.channels == 1);
    CHECK_FALSE(prepared[0].bytes.converted.empty());
    CHECK(prepared[1].source.channels == 2);
    CHECK(prepared[1].output.channels == 2);
    CHECK(prepared[1].bytes.converted.empty());
    CHECK(prepared[1].bytes.borrowed.data() == effectAudio(data, 0).data());
    CHECK(prepared[2].source.channels == 2);
    CHECK(prepared[2].output.channels == 1);
    CHECK(prepared[2].bytes.data().empty());
  }

  TEST_CASE("Prepares interleaved sound requests in request order") {
    pistoris::SoundsData data = effectSounds({{"sfx/stereo.wav", makePcm16Wav(2)}, {"sfx/mono.wav", makePcm16Wav(1)}});
    const std::vector<pistoris::sounds::AudioPreparationRequest> requests = {
        {pistoris::sounds::effectHandle(0),
         pistoris::kSoundEffects,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kMono}},
        {pistoris::sounds::effectHandle(1),
         pistoris::kSoundEffects,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kPreserve}},
        {pistoris::sounds::effectHandle(0),
         pistoris::kSoundEffects,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kPreserve}},
    };

    std::vector<pistoris::sounds::PreparedAudio> prepared;
    REQUIRE(pistoris::sounds::prepareAudio(data, requests, prepared) == pistoris::sounds::Error::kNone);
    REQUIRE(prepared.size() == requests.size());
    CHECK(prepared[0].source.channels == 2);
    CHECK(prepared[0].output.channels == 1);
    CHECK_FALSE(prepared[0].bytes.converted.empty());
    CHECK(prepared[1].source.channels == 1);
    CHECK(prepared[1].output.channels == 1);
    CHECK(prepared[1].bytes.borrowed.data() == effectAudio(data, 1).data());
    CHECK(prepared[2].source.channels == 2);
    CHECK(prepared[2].output.channels == 2);
    CHECK(prepared[2].bytes.borrowed.data() == effectAudio(data, 0).data());
  }

  TEST_CASE("Sound preparation does not overwrite output on failure") {
    pistoris::SoundsData data = effectSounds({{"sfx/test.wav", {1, 2, 3}}});
    const pistoris::sounds::AudioPreparationRequest request{
        pistoris::sounds::effectHandle(0),
        pistoris::kSoundEffects,
        {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav)}};
    std::vector<pistoris::sounds::PreparedAudio> prepared(1);
    prepared[0].bytes.converted = {42};

    CHECK(pistoris::sounds::prepareAudio(data, {&request, 1}, prepared) == pistoris::sounds::Error::kBadAudio);
    REQUIRE(prepared.size() == 1);
    CHECK(prepared[0].bytes.converted == std::vector<std::uint8_t>{42});
  }

  TEST_CASE("Mono audio can provide both preserved and mono variants") {
    const std::vector<std::uint8_t> source = makePcm16Wav(1);
    std::vector<std::uint8_t> preserved = {1};
    std::vector<std::uint8_t> mono = {2};
    pistoris::audio::Info source_info{.channels = 99};

    REQUIRE(pistoris::audio::transcodeToPcm16WavVariants(source, true, true, &preserved, &mono, &source_info) ==
            pistoris::audio::Error::kNone);
    CHECK_FALSE(preserved.empty());
    CHECK(mono == preserved);
    CHECK(source_info.channels == 1);
  }

  TEST_CASE("Audio variant output rejects aliasing transactionally") {
    const std::vector<std::uint8_t> source = makePcm16Wav(1);
    std::vector<std::uint8_t> output = {42};
    pistoris::audio::Info source_info{.channels = 99};

    CHECK(pistoris::audio::transcodeToPcm16WavVariants(source, true, true, &output, &output, &source_info) ==
          pistoris::audio::Error::kMalformed);
    CHECK(output == std::vector<std::uint8_t>{42});
    CHECK(source_info.channels == 99);
  }

  TEST_CASE("Rebases paths without losing duplicate basenames") {
    pistoris::SoundsData data = effectSounds({{"first/wind.wav", {}}, {"second/wind.wav", {}}});
    pistoris::sounds::PathRebaseInfo info;
    REQUIRE(pistoris::sounds::rebasePaths(data, R"(sfx\ambiance)", &info) == pistoris::sounds::Error::kNone);
    CHECK(data.effect_paths[0] == "sfx/ambiance/wind.wav");
    CHECK(data.effect_paths[1] == "sfx/ambiance/wind_1.wav");
    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "second/wind.wav");
    CHECK(info.repairs[0].repaired == "sfx/ambiance/wind_1.wav");
  }

  TEST_CASE("Rebase suffixes do not displace natural basenames") {
    pistoris::SoundsData data = effectSounds({{"first/a.wav", {}}, {"second/a.wav", {}}, {"third/a_1.wav", {}}});
    REQUIRE(pistoris::sounds::rebasePaths(data, "sfx/ambiance") == pistoris::sounds::Error::kNone);
    CHECK(data.effect_paths[0] == "sfx/ambiance/a.wav");
    CHECK(data.effect_paths[1] == "sfx/ambiance/a_2.wav");
    CHECK(data.effect_paths[2] == "sfx/ambiance/a_1.wav");
  }

  TEST_CASE("Rebase accepts root and trailing directories") {
    pistoris::SoundsData rooted = effectSounds({{"folder/wind.wav", {}}});
    REQUIRE(pistoris::sounds::rebasePaths(rooted, {}) == pistoris::sounds::Error::kNone);
    CHECK(rooted.effect_paths[0] == "wind.wav");

    pistoris::SoundsData nested = effectSounds({{"folder/wind.wav", {}}});
    REQUIRE(pistoris::sounds::rebasePaths(nested, R"(SFX\Ambiance\)") == pistoris::sounds::Error::kNone);
    CHECK(nested.effect_paths[0] == "sfx/ambiance/wind.wav");
  }

  TEST_CASE("Rebase reports structural directory repair once") {
    pistoris::SoundsData data = effectSounds({{"first/a.wav", {}}, {"second/b.wav", {}}});
    pistoris::sounds::PathRebaseInfo info;
    REQUIRE(pistoris::sounds::rebasePaths(data, "Custom?/Audio", &info) == pistoris::sounds::Error::kNone);

    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "Custom?/Audio");
    CHECK(info.repairs[0].repaired == "custom-/audio");
  }

  TEST_CASE("Invalid rebase directory preserves diagnostics") {
    pistoris::SoundsData data = effectSounds({{"folder/wind.wav", {}}});
    pistoris::sounds::PathRebaseInfo info;
    info.repairs.push_back({"old", "new"});

    CHECK(pistoris::sounds::rebasePaths(data, "bad//directory", &info) == pistoris::sounds::Error::kBadPath);
    CHECK(data.effect_paths[0] == "folder/wind.wav");
    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "old");
  }

  TEST_CASE("Rejects non-relative and non-portable paths transactionally") {
    pistoris::SoundsData data;
    for (std::string path : {"../wind.wav", "C:/wind.wav"}) {
      pistoris::Sound candidate{std::move(path), {}};
      CHECK(pistoris::sounds::repairPath(data, candidate, pistoris::kNoSound) == pistoris::sounds::Error::kBadPath);
    }
    const std::string original_long_path = std::string(252, 'a') + ".wav";
    pistoris::Sound long_path{original_long_path, {}};
    REQUIRE(pistoris::sounds::repairPath(data, long_path, pistoris::kNoSound) == pistoris::sounds::Error::kNone);
    CHECK(long_path.path != original_long_path);
    CHECK(pistoris::sounds::validateSound(long_path) == pistoris::sounds::Error::kNone);
    pistoris::Sound valid{"wind.wav", {}};
    REQUIRE(pistoris::sounds::repairPath(data, valid, pistoris::kNoSound) == pistoris::sounds::Error::kNone);
    REQUIRE(pistoris::sounds::validateSound(valid) == pistoris::sounds::Error::kNone);
    pistoris::sounds::addSound(data, std::move(valid));
    CHECK(pistoris::sounds::rebasePaths(data, "../outside") == pistoris::sounds::Error::kBadPath);
    CHECK(data.effect_paths[0] == "wind.wav");
  }

  TEST_CASE("Rejects invalid rebase directories without sounds") {
    pistoris::SoundsData data;
    CHECK(pistoris::sounds::rebasePaths(data, "../outside") == pistoris::sounds::Error::kBadPath);
    CHECK(pistoris::sounds::rebasePaths(data, "/absolute") == pistoris::sounds::Error::kBadPath);
  }

  TEST_CASE("Speech paths and encodings are grouped independently") {
    pistoris::SoundsData data;
    const pistoris::SoundHandle effect = pistoris::sounds::addPath(data, pistoris::SoundKind::kEffect, "shared/path");
    const pistoris::SoundHandle speech = pistoris::sounds::addPath(data, pistoris::SoundKind::kSpeech, "shared/path");
    pistoris::sounds::setLanguage(data, 1, "english");
    pistoris::sounds::setLanguage(data, 2, "french");
    pistoris::sounds::setEncodedAudio(data, effect, pistoris::kSoundEffects, makePcm16Wav(1));
    pistoris::sounds::setEncodedAudio(data, speech, 1, makePcm16Wav(1));
    pistoris::sounds::setEncodedAudio(data, speech, 2, makePcm16Wav(2));

    REQUIRE(pistoris::sounds::validate(data) == pistoris::sounds::Error::kNone);
    CHECK(pistoris::sounds::count(data, pistoris::SoundKind::kEffect) == 1);
    CHECK(pistoris::sounds::count(data, pistoris::SoundKind::kSpeech) == 1);
    CHECK(pistoris::sounds::encodedAudio(data, speech, 1).size() == makePcm16Wav(1).size());
    CHECK(pistoris::sounds::encodedAudio(data, speech, 2).size() == makePcm16Wav(2).size());

    pistoris::sounds::removeLanguage(data, 1);
    CHECK(pistoris::sounds::encodedAudio(data, speech, 1).empty());
    CHECK_FALSE(pistoris::sounds::encodedAudio(data, speech, 2).empty());
  }

  TEST_CASE("Language names are portable and unique by path identity") {
    pistoris::SoundsData data;
    data.languages.emplace(1, "English");
    data.languages.emplace(2, "english");
    CHECK(pistoris::sounds::validateStructure(data) == pistoris::sounds::Error::kBadLanguage);

    data.languages.erase(2);
    data.languages.at(1) = "con";
    CHECK(pistoris::sounds::validateStructure(data) == pistoris::sounds::Error::kBadLanguage);
  }

  TEST_CASE("Compaction remaps only the requested sound kind") {
    pistoris::SoundsData data;
    const pistoris::SoundHandle unused = pistoris::sounds::addPath(data, pistoris::SoundKind::kEffect, "unused");
    const pistoris::SoundHandle retained = pistoris::sounds::addPath(data, pistoris::SoundKind::kEffect, "retained");
    const pistoris::SoundHandle speech = pistoris::sounds::addPath(data, pistoris::SoundKind::kSpeech, "speech");
    pistoris::sounds::setLanguage(data, 1, "english");
    pistoris::sounds::setEncodedAudio(data, unused, pistoris::kSoundEffects, makePcm16Wav(1));
    pistoris::sounds::setEncodedAudio(data, retained, pistoris::kSoundEffects, makePcm16Wav(2));
    pistoris::sounds::setEncodedAudio(data, speech, 1, makePcm16Wav(1));

    std::vector<pistoris::SoundIndex> remap;
    std::size_t removed = 0;
    const std::vector<std::uint8_t> used = {0, 1};
    REQUIRE(pistoris::sounds::compact(data, pistoris::SoundKind::kEffect, used, remap, removed) ==
            pistoris::sounds::Error::kNone);
    REQUIRE(remap.size() == 2);
    CHECK(remap[0] == pistoris::kNoSound);
    CHECK(remap[1] == 0);
    CHECK(removed == 1);
    CHECK((pistoris::sounds::path(data, pistoris::sounds::effectHandle(0)) == "retained"));
    CHECK((pistoris::sounds::path(data, speech) == "speech"));
    CHECK_FALSE(pistoris::sounds::encodedAudio(data, pistoris::sounds::effectHandle(0)).empty());
    CHECK_FALSE(pistoris::sounds::encodedAudio(data, speech, 1).empty());
  }
}
