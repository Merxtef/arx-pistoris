// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/tea_data.hpp"

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

TEST_SUITE("tea") {
  // --- data/arx/tea/ (game assets, silently skipped if absent) ---

  TEST_CASE("ArxTeaParse") {
    const fs::path dir = "data/arx/tea";
    if (!fs::exists(dir)) return;
    for (auto& e : fs::directory_iterator(dir)) {
      if (e.path().extension() != ".tea") continue;
      CAPTURE(e.path().string());
      auto bytes       = readBytes(e.path());
      ArxTeaHandle h   = nullptr;
      ArxReturnCode rc = arx_pistoris_tea_parse(bytes.data(), bytes.size(), &h);
      CHECK(rc == ARX_OK);
      if (h) arx_pistoris_tea_free(h);
    }
  }

  TEST_CASE("ArxTeaWriteRoundtrip") {
    const fs::path dir = "data/arx/tea";
    if (!fs::exists(dir)) return;
    for (auto& e : fs::directory_iterator(dir)) {
      if (e.path().extension() != ".tea") continue;
      CAPTURE(e.path().string());
      auto bytes      = readBytes(e.path());
      ArxTeaHandle h1 = nullptr;
      CHECK(arx_pistoris_tea_parse(bytes.data(), bytes.size(), &h1) == ARX_OK);
      if (!h1) continue;

      uint8_t* out = nullptr;
      size_t sz    = 0;
      CHECK(arx_pistoris_tea_write(h1, &out, &sz) == ARX_OK);
      if (!out) {
        arx_pistoris_tea_free(h1);
        continue;
      }

      ArxTeaHandle h2   = nullptr;
      ArxReturnCode rc2 = arx_pistoris_tea_parse(out, sz, &h2);
      arx_pistoris_free_bytes(out);
      CHECK(rc2 == ARX_OK);
      if (h2) {
        checkEq(*reinterpret_cast<const pistoris::tea::Data*>(h1), *reinterpret_cast<const pistoris::tea::Data*>(h2));
        arx_pistoris_tea_free(h2);
      }
      arx_pistoris_tea_free(h1);
    }
  }

}  // TEST_SUITE("tea")
