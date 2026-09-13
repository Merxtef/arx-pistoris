// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "audio_helpers.h"
#include "modules/sounds.h"
#include "utils/audio.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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
    CHECK(data.sounds[0].path == "sfx/ambiance/wind.wav");

    pistoris::Sound duplicate{"SFX/AMBIANCE/wind.WAV", {}};
    REQUIRE(pistoris::sounds::repairPath(data, duplicate, pistoris::kNoSound) == pistoris::sounds::Error::kNone);
    CHECK(duplicate.path == "sfx/ambiance/wind_1.wav");
  }

  TEST_CASE("Normalizes replacement paths before committing") {
    pistoris::SoundsData data{{{"old.wav", {}}}};
    std::vector<pistoris::Sound> replacement{{R"(SFX\Ambiance\WIND.WAV)", {}}};
    REQUIRE(pistoris::sounds::repairPaths(replacement) == pistoris::sounds::Error::kNone);
    REQUIRE(pistoris::sounds::validate(replacement) == pistoris::sounds::Error::kNone);
    pistoris::sounds::replaceSounds(data, std::move(replacement));
    CHECK(data.sounds[0].path == "sfx/ambiance/wind.wav");
  }

  TEST_CASE("Validates encoded audio before mutation") {
    pistoris::SoundsData data;
    const pistoris::SoundIndex index = pistoris::sounds::addSound(data, {"sfx/test.wav", makePcm16Wav(2)});
    const std::vector<std::uint8_t> invalid = {1, 2, 3};
    CHECK(pistoris::sounds::validateEncodedAudio(invalid) == pistoris::sounds::Error::kBadAudio);
    CHECK(data.sounds[index].encoded_audio.size() == makePcm16Wav(2).size());
  }

  TEST_CASE("Prepares requested sound variants in request order") {
    pistoris::SoundsData data{{{"sfx/test.wav", makePcm16Wav(2)}}};
    const std::vector<pistoris::sounds::AudioPreparationRequest> requests = {
        {0,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kMono}},
        {0,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kPreserve}},
        {0,
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
    CHECK(prepared[1].bytes.borrowed.data() == data.sounds[0].encoded_audio.data());
    CHECK(prepared[2].source.channels == 2);
    CHECK(prepared[2].output.channels == 1);
    CHECK(prepared[2].bytes.data().empty());
  }

  TEST_CASE("Prepares interleaved sound requests in request order") {
    pistoris::SoundsData data{{{"sfx/stereo.wav", makePcm16Wav(2)}, {"sfx/mono.wav", makePcm16Wav(1)}}};
    const std::vector<pistoris::sounds::AudioPreparationRequest> requests = {
        {0,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kMono}},
        {1,
         {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav),
          pistoris::sounds::AudioFormat::kWav,
          pistoris::sounds::ChannelMode::kPreserve}},
        {0,
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
    CHECK(prepared[1].bytes.borrowed.data() == data.sounds[1].encoded_audio.data());
    CHECK(prepared[2].source.channels == 2);
    CHECK(prepared[2].output.channels == 2);
    CHECK(prepared[2].bytes.borrowed.data() == data.sounds[0].encoded_audio.data());
  }

  TEST_CASE("Sound preparation does not overwrite output on failure") {
    pistoris::SoundsData data{{{"sfx/test.wav", {1, 2, 3}}}};
    const pistoris::sounds::AudioPreparationRequest request{
        0, {pistoris::sounds::audioFormatFlag(pistoris::sounds::AudioFormat::kWav)}};
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
    pistoris::SoundsData data{{{"first/wind.wav", {}}, {"second/wind.wav", {}}}};
    pistoris::sounds::PathRebaseInfo info;
    REQUIRE(pistoris::sounds::rebasePaths(data, R"(sfx\ambiance)", &info) == pistoris::sounds::Error::kNone);
    CHECK(data.sounds[0].path == "sfx/ambiance/wind.wav");
    CHECK(data.sounds[1].path == "sfx/ambiance/wind_1.wav");
    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "second/wind.wav");
    CHECK(info.repairs[0].repaired == "sfx/ambiance/wind_1.wav");
  }

  TEST_CASE("Rebase suffixes do not displace natural basenames") {
    pistoris::SoundsData data{{{"first/a.wav", {}}, {"second/a.wav", {}}, {"third/a_1.wav", {}}}};
    REQUIRE(pistoris::sounds::rebasePaths(data, "sfx/ambiance") == pistoris::sounds::Error::kNone);
    CHECK(data.sounds[0].path == "sfx/ambiance/a.wav");
    CHECK(data.sounds[1].path == "sfx/ambiance/a_2.wav");
    CHECK(data.sounds[2].path == "sfx/ambiance/a_1.wav");
  }

  TEST_CASE("Rebase accepts root and trailing directories") {
    pistoris::SoundsData rooted{{{"folder/wind.wav", {}}}};
    REQUIRE(pistoris::sounds::rebasePaths(rooted, {}) == pistoris::sounds::Error::kNone);
    CHECK(rooted.sounds[0].path == "wind.wav");

    pistoris::SoundsData nested{{{"folder/wind.wav", {}}}};
    REQUIRE(pistoris::sounds::rebasePaths(nested, R"(SFX\Ambiance\)") == pistoris::sounds::Error::kNone);
    CHECK(nested.sounds[0].path == "sfx/ambiance/wind.wav");
  }

  TEST_CASE("Rebase reports structural directory repair once") {
    pistoris::SoundsData data{{{"first/a.wav", {}}, {"second/b.wav", {}}}};
    pistoris::sounds::PathRebaseInfo info;
    REQUIRE(pistoris::sounds::rebasePaths(data, "Custom?/Audio", &info) == pistoris::sounds::Error::kNone);

    REQUIRE(info.repairs.size() == 1);
    CHECK(info.repairs[0].original == "Custom?/Audio");
    CHECK(info.repairs[0].repaired == "custom-/audio");
  }

  TEST_CASE("Invalid rebase directory preserves diagnostics") {
    pistoris::SoundsData data{{{"folder/wind.wav", {}}}};
    pistoris::sounds::PathRebaseInfo info;
    info.repairs.push_back({"old", "new"});

    CHECK(pistoris::sounds::rebasePaths(data, "bad//directory", &info) == pistoris::sounds::Error::kBadPath);
    CHECK(data.sounds[0].path == "folder/wind.wav");
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
    CHECK(data.sounds[0].path == "wind.wav");
  }

  TEST_CASE("Rejects invalid rebase directories without sounds") {
    pistoris::SoundsData data;
    CHECK(pistoris::sounds::rebasePaths(data, "../outside") == pistoris::sounds::Error::kBadPath);
    CHECK(pistoris::sounds::rebasePaths(data, "/absolute") == pistoris::sounds::Error::kBadPath);
  }
}
