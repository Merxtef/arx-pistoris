// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

TEST_SUITE("llf_corpus") {
  TEST_CASE("ArxLlfParse") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kLlf)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> llf_bytes;
      if (!test_support::readCorpusBytes(path, llf_bytes)) continue;
      auto llf = pistoris::readLlf(llf_bytes);
      if (!test_support::checkCorpusStatus(path, "read LLF", llf)) continue;
      test_support::checkCorpusStatus(path, "validate LLF", pistoris::validate(*llf));
    }
  }

  TEST_CASE("ArxLlfWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kLlf)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      auto source = pistoris::readLlf(source_bytes);
      if (!test_support::checkCorpusStatus(path, "read source LLF", source)) continue;

      auto written = pistoris::writeLlf(*source, {});
      if (!test_support::checkCorpusStatus(path, "write LLF", written)) continue;

      auto roundtrip = pistoris::readLlf(*written);
      if (!test_support::checkCorpusStatus(path, "read written LLF", roundtrip)) continue;
      if (!test_support::checkCorpusStatus(path, "validate written LLF", pistoris::validate(*roundtrip))) continue;
      test_support::checkEquivalent(*source, *roundtrip);
    }
  }
}
