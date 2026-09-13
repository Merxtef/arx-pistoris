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
      pistoris::Llf llf;
      if (!test_support::checkCorpusStatus(path, "read LLF", pistoris::readLlf(llf_bytes, llf))) continue;
      test_support::checkCorpusStatus(path, "validate LLF", pistoris::validate(llf));
    }
  }

  TEST_CASE("ArxLlfWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kLlf)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Llf source;
      if (!test_support::checkCorpusStatus(path, "read source LLF", pistoris::readLlf(source_bytes, source))) continue;

      std::vector<std::uint8_t> written;
      if (!test_support::checkCorpusStatus(path, "write LLF", pistoris::writeLlf(source, {}, written))) continue;

      pistoris::Llf roundtrip;
      if (!test_support::checkCorpusStatus(path, "read written LLF", pistoris::readLlf(written, roundtrip))) continue;
      if (!test_support::checkCorpusStatus(path, "validate written LLF", pistoris::validate(roundtrip))) continue;
      test_support::checkEquivalent(source, roundtrip);
    }
  }
}
