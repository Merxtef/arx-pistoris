// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

TEST_SUITE("dlf_corpus") {
  TEST_CASE("ArxDlfParse") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kDlf)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> bytes;
      if (!test_support::readCorpusBytes(path, bytes)) continue;
      auto bundle = pistoris::readDlf(bytes);
      if (!test_support::checkCorpusStatus(path, "read DLF", bundle)) continue;
      if (!test_support::checkCorpusStatus(path, "validate DLF", pistoris::validate(bundle->dlf))) continue;
      if (bundle->embedded_lighting)
        test_support::checkCorpusStatus(path, "validate embedded LLF", pistoris::validate(*bundle->embedded_lighting));
    }
  }

  TEST_CASE("ArxDlfWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kDlf)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      auto source = pistoris::readDlf(source_bytes);
      if (!test_support::checkCorpusStatus(path, "read source DLF", source)) continue;

      pistoris::DlfWriteOptions options{source->embedded_lighting ? &*source->embedded_lighting : nullptr, {}};
      auto written = pistoris::writeDlf(source->dlf, options);
      if (!test_support::checkCorpusStatus(path, "write DLF", written)) continue;

      auto roundtrip = pistoris::readDlf(*written);
      if (!test_support::checkCorpusStatus(path, "read written DLF", roundtrip)) continue;
      if (!test_support::checkCorpusStatus(path, "validate written DLF", pistoris::validate(roundtrip->dlf))) continue;
      test_support::checkEquivalent(source->dlf, roundtrip->dlf);
      if (!test_support::checkCorpusCondition(
              path,
              "compare embedded LLF",
              source->embedded_lighting.has_value() == roundtrip->embedded_lighting.has_value(),
              "presence changed"))
        continue;
      if (source->embedded_lighting) {
        if (!test_support::checkCorpusStatus(
                path, "validate written embedded LLF", pistoris::validate(*roundtrip->embedded_lighting)))
          continue;
        test_support::checkEquivalent(*source->embedded_lighting, *roundtrip->embedded_lighting);
      }
    }
  }
}
