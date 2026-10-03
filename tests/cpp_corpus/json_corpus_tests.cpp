// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

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

template <class Import, class Export>
void checkJsonRoundtrip(std::string_view source, Import&& import_json, Export&& export_json) {
  auto native = import_json(source);
  REQUIRE(native);
  REQUIRE(pistoris::validate(*native));
  auto encoded = export_json(*native);
  REQUIRE(encoded);
  auto roundtrip = import_json(*encoded);
  REQUIRE(roundtrip);
  REQUIRE(pistoris::validate(*roundtrip));
  test_support::checkEquivalent(*native, *roundtrip);
}

void checkFtsJsonRoundtrip(std::string_view source) {
  auto imported = pistoris::fromFtsJson(source);
  REQUIRE(imported);
  REQUIRE(pistoris::validate(imported->fts));
  auto encoded = pistoris::toFtsJson(imported->fts, imported->level);
  REQUIRE(encoded);
  auto roundtrip = pistoris::fromFtsJson(*encoded);
  REQUIRE(roundtrip);
  REQUIRE(pistoris::validate(roundtrip->fts));
  CHECK(roundtrip->level == imported->level);
  test_support::checkEquivalent(imported->fts, roundtrip->fts);
}

}  // namespace

TEST_SUITE("json_corpus") {
  TEST_CASE("JSON fixtures roundtrip through native carriers") {
    for (const test_support::JsonFixture& fixture : test_support::fixtureCatalog().json) {
      CAPTURE(fixture.path.string());
      CAPTURE(fixture.format);
      const std::string source = readText(fixture.path);
      if (fixture.format == "amb")
        checkJsonRoundtrip(
            source,
            [](std::string_view json) { return pistoris::fromAmbJson(json); },
            [](const pistoris::Amb& value) { return pistoris::toAmbJson(value); });
      else if (fixture.format == "dlf")
        checkJsonRoundtrip(
            source,
            [](std::string_view json) { return pistoris::fromDlfJson(json); },
            [](const pistoris::Dlf& value) { return pistoris::toDlfJson(value); });
      else if (fixture.format == "ftl")
        checkJsonRoundtrip(
            source,
            [](std::string_view json) { return pistoris::fromFtlJson(json); },
            [](const pistoris::Ftl& value) { return pistoris::toFtlJson(value); });
      else if (fixture.format == "fts")
        checkFtsJsonRoundtrip(source);
      else if (fixture.format == "llf")
        checkJsonRoundtrip(
            source,
            [](std::string_view json) { return pistoris::fromLlfJson(json); },
            [](const pistoris::Llf& value) { return pistoris::toLlfJson(value); });
      else if (fixture.format == "tea")
        checkJsonRoundtrip(
            source,
            [](std::string_view json) { return pistoris::fromTeaJson(json); },
            [](const pistoris::Tea& value) { return pistoris::toTeaJson(value); });
      else
        FAIL_CHECK("Unknown native JSON fixture format");
    }
  }
}
