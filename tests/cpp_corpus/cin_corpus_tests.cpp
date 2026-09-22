// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstdint>
#include <filesystem>
#include <vector>

TEST_SUITE("cin_corpus") {
  TEST_CASE("CIN files survive native read-write") {
    const std::vector<std::filesystem::path> files =
        test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kCin);
    REQUIRE_FALSE(files.empty());
    for (const std::filesystem::path& path : files) {
      CAPTURE(path.string());
      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Cin source;
      if (!test_support::checkCorpusStatus(path, "read CIN", pistoris::readCin(source_bytes, source))) continue;
      if (!test_support::checkCorpusStatus(path, "validate CIN", pistoris::validate(source))) continue;

      std::vector<std::uint8_t> written;
      if (!test_support::checkCorpusStatus(path, "write CIN", pistoris::writeCin(source, written))) continue;
      pistoris::Cin roundtrip;
      if (!test_support::checkCorpusStatus(path, "read written CIN", pistoris::readCin(written, roundtrip))) continue;
      test_support::checkEquivalent(source, roundtrip);
    }
  }
}
