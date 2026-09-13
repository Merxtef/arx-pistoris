// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/tea.hpp"

#include "support/corpus_checks.h"
#include "support/corpus_files.h"
#include "support/native_equivalence.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

TEST_SUITE("tea") {
  TEST_CASE("ArxTeaParse") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kTea)) {
      CAPTURE(path.string());
      std::vector<std::uint8_t> bytes;
      if (!test_support::readCorpusBytes(path, bytes)) continue;
      ArxTea* h = nullptr;
      const ArxReturnCode rc = arx_pistoris_tea_read(bytes.data(), bytes.size(), &h);
      if (!test_support::checkCorpusStatus(path, "read TEA", rc)) {
        if (h) arx_pistoris_tea_destroy(h);
        continue;
      }
      test_support::checkCorpusCondition(path, "read TEA", h != nullptr, "returned no TEA");
      if (h) arx_pistoris_tea_destroy(h);
    }
  }

  TEST_CASE("ArxTeaWriteRoundtrip") {
    for (const fs::path& path : test_support::nativeCorpusFiles(test_support::NativeCorpusFormat::kTea)) {
      CAPTURE(path.string());
      std::vector<std::uint8_t> bytes;
      if (!test_support::readCorpusBytes(path, bytes)) continue;
      ArxTea* h1 = nullptr;
      ArxReturnCode rc = arx_pistoris_tea_read(bytes.data(), bytes.size(), &h1);
      if (!test_support::checkCorpusStatus(path, "read source TEA", rc)) {
        if (h1) arx_pistoris_tea_destroy(h1);
        continue;
      }
      if (!test_support::checkCorpusCondition(path, "read source TEA", h1 != nullptr, "returned no TEA")) continue;

      uint8_t* out = nullptr;
      size_t sz = 0;
      rc = arx_pistoris_tea_write(h1, &out, &sz);
      if (!test_support::checkCorpusStatus(path, "write TEA", rc) ||
          !test_support::checkCorpusCondition(path, "write TEA", out != nullptr, "returned no bytes")) {
        if (out) arx_pistoris_free_bytes(out);
        arx_pistoris_tea_destroy(h1);
        continue;
      }

      ArxTea* h2 = nullptr;
      const ArxReturnCode rc2 = arx_pistoris_tea_read(out, sz, &h2);
      arx_pistoris_free_bytes(out);
      if (!test_support::checkCorpusStatus(path, "read written TEA", rc2)) {
        if (h2) arx_pistoris_tea_destroy(h2);
        arx_pistoris_tea_destroy(h1);
        continue;
      }
      test_support::checkCorpusCondition(path, "read written TEA", h2 != nullptr, "returned no TEA");
      if (h2) {
        test_support::checkEquivalent(*reinterpret_cast<const pistoris::tea::Data*>(h1),
                                      *reinterpret_cast<const pistoris::tea::Data*>(h2));
        arx_pistoris_tea_destroy(h2);
      }
      arx_pistoris_tea_destroy(h1);
    }
  }

}  // TEST_SUITE("tea")
