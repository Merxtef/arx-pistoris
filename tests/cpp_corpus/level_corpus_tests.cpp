// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_files.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> readBytes(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

void checkNativeTriplet(const test_support::LevelNativeTriplet& paths) {
  CAPTURE(paths.fts.string());
  CAPTURE(paths.dlf.string());
  CAPTURE(paths.llf.string());

  pistoris::Fts fts;
  pistoris::Dlf dlf;
  pistoris::Llf llf;
  REQUIRE(pistoris::readFts(readBytes(paths.fts), fts) == ARX_OK);
  REQUIRE(pistoris::readDlf(readBytes(paths.dlf), dlf) == ARX_OK);
  REQUIRE(pistoris::readLlf(readBytes(paths.llf), llf) == ARX_OK);

  pistoris::Level level;
  REQUIRE(pistoris::Level::fromNative(level, fts, &llf, &dlf) == ARX_OK);
  REQUIRE(level.validate() == ARX_OK);

  const std::string level_name = paths.fts.stem().string();
  pistoris::Level::NativeBakeOptions options;
  options.level_name = level_name;
  options.include_texture_files = false;
  pistoris::NativeLevelBundle baked;
  REQUIRE(level.bakeNativeBundle(options, baked) == ARX_OK);
  CHECK(pistoris::validate(baked.fts) == ARX_OK);
  CHECK(pistoris::validate(baked.llf) == ARX_OK);
  CHECK(pistoris::validate(baked.dlf) == ARX_OK);

  pistoris::Level roundtrip;
  REQUIRE(pistoris::Level::fromNative(roundtrip, baked.fts, &baked.llf, &baked.dlf) == ARX_OK);
  CHECK(roundtrip.validate() == ARX_OK);
}

}  // namespace

TEST_SUITE("level_corpus") {
  TEST_CASE("NativeTripletsBuildAndBakeLevel") {
    std::vector<test_support::LevelNativeTriplet> triplets = test_support::discoverLevelTriplets(
        "data/fixtures/level/fts/native", "data/fixtures/level/dlf/native", "data/fixtures/level/llf/native");
    REQUIRE_FALSE(triplets.empty());

    std::vector<test_support::LevelNativeTriplet> optional =
        test_support::discoverLevelTriplets("data/arx/fts", "data/arx/dlf", "data/arx/llf");
    triplets.insert(triplets.end(), optional.begin(), optional.end());
    for (const test_support::LevelNativeTriplet& triplet : triplets) checkNativeTriplet(triplet);
  }

  TEST_CASE("CommittedGlbLevelsRoundtrip") {
    const std::vector<fs::path> files = test_support::discoverCorpusFiles("data/fixtures/level/glb", ".glb");
    REQUIRE_FALSE(files.empty());

    for (const fs::path& path : files) {
      CAPTURE(path.string());
      pistoris::Level level;
      REQUIRE(pistoris::Level::fromGlb(level, readBytes(path)) == ARX_OK);
      REQUIRE(level.validate() == ARX_OK);

      std::vector<std::uint8_t> written;
      REQUIRE(level.exportGlb(written) == ARX_OK);
      pistoris::Level roundtrip;
      REQUIRE(pistoris::Level::fromGlb(roundtrip, written) == ARX_OK);
      CHECK(roundtrip.validate() == ARX_OK);
    }
  }
}
