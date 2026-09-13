// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/pistoris.hpp"
#include "arx_pistoris/texture.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/fixture_catalog.h"
#include "support/fixture_resources.h"
#include "support/level_equivalence.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct LevelCorpusCoverage {
  bool textures = false;
  bool portals = false;
  bool room_distances = false;
  bool anchors = false;
  bool anchor_connections = false;
  bool lights = false;
  bool entities = false;
  bool fogs = false;
  bool zones = false;
  bool paths = false;

  void observe(const pistoris::Level& level) {
    textures = textures || level.textureCount() != 0;
    portals = portals || level.portalCount() != 0;
    room_distances = room_distances || level.roomDistanceCount() != 0;
    anchors = anchors || level.anchorCount() != 0;
    anchor_connections = anchor_connections || level.anchorConnectionCount() != 0;
    lights = lights || level.lightCount() != 0;
    entities = entities || level.entityCount() != 0;
    fogs = fogs || level.fogCount() != 0;
    zones = zones || level.zoneCount() != 0;
    paths = paths || level.pathCount() != 0;
  }

  void checkNative() const {
    CHECK(textures);
    CHECK(portals);
    CHECK(room_distances);
    CHECK(anchors);
    CHECK(anchor_connections);
    CHECK(lights);
    CHECK(entities);
    CHECK(fogs);
    CHECK(zones);
    CHECK(paths);
  }

  void checkGlb() const {
    CHECK(textures);
    CHECK(portals);
    CHECK(lights);
    CHECK(entities);
    CHECK(fogs);
    CHECK(zones);
    CHECK(paths);
  }
};

std::optional<std::size_t> checkNativeTriplet(const test_support::LevelNativeTriplet& paths,
                                              LevelCorpusCoverage* coverage) {
  CAPTURE(paths.fts.string());
  CAPTURE(paths.dlf.string());
  CAPTURE(paths.llf.string());

  pistoris::Fts fts;
  pistoris::Dlf dlf;
  pistoris::Llf llf;
  std::vector<std::uint8_t> fts_bytes;
  std::vector<std::uint8_t> dlf_bytes;
  std::vector<std::uint8_t> llf_bytes;
  if (!test_support::readCorpusBytes(paths.fts, fts_bytes) ||
      !test_support::checkCorpusStatus(paths.fts, "read FTS", pistoris::readFts(fts_bytes, fts)))
    return std::nullopt;
  if (!test_support::readCorpusBytes(paths.dlf, dlf_bytes) ||
      !test_support::checkCorpusStatus(paths.dlf, "read DLF", pistoris::readDlf(dlf_bytes, dlf)))
    return std::nullopt;
  if (!test_support::readCorpusBytes(paths.llf, llf_bytes) ||
      !test_support::checkCorpusStatus(paths.llf, "read LLF", pistoris::readLlf(llf_bytes, llf)))
    return std::nullopt;

  pistoris::Level level;
  std::vector<std::string> texture_source_paths;
  if (!test_support::checkCorpusStatus(paths.dlf,
                                       "import native triplet into Level",
                                       pistoris::Level::importNative(level, fts, &llf, &dlf, &texture_source_paths)))
    return std::nullopt;
  if (!test_support::checkCorpusStatus(paths.dlf, "validate Level", level.validate())) return std::nullopt;
  const std::optional<test_support::HydrationResult> hydration =
      test_support::hydrateTextures(level, test_support::nativeMount(paths.dlf), texture_source_paths, true);
  if (!hydration) return std::nullopt;
  if (coverage != nullptr) coverage->observe(level);

  const std::string level_name = paths.dlf.stem().string();
  pistoris::Level::NativeBakeOptions options;
  options.level_name = level_name;
  options.textures.include_files = true;
  pistoris::NativeLevelBundle baked;
  if (!test_support::checkCorpusStatus(
          paths.dlf, "bake Level to native bundle", level.bakeNativeBundle(options, baked)))
    return std::nullopt;
  if (!test_support::validateTextureFiles(level, std::span<const pistoris::NativeTextureFile>(baked.texture_files)))
    return std::nullopt;
  if (!test_support::checkCorpusStatus(paths.fts, "validate baked FTS", pistoris::validate(baked.fts)) ||
      !test_support::checkCorpusStatus(paths.llf, "validate baked LLF", pistoris::validate(baked.llf)) ||
      !test_support::checkCorpusStatus(paths.dlf, "validate baked DLF", pistoris::validate(baked.dlf)))
    return std::nullopt;

  pistoris::Level roundtrip;
  std::vector<std::string> roundtrip_sources;
  if (!test_support::checkCorpusStatus(
          paths.dlf,
          "import baked native triplet into Level",
          pistoris::Level::importNative(roundtrip, baked.fts, &baked.llf, &baked.dlf, &roundtrip_sources)))
    return std::nullopt;
  const std::optional<test_support::HydrationResult> roundtrip_hydration = test_support::hydrateTexturesFromFiles(
      roundtrip, roundtrip_sources, std::span<const pistoris::NativeTextureFile>(baked.texture_files));
  if (!roundtrip_hydration) return std::nullopt;
  if (!test_support::checkCorpusStatus(paths.dlf, "validate roundtrip Level", roundtrip.validate()))
    return std::nullopt;
  CHECK(roundtrip_hydration->hydrated >= hydration->hydrated);
  if (coverage != nullptr)
    test_support::checkLevelsEquivalent(level, roundtrip, {test_support::LevelEquivalenceDomain::kNativeBundle, 2e-4f});
  return hydration->hydrated;
}

}  // namespace

