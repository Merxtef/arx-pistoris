// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/ftl.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

TEST_SUITE("ftl") {
  TEST_CASE("ModelFtlParse") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kFtl)) {
      CAPTURE(path.string());
      std::vector<std::uint8_t> bytes;
      if (!test_support::readCorpusBytes(path, bytes)) continue;
      ArxFtl* h = nullptr;
      const ArxReturnCode rc = arx_pistoris_ftl_read(bytes.data(), bytes.size(), &h);
      if (!test_support::checkCorpusStatus(path, "read FTL", rc)) {
        if (h) arx_pistoris_ftl_destroy(h);
        continue;
      }
      test_support::checkCorpusCondition(path, "read FTL", h != nullptr, "returned no FTL");
      if (h) arx_pistoris_ftl_destroy(h);
    }
  }

  TEST_CASE("ModelFtlWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kFtl)) {
      CAPTURE(path.string());
      std::vector<std::uint8_t> bytes;
      if (!test_support::readCorpusBytes(path, bytes)) continue;
      ArxFtl* h1 = nullptr;
      ArxReturnCode rc = arx_pistoris_ftl_read(bytes.data(), bytes.size(), &h1);
      if (!test_support::checkCorpusStatus(path, "read source FTL", rc)) {
        if (h1) arx_pistoris_ftl_destroy(h1);
        continue;
      }
      if (!test_support::checkCorpusCondition(path, "read source FTL", h1 != nullptr, "returned no FTL")) continue;

      uint8_t* out = nullptr;
      size_t sz = 0;
      rc = arx_pistoris_ftl_write(h1, 1, &out, &sz);
      if (!test_support::checkCorpusStatus(path, "write FTL", rc) ||
          !test_support::checkCorpusCondition(path, "write FTL", out != nullptr, "returned no bytes")) {
        if (out) arx_pistoris_free_bytes(out);
        arx_pistoris_ftl_destroy(h1);
        continue;
      }

      ArxFtl* h2 = nullptr;
      const ArxReturnCode rc2 = arx_pistoris_ftl_read(out, sz, &h2);
      arx_pistoris_free_bytes(out);
      if (!test_support::checkCorpusStatus(path, "read written FTL", rc2)) {
        if (h2) arx_pistoris_ftl_destroy(h2);
        arx_pistoris_ftl_destroy(h1);
        continue;
      }
      test_support::checkCorpusCondition(path, "read written FTL", h2 != nullptr, "returned no FTL");
      if (h2) {
        test_support::checkEquivalent(*reinterpret_cast<const pistoris::ftl::Data*>(h1),
                                      *reinterpret_cast<const pistoris::ftl::Data*>(h2));
        arx_pistoris_ftl_destroy(h2);
      }
      arx_pistoris_ftl_destroy(h1);
    }
  }

}  // TEST_SUITE("ftl")
