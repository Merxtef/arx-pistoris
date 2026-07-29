// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::vector<std::uint8_t> readBytes(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}

}  // namespace

TEST_SUITE("dlf_corpus") {
  TEST_CASE("ArxDlfParse") {
    for (const fs::path& path :
         test_support::discoverCorpusFiles({"data/fixtures/level/dlf/native", "data/arx/dlf"}, ".dlf")) {
      CAPTURE(path.string());

      pistoris::Dlf dlf;
      std::optional<pistoris::Llf> lighting;
      REQUIRE(pistoris::readDlf(readBytes(path), dlf, &lighting) == ARX_OK);
      CHECK(pistoris::validate(dlf) == ARX_OK);
      if (lighting) CHECK(pistoris::validate(*lighting) == ARX_OK);
    }
  }

  TEST_CASE("ArxDlfWriteRoundtrip") {
    for (const fs::path& path :
         test_support::discoverCorpusFiles({"data/fixtures/level/dlf/native", "data/arx/dlf"}, ".dlf")) {
      CAPTURE(path.string());

      pistoris::Dlf source;
      std::optional<pistoris::Llf> source_lighting;
      REQUIRE(pistoris::readDlf(readBytes(path), source, &source_lighting) == ARX_OK);

      std::vector<std::uint8_t> written;
      pistoris::DlfWriteOptions options{source_lighting ? &*source_lighting : nullptr, {}};
      REQUIRE(pistoris::writeDlf(source, options, written) == ARX_OK);

      pistoris::Dlf roundtrip;
      std::optional<pistoris::Llf> roundtrip_lighting;
      REQUIRE(pistoris::readDlf(written, roundtrip, &roundtrip_lighting) == ARX_OK);
      CHECK(pistoris::validate(roundtrip) == ARX_OK);
      test_support::checkEquivalent(source, roundtrip);
      REQUIRE(source_lighting.has_value() == roundtrip_lighting.has_value());
      if (source_lighting) {
        CHECK(pistoris::validate(*roundtrip_lighting) == ARX_OK);
        test_support::checkEquivalent(*source_lighting, *roundtrip_lighting);
      }
    }
  }
}
