// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/pistoris.hpp"

#include "amb_helpers.h"

#include <cstdint>
#include <string>
#include <vector>

TEST_SUITE("cpp_api") {
  TEST_CASE("AmbReadWriteRoundtrip") {
    const std::vector<std::uint8_t> bytes = makeAmbBytes(pistoris::kAmbVersion1003);
    pistoris::Amb amb;
    REQUIRE(pistoris::readAmb(bytes, amb) == ARX_OK);
    REQUIRE(amb.tracks.size() == 1);
    REQUIRE(amb.tracks.front().keys.size() == 2);
    CHECK(amb.tracks.front().keys.front().start_ms == 100);
    CHECK(pistoris::validate(amb) == ARX_OK);

    std::vector<std::uint8_t> written;
    REQUIRE(pistoris::writeAmb(amb, written) == ARX_OK);
    CHECK(!written.empty());
  }

  TEST_CASE("AmbReadAndWriteAreTransactional") {
    pistoris::Amb amb = makeAmbData();
    amb.tracks.front().sample_path = "unchanged";
    std::vector<std::uint8_t> truncated = makeAmbBytes();
    truncated.pop_back();
    CHECK(pistoris::readAmb(truncated, amb) == ARX_UNEXPECTED_EOF);
    CHECK(amb.tracks.front().sample_path == "unchanged");

    std::vector<std::uint8_t> output = {1, 2, 3};
    amb.tracks.front().sample_path.clear();
    CHECK(pistoris::writeAmb(amb, output) == ARX_AMB_BAD_SAMPLE_PATH);
    CHECK(output == std::vector<std::uint8_t>{1, 2, 3});
  }

  TEST_CASE("AmbJsonRoundtrip") {
    pistoris::Amb source = makeAmbData();
    std::string json;
    REQUIRE(pistoris::toJson(source, json, true) == ARX_OK);
    CHECK(json.find("https://arx-tools.github.io/schemas/amb.schema.json") != std::string::npos);

    pistoris::Amb result;
    REQUIRE(pistoris::fromJson(json, result) == ARX_OK);
    REQUIRE(result.tracks.size() == 1);
    CHECK(result.tracks.front().sample_path == source.tracks.front().sample_path);
    CHECK(result.tracks.front().keys.size() == source.tracks.front().keys.size());
  }
}
