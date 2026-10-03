// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
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
#include <utility>
#include <vector>

TEST_SUITE("ambiance_corpus") {
  TEST_CASE("Native AMB converts through Ambiance") {
    std::size_t fixture_hydrations = 0;
    for (const std::filesystem::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kAmb)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      auto native_result = pistoris::readAmb(source_bytes);
      if (!test_support::checkCorpusStatus(path, "read AMB", native_result)) continue;
      pistoris::Amb native = std::move(*native_result);
      std::vector<pistoris::SoundSourceReference> sound_sources;
      auto imported = pistoris::Ambiance::importNative(native, &sound_sources);
      if (!test_support::checkCorpusStatus(path, "import AMB into Ambiance", imported)) continue;
      pistoris::Ambiance ambiance = std::move(*imported);
      if (!test_support::checkCorpusStatus(path, "validate Ambiance", ambiance.validate())) continue;
      const std::optional<test_support::HydrationResult> hydration = test_support::hydrateSounds(
          ambiance, test_support::nativeMount(path), sound_sources, test_support::SoundSourceLayout::kNativeAmbiance);
      if (!hydration) continue;
      if (test_support::isCommittedFixture(path)) fixture_hydrations += hydration->hydrated;

      auto baked_result = ambiance.bakeNativeBundle({.include_sound_files = true});
      if (!test_support::checkCorpusStatus(path, "bake Ambiance to native bundle", baked_result)) continue;
      pistoris::NativeAmbianceBundle baked = std::move(*baked_result);
      if (!test_support::validateSoundFiles(ambiance, std::span<const pistoris::SoundFile>(baked.sound_files)))
        continue;

      std::vector<pistoris::SoundSourceReference> roundtrip_sources;
      auto roundtrip_result = pistoris::Ambiance::importNative(baked.amb, &roundtrip_sources);
      if (!test_support::checkCorpusStatus(path, "import baked AMB into Ambiance", roundtrip_result)) continue;
      pistoris::Ambiance roundtrip = std::move(*roundtrip_result);
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

      std::vector<pistoris::SoundSourceReference> sound_sources;
      auto imported =
          pistoris::Ambiance::importGlb(test_support::readBytes(fixture.glb.path), import_options, &sound_sources);
      REQUIRE(imported);
      pistoris::Ambiance ambiance = std::move(*imported);
      REQUIRE(ambiance.validate());
      const std::optional<test_support::HydrationResult> hydration =
          test_support::hydrateSounds(ambiance, fixture.glb.path.parent_path(), sound_sources);
      if (!hydration) continue;
      fixture_hydrations += hydration->hydrated;

      auto bundle_result = ambiance.exportGlbBundle(export_options, nullptr);
      REQUIRE(bundle_result);
      pistoris::AmbianceGlbBundle bundle = std::move(*bundle_result);
      if (!test_support::validateSoundFiles(ambiance, std::span<const pistoris::SoundFile>(bundle.sound_files)))
        continue;

      std::vector<pistoris::SoundSourceReference> roundtrip_sources;
      auto roundtrip_result = pistoris::Ambiance::importGlb(bundle.glb, import_options, &roundtrip_sources);
      REQUIRE(roundtrip_result);
      pistoris::Ambiance roundtrip = std::move(*roundtrip_result);
      const std::optional<test_support::HydrationResult> roundtrip_hydration = test_support::hydrateSoundsFromFiles(
          roundtrip, roundtrip_sources, std::span<const pistoris::SoundFile>(bundle.sound_files));
      if (!roundtrip_hydration) continue;
      CHECK(roundtrip.validate());
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

      auto native_model = pistoris::readFtl(test_support::readBytes(model_fixture->ftl));
      REQUIRE(native_model);
      auto reference_model_result = pistoris::Model::importNative(*native_model);
      REQUIRE(reference_model_result);
      pistoris::Model reference_model = std::move(*reference_model_result);

      pistoris::Ambiance::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      auto imported = pistoris::Ambiance::importGlb(test_support::readBytes(fixture.glb.path), import_options);
      REQUIRE(imported);
      pistoris::Ambiance ambiance = std::move(*imported);
      pistoris::Ambiance::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      auto bundle_result = ambiance.exportGlbBundle(export_options, &reference_model);
      REQUIRE(bundle_result);
      auto roundtrip_result = pistoris::Ambiance::importGlb(bundle_result->glb, import_options);
      REQUIRE(roundtrip_result);
      pistoris::Ambiance roundtrip = std::move(*roundtrip_result);
      CHECK(roundtrip.validate());
      test_support::checkAmbiancesEquivalent(ambiance, roundtrip);
    }
    CHECK(reference_cases > 0);
  }
}