TEST_SUITE("level_corpus") {
  TEST_CASE("NativeTripletsBuildAndBakeLevel") {
    const std::vector<test_support::LevelNativeTriplet> triplets = test_support::levelNativeTriplets();
    REQUIRE_FALSE(triplets.empty());
    std::size_t fixture_hydrations = 0;
    LevelCorpusCoverage coverage;
    for (std::size_t index = 0; index < triplets.size(); ++index) {
      const bool is_fixture = test_support::isCommittedFixture(triplets[index].fts);
      const std::optional<std::size_t> hydrated = checkNativeTriplet(triplets[index], is_fixture ? &coverage : nullptr);
      if (is_fixture && hydrated) fixture_hydrations += *hydrated;
    }
    CHECK(fixture_hydrations > 0);
    coverage.checkNative();
  }

  TEST_CASE("CommittedGlbLevelsRoundtrip") {
    const test_support::FixtureCatalog& catalog = test_support::fixtureCatalog();
    REQUIRE_FALSE(catalog.levels.empty());
    LevelCorpusCoverage coverage;

    for (const test_support::LevelFixture& fixture : catalog.levels) {
      const fs::path& path = fixture.glb.path;
      CAPTURE(path.string());
      pistoris::Level::GlbImportOptions import_options;
      import_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      pistoris::Level level;
      std::vector<std::string> texture_source_paths;
      REQUIRE(pistoris::Level::importGlb(
                  level, test_support::readBytes(path), import_options, nullptr, &texture_source_paths) == ARX_OK);
      REQUIRE(level.validate() == ARX_OK);
      if (!test_support::hydrateTextures(level, path.parent_path(), texture_source_paths)) continue;
      coverage.observe(level);
      if (fixture.name == "level9") {
        CHECK(level.faceCount() == 2);
        CHECK(level.zoneCount() == 1);
      }

      std::vector<std::uint8_t> written;
      pistoris::Level::GlbExportOptions export_options;
      export_options.arx_units_per_glb_unit = fixture.glb.arx_units_per_glb_unit;
      REQUIRE(level.exportGlb(written, export_options) == ARX_OK);
      pistoris::Level roundtrip;
      REQUIRE(pistoris::Level::importGlb(roundtrip, written, import_options) == ARX_OK);
      CHECK(roundtrip.validate() == ARX_OK);
      test_support::checkLevelsEquivalent(level, roundtrip, {test_support::LevelEquivalenceDomain::kGlb, 1e-4f});
    }
    coverage.checkGlb();
  }
}
