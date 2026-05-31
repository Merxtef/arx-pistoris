// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/ftl_data.hpp"

#include "helpers.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

static std::vector<uint8_t> readBytes(const fs::path& p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}

TEST_SUITE("ftl") {
  // --- data/fixtures/model/native/ (committed CC0 seeds, always run) ---

  TEST_CASE("ModelFtlParse") {
    std::size_t count = 0;
    for (auto& e : fs::directory_iterator("data/fixtures/model/native")) {
      if (e.path().extension() != ".ftl") continue;
      count++;
      CAPTURE(e.path().string());
      auto bytes       = readBytes(e.path());
      ArxFtlHandle h   = nullptr;
      ArxReturnCode rc = arx_pistoris_ftl_parse(bytes.data(), bytes.size(), &h);
      CHECK(rc == ARX_OK);
      if (h) arx_pistoris_ftl_free(h);
    }
    CHECK(count >= 1);
  }

  TEST_CASE("ModelFtlWriteRoundtrip") {
    std::size_t count = 0;
    for (auto& e : fs::directory_iterator("data/fixtures/model/native")) {
      if (e.path().extension() != ".ftl") continue;
      count++;
      CAPTURE(e.path().string());
      auto bytes      = readBytes(e.path());
      ArxFtlHandle h1 = nullptr;
      CHECK(arx_pistoris_ftl_parse(bytes.data(), bytes.size(), &h1) == ARX_OK);
      if (!h1) continue;

      uint8_t* out = nullptr;
      size_t sz    = 0;
      CHECK(arx_pistoris_ftl_write(h1, &out, &sz) == ARX_OK);
      if (!out) {
        arx_pistoris_ftl_free(h1);
        continue;
      }

      ArxFtlHandle h2   = nullptr;
      ArxReturnCode rc2 = arx_pistoris_ftl_parse(out, sz, &h2);
      arx_pistoris_free_bytes(out);
      CHECK(rc2 == ARX_OK);
      if (h2) {
        checkEq(*reinterpret_cast<const pistoris::ftl::Data*>(h1), *reinterpret_cast<const pistoris::ftl::Data*>(h2));
        arx_pistoris_ftl_free(h2);
      }
      arx_pistoris_ftl_free(h1);
    }
    CHECK(count >= 1);
  }

  // --- data/arx/ftl/ (game assets, silently skipped if absent) ---

  TEST_CASE("ArxFtlParse") {
    const fs::path dir = "data/arx/ftl";
    if (!fs::exists(dir)) return;
    for (auto& e : fs::directory_iterator(dir)) {
      if (e.path().extension() != ".ftl") continue;
      CAPTURE(e.path().string());
      auto bytes       = readBytes(e.path());
      ArxFtlHandle h   = nullptr;
      ArxReturnCode rc = arx_pistoris_ftl_parse(bytes.data(), bytes.size(), &h);
      CHECK(rc == ARX_OK);
      if (h) arx_pistoris_ftl_free(h);
    }
  }

  TEST_CASE("ArxFtlWriteRoundtrip") {
    const fs::path dir = "data/arx/ftl";
    if (!fs::exists(dir)) return;
    for (auto& e : fs::directory_iterator(dir)) {
      if (e.path().extension() != ".ftl") continue;
      CAPTURE(e.path().string());
      auto bytes      = readBytes(e.path());
      ArxFtlHandle h1 = nullptr;
      CHECK(arx_pistoris_ftl_parse(bytes.data(), bytes.size(), &h1) == ARX_OK);
      if (!h1) continue;

      uint8_t* out = nullptr;
      size_t sz    = 0;
      CHECK(arx_pistoris_ftl_write(h1, &out, &sz) == ARX_OK);
      if (!out) {
        arx_pistoris_ftl_free(h1);
        continue;
      }

      ArxFtlHandle h2   = nullptr;
      ArxReturnCode rc2 = arx_pistoris_ftl_parse(out, sz, &h2);
      arx_pistoris_free_bytes(out);
      CHECK(rc2 == ARX_OK);
      if (h2) {
        checkEq(*reinterpret_cast<const pistoris::ftl::Data*>(h1), *reinterpret_cast<const pistoris::ftl::Data*>(h2));
        arx_pistoris_ftl_free(h2);
      }
      arx_pistoris_ftl_free(h1);
    }
  }

}  // TEST_SUITE("ftl")
