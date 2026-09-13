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
      pistoris::Dlf dlf;
      std::optional<pistoris::Llf> lighting;
      if (!test_support::checkCorpusStatus(path, "read DLF", pistoris::readDlf(bytes, dlf, &lighting))) continue;
      if (!test_support::checkCorpusStatus(path, "validate DLF", pistoris::validate(dlf))) continue;
      if (lighting) test_support::checkCorpusStatus(path, "validate embedded LLF", pistoris::validate(*lighting));
    }
  }

  TEST_CASE("ArxDlfWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kDlf)) {
      CAPTURE(path.string());

      std::vector<std::uint8_t> source_bytes;
      if (!test_support::readCorpusBytes(path, source_bytes)) continue;
      pistoris::Dlf source;
      std::optional<pistoris::Llf> source_lighting;
      if (!test_support::checkCorpusStatus(
              path, "read source DLF", pistoris::readDlf(source_bytes, source, &source_lighting)))
        continue;

      std::vector<std::uint8_t> written;
      pistoris::DlfWriteOptions options{source_lighting ? &*source_lighting : nullptr, {}};
      if (!test_support::checkCorpusStatus(path, "write DLF", pistoris::writeDlf(source, options, written))) continue;

      pistoris::Dlf roundtrip;
      std::optional<pistoris::Llf> roundtrip_lighting;
      if (!test_support::checkCorpusStatus(
              path, "read written DLF", pistoris::readDlf(written, roundtrip, &roundtrip_lighting)))
        continue;
      if (!test_support::checkCorpusStatus(path, "validate written DLF", pistoris::validate(roundtrip))) continue;
      test_support::checkEquivalent(source, roundtrip);
      if (!test_support::checkCorpusCondition(path,
                                              "compare embedded LLF",
                                              source_lighting.has_value() == roundtrip_lighting.has_value(),
                                              "presence changed"))
        continue;
      if (source_lighting) {
        if (!test_support::checkCorpusStatus(
                path, "validate written embedded LLF", pistoris::validate(*roundtrip_lighting)))
          continue;
        test_support::checkEquivalent(*source_lighting, *roundtrip_lighting);
      }
    }
  }
}
