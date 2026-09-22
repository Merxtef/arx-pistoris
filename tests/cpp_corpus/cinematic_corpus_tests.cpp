// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/cinematic/sound.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/sound.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"
#include "support/native_equivalence.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

TEST_SUITE("cinematic_corpus") {
  TEST_CASE("Native CIN converts through Cinematic") {
    const std::vector<std::filesystem::path> files =
        test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kCin);
    REQUIRE_FALSE(files.empty());
    for (const std::filesystem::path& path : files) {
      CAPTURE(path.string());
      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Cin native;
      if (!test_support::checkCorpusStatus(path, "read CIN", pistoris::readCin(source_bytes, native))) continue;

      pistoris::Cinematic cinematic;
      std::vector<std::string> illustration_sources;
      std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
      if (!test_support::checkCorpusStatus(
              path,
              "import CIN into Cinematic",
              pistoris::Cinematic::importNative(cinematic, native, &illustration_sources, &sound_sources)))
        continue;
      if (!test_support::checkCorpusStatus(path, "validate Cinematic", cinematic.validate())) continue;
      CHECK(illustration_sources.size() == cinematic.textureCount());

      pistoris::Cin baked;
      if (!test_support::checkCorpusStatus(path, "bake Cinematic", cinematic.bakeNative(baked))) continue;
      pistoris::Cinematic roundtrip;
      if (!test_support::checkCorpusStatus(
              path, "import baked CIN", pistoris::Cinematic::importNative(roundtrip, baked)))
        continue;
      if (!test_support::checkCorpusStatus(path, "validate roundtrip Cinematic", roundtrip.validate())) continue;

      pistoris::Cin rebaked;
      if (!test_support::checkCorpusStatus(path, "rebake Cinematic", roundtrip.bakeNative(rebaked))) continue;
      test_support::checkEquivalent(baked, rebaked);
    }
  }

  TEST_CASE("GLB fixtures convert through Cinematic") {
    for (const test_support::CinematicFixture& fixture : test_support::fixtureCatalog().cinematics) {
      CAPTURE(fixture.glb.string());
      pistoris::Cinematic cinematic;
      std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
      REQUIRE(pistoris::Cinematic::importGlb(cinematic, test_support::readBytes(fixture.glb), &sound_sources) ==
              ARX_OK);
      REQUIRE(cinematic.validate() == ARX_OK);
      std::size_t effect_references = 0;
      std::size_t speech_references = 0;
      for (const pistoris::CinematicSoundSourceReference& source : sound_sources) {
        pistoris::SoundKind kind = pistoris::SoundKind::kEffect;
        REQUIRE(pistoris::soundHandleKind(source.sound, kind) == ARX_OK);
        if (kind == pistoris::SoundKind::kEffect)
          ++effect_references;
        else
          ++speech_references;
      }
      CHECK(effect_references == fixture.audio.effect_references);
      CHECK(speech_references == fixture.audio.speech_references);

      pistoris::Cin baked;
      REQUIRE(cinematic.bakeNative(baked) == ARX_OK);
      std::vector<std::uint8_t> encoded;
      REQUIRE(cinematic.exportGlb(encoded) == ARX_OK);
      pistoris::Cinematic roundtrip;
      REQUIRE(pistoris::Cinematic::importGlb(roundtrip, encoded) == ARX_OK);
      REQUIRE(roundtrip.validate() == ARX_OK);
      pistoris::Cin rebaked;
      REQUIRE(roundtrip.bakeNative(rebaked) == ARX_OK);
      test_support::checkEquivalent(baked, rebaked, {.comparison_epsilon = 1.0e-4f});
    }
  }
}
