// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> readBytes(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

}  // namespace

TEST_SUITE("llf_corpus") {
  TEST_CASE("ArxLlfParse") {
    for (const fs::path& path :
         test_support::discoverCorpusFiles({"data/fixtures/level/llf/native", "data/arx/llf"}, ".llf")) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> llf_bytes = readBytes(path);
      pistoris::Llf llf;
      REQUIRE(pistoris::readLlf(llf_bytes, llf) == ARX_OK);
      CHECK(pistoris::validate(llf) == ARX_OK);
    }
  }

  TEST_CASE("ArxLlfWriteRoundtrip") {
    for (const fs::path& path :
         test_support::discoverCorpusFiles({"data/fixtures/level/llf/native", "data/arx/llf"}, ".llf")) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes = readBytes(path);
      pistoris::Llf source;
      REQUIRE(pistoris::readLlf(source_bytes, source) == ARX_OK);

      std::vector<std::uint8_t> written;
      REQUIRE(pistoris::writeLlf(source, {}, written) == ARX_OK);

      pistoris::Llf roundtrip;
      REQUIRE(pistoris::readLlf(written, roundtrip) == ARX_OK);
      CHECK(pistoris::validate(roundtrip) == ARX_OK);
      test_support::checkEquivalent(source, roundtrip);
    }
  }
}
