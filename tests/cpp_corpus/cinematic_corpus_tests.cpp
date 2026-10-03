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
#include <utility>
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
      auto native_result = pistoris::readCin(source_bytes);
      if (!test_support::checkCorpusStatus(path, "read CIN", native_result)) continue;
      pistoris::Cin native = std::move(*native_result);

      std::vector<std::string> illustration_sources;
      std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
      auto imported = pistoris::Cinematic::importNative(native, &illustration_sources, &sound_sources);
      if (!test_support::checkCorpusStatus(path, "import CIN into Cinematic", imported)) continue;
      pistoris::Cinematic cinematic = std::move(*imported);
      if (!test_support::checkCorpusStatus(path, "validate Cinematic", cinematic.validate())) continue;
      CHECK(illustration_sources.size() == cinematic.textureCount());

      auto baked_result = cinematic.bakeNative();
      if (!test_support::checkCorpusStatus(path, "bake Cinematic", baked_result)) continue;
      pistoris::Cin baked = std::move(*baked_result);
      auto roundtrip_result = pistoris::Cinematic::importNative(baked);
      if (!test_support::checkCorpusStatus(path, "import baked CIN", roundtrip_result)) continue;
      pistoris::Cinematic roundtrip = std::move(*roundtrip_result);
      if (!test_support::checkCorpusStatus(path, "validate roundtrip Cinematic", roundtrip.validate())) continue;

      auto rebaked = roundtrip.bakeNative();
      if (!test_support::checkCorpusStatus(path, "rebake Cinematic", rebaked)) continue;
      test_support::checkEquivalent(baked, *rebaked, {.comparison_epsilon = 1.0e-4f});
    }
  }

  TEST_CASE("GLB fixtures convert through Cinematic") {
    for (const test_support::CinematicFixture& fixture : test_support::fixtureCatalog().cinematics) {
      CAPTURE(fixture.glb.string());
      std::vector<pistoris::CinematicSoundSourceReference> sound_sources;
      auto imported = pistoris::Cinematic::importGlb(test_support::readBytes(fixture.glb), &sound_sources);
      REQUIRE(imported);
      pistoris::Cinematic cinematic = std::move(*imported);
      REQUIRE(cinematic.validate());
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

      const auto baked = cinematic.bakeNative();
      REQUIRE(baked);
      const auto encoded = cinematic.exportGlb();
      REQUIRE(encoded);
      auto roundtrip_result = pistoris::Cinematic::importGlb(*encoded);
      REQUIRE(roundtrip_result);
      pistoris::Cinematic roundtrip = std::move(*roundtrip_result);
      REQUIRE(roundtrip.validate());
      const auto rebaked = roundtrip.bakeNative();
      REQUIRE(rebaked);
      test_support::checkEquivalent(*baked, *rebaked, {.comparison_epsilon = 1.0e-4f});
    }
  }
}
