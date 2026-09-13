// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/sound.hpp"

#include "support/ambiance_equivalence.h"
#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

TEST_SUITE("ambiance_corpus") {
  TEST_CASE("Native AMB converts through Ambiance") {
    std::size_t fixture_hydrations = 0;
    for (const std::filesystem::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kAmb)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Amb native;
      if (!test_support::checkCorpusStatus(path, "read AMB", pistoris::readAmb(source_bytes, native))) continue;
      pistoris::Ambiance ambiance;
      std::vector<pistoris::SoundSourceReference> sound_sources;
      if (!test_support::checkCorpusStatus(
              path, "import AMB into Ambiance", pistoris::Ambiance::importNative(ambiance, native, &sound_sources)))
        continue;
      if (!test_support::checkCorpusStatus(path, "validate Ambiance", ambiance.validate())) continue;
      const std::optional<test_support::HydrationResult> hydration = test_support::hydrateSounds(
          ambiance, test_support::nativeMount(path), sound_sources, test_support::SoundSourceLayout::kNativeAmbiance);
      if (!hydration) continue;
      if (test_support::isCommittedFixture(path)) fixture_hydrations += hydration->hydrated;

      pistoris::NativeAmbianceBundle baked;
      if (!test_support::checkCorpusStatus(
              path, "bake Ambiance to native bundle", ambiance.bakeNativeBundle({.include_files = true}, baked)))
        continue;
      if (!test_support::validateSoundFiles(ambiance, std::span<const pistoris::SoundFile>(baked.sound_files)))
        continue;

      pistoris::Ambiance roundtrip;
      std::vector<pistoris::SoundSourceReference> roundtrip_sources;
      if (!test_support::checkCorpusStatus(path,
                                           "import baked AMB into Ambiance",
                                           pistoris::Ambiance::importNative(roundtrip, baked.amb, &roundtrip_sources)))
        continue;
      const std::optional<test_support::HydrationResult> roundtrip_hydration =
          test_support::hydrateSoundsFromFiles(roundtrip,
                                               roundtrip_sources,
                                               std::span<const pistoris::SoundFile>(baked.sound_files),
                                               test_support::SoundSourceLayout::kNativeAmbiance);
      if (!roundtrip_hydration) continue;
      if (!test_support::checkCorpusStatus(path, "validate roundtrip Ambiance", roundtrip.validate())) continue;
      CHECK(roundtrip_hydration->hydrated >= hydration->hydrated);
      test_support::AmbianceEquivalenceOptions equivalence;
      equivalence.sounds.compare_paths = false;
      equivalence.sounds.compare_encoded_audio = false;
      test_support::checkAmbiancesEquivalent(ambiance, roundtrip, equivalence);
    }
    CHECK(fixture_hydrations > 0);
  }

  TEST_CASE("GLB fixtures convert through Ambiance") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    std::size_t fixture_hydrations = 0;
    for (const test_support::AmbianceFixture& fixture : catalog.ambiances) {
      if (fixture.glb.empty()) continue;
      CAPTURE(fixture.glb.path.string());

      pistoris::Ambiance::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      pistoris::Ambiance::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;

      pistoris::Ambiance ambiance;
      std::vector<pistoris::SoundSourceReference> sound_sources;
      REQUIRE(pistoris::Ambiance::importGlb(
                  ambiance, test_support::readBytes(fixture.glb.path), import_options, &sound_sources) == ARX_OK);
      REQUIRE(ambiance.validate() == ARX_OK);
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateSounds(ambiance, fixture.glb.path.parent_path(), sound_sources);
      if (!hydration) continue;
      fixture_hydrations += hydration->hydrated;

      pistoris::AmbianceGlbBundle bundle;
      REQUIRE(ambiance.exportGlbBundle(export_options, nullptr, bundle) == ARX_OK);
      if (!test_support::validateSoundFiles(ambiance, std::span<const pistoris::SoundFile>(bundle.sound_files)))
        continue;

      pistoris::Ambiance roundtrip;
      std::vector<pistoris::SoundSourceReference> roundtrip_sources;
      REQUIRE(pistoris::Ambiance::importGlb(roundtrip, bundle.glb, import_options, &roundtrip_sources) == ARX_OK);
      const std::optional<test_support::HydrationResult> roundtrip_hydration = test_support::hydrateSoundsFromFiles(
          roundtrip, roundtrip_sources, std::span<const pistoris::SoundFile>(bundle.sound_files));
      if (!roundtrip_hydration) continue;
      CHECK(roundtrip.validate() == ARX_OK);
      CHECK(roundtrip_hydration->hydrated >= hydration->hydrated);
      test_support::checkAmbiancesEquivalent(ambiance, roundtrip);
    }
    CHECK(fixture_hydrations > 0);
  }

  TEST_CASE("Ambiance GLB accepts its reference Model") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    std::size_t reference_cases = 0;
    for (const test_support::AmbianceFixture& fixture : catalog.ambiances) {
      if (fixture.glb.empty() || fixture.reference_model.empty()) continue;
      ++reference_cases;
      const auto model_fixture = std::ranges::find_if(catalog.models, [&](const test_support::ModelFixture& model) {
        return model.name == fixture.reference_model;
      });
      REQUIRE(model_fixture != catalog.models.end());

      pistoris::Ftl native_model;
      REQUIRE(pistoris::readFtl(test_support::readBytes(model_fixture->ftl), native_model) == ARX_OK);
      pistoris::Model reference_model;
      REQUIRE(pistoris::Model::importNative(reference_model, native_model) == ARX_OK);

      pistoris::Ambiance ambiance;
      pistoris::Ambiance::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      REQUIRE(pistoris::Ambiance::importGlb(ambiance, test_support::readBytes(fixture.glb.path), import_options) ==
              ARX_OK);
      pistoris::AmbianceGlbBundle bundle;
      pistoris::Ambiance::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      REQUIRE(ambiance.exportGlbBundle(export_options, &reference_model, bundle) == ARX_OK);
      pistoris::Ambiance roundtrip;
      REQUIRE(pistoris::Ambiance::importGlb(roundtrip, bundle.glb, import_options) == ARX_OK);
      CHECK(roundtrip.validate() == ARX_OK);
      test_support::checkAmbiancesEquivalent(ambiance, roundtrip);
    }
    CHECK(reference_cases > 0);
  }
}
