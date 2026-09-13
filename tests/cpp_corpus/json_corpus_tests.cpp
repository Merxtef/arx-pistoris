// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native.hpp"

#include "support/fixture_catalog.h"
#include "support/native_equivalence.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

std::string readText(const std::filesystem::path& path) {
  std::ifstream file(path);
  return {std::istreambuf_iterator<char>(file), {}};
}

template <class Native>
void checkJsonRoundtrip(std::string_view source) {
  Native native;
  REQUIRE(pistoris::fromJson(source, native) == ARX_OK);
  REQUIRE(pistoris::validate(native) == ARX_OK);
  std::string encoded;
  REQUIRE(pistoris::toJson(native, encoded) == ARX_OK);
  Native roundtrip;
  REQUIRE(pistoris::fromJson(encoded, roundtrip) == ARX_OK);
  REQUIRE(pistoris::validate(roundtrip) == ARX_OK);
  test_support::checkEquivalent(native, roundtrip);
}

}  // namespace

TEST_SUITE("json_corpus") {
  TEST_CASE("JSON fixtures roundtrip through native carriers") {
    for (const test_support::JsonFixture& fixture : test_support::fixtureCatalog().json) {
      CAPTURE(fixture.path.string());
      CAPTURE(fixture.format);
      const std::string source = readText(fixture.path);
      if (fixture.format == "amb")
        checkJsonRoundtrip<pistoris::Amb>(source);
      else if (fixture.format == "dlf")
        checkJsonRoundtrip<pistoris::Dlf>(source);
      else if (fixture.format == "ftl")
        checkJsonRoundtrip<pistoris::Ftl>(source);
      else if (fixture.format == "fts")
        checkJsonRoundtrip<pistoris::Fts>(source);
      else if (fixture.format == "llf")
        checkJsonRoundtrip<pistoris::Llf>(source);
      else if (fixture.format == "tea")
        checkJsonRoundtrip<pistoris::Tea>(source);
      else
        FAIL_CHECK("Unknown native JSON fixture format");
    }
  }
}
