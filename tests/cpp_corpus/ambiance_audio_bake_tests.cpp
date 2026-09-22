// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/audio.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/sound.hpp"

#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

TEST_SUITE("ambiance_corpus") {
  TEST_CASE("GLB ambiance audio bakes to native WAV") {
    std::size_t mp3_sources = 0;
    std::size_t ogg_sources = 0;

    for (const test_support::AmbianceFixture& fixture : test_support::fixtureCatalog().ambiances) {
      if (fixture.glb.empty()) continue;
      CAPTURE(fixture.glb.path.string());

      pistoris::Ambiance::GlbImportOptions options;
      options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      pistoris::Ambiance ambiance;
      std::vector<pistoris::SoundSourceReference> sources;
      REQUIRE(pistoris::Ambiance::importGlb(ambiance, test_support::readBytes(fixture.glb.path), options, &sources) ==
              ARX_OK);
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateSounds(ambiance, fixture.glb.path.parent_path(), sources);
      REQUIRE(hydration.has_value());

      std::vector<ArxSoundView> sounds(ambiance.soundCount());
      if (!sounds.empty()) REQUIRE(ambiance.copySoundViews(0, sounds.size(), sounds.data()) == ARX_OK);
      std::vector<pistoris::SoundIndex> converted_sources;
      for (std::size_t index = 0; index < sounds.size(); ++index) {
        const ArxSoundView& sound = sounds[index];
        if (sound.encoded_audio.size == 0) continue;
        ArxAudioInfo info{};
        REQUIRE(pistoris::binary::inspectEncodedAudio({sound.encoded_audio.data, sound.encoded_audio.size}, info) ==
                ARX_OK);
        if (info.format == ARX_AUDIO_FORMAT_MP3) {
          ++mp3_sources;
          converted_sources.push_back(static_cast<pistoris::SoundIndex>(index));
        }
        if (info.format == ARX_AUDIO_FORMAT_OGG_VORBIS) {
          ++ogg_sources;
          converted_sources.push_back(static_cast<pistoris::SoundIndex>(index));
        }
      }

      pistoris::NativeAmbianceBundle bundle;
      REQUIRE(ambiance.bakeNativeBundle({.include_sound_files = true}, bundle) == ARX_OK);
      REQUIRE(pistoris::validate(bundle.amb) == ARX_OK);
      REQUIRE(test_support::validateSoundFiles(ambiance, std::span<const pistoris::SoundFile>(bundle.sound_files)));
      for (const pistoris::SoundIndex source : converted_sources) {
        bool emitted = false;
        for (const pistoris::SoundFile& file : bundle.sound_files) emitted = emitted || file.source_sound == source;
        CHECK(emitted);
      }
      for (const pistoris::SoundFile& file : bundle.sound_files) {
        CAPTURE(file.path);
        CHECK(std::filesystem::path(file.path).extension() == ".wav");
        ArxAudioInfo info{};
        REQUIRE(pistoris::binary::inspectEncodedAudio(file.encoded_audio, info) == ARX_OK);
        CHECK(info.format == ARX_AUDIO_FORMAT_WAV);
      }
    }

    CHECK(mp3_sources > 0);
    CHECK(ogg_sources > 0);
  }
}
