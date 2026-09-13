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

namespace fs = std::filesystem;

TEST_SUITE("amb_corpus") {
  TEST_CASE("ArxAmbReadWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kAmb)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Amb source;
      if (!test_support::checkCorpusStatus(path, "read AMB", pistoris::readAmb(source_bytes, source))) continue;
      if (!test_support::checkCorpusStatus(path, "validate AMB", pistoris::validate(source))) continue;

      std::vector<std::uint8_t> written;
      if (!test_support::checkCorpusStatus(path, "write AMB", pistoris::writeAmb(source, written))) continue;

      pistoris::Amb roundtrip;
      if (!test_support::checkCorpusStatus(path, "read written AMB", pistoris::readAmb(written, roundtrip))) continue;
      test_support::checkEquivalent(source, roundtrip);
    }
  }
}
