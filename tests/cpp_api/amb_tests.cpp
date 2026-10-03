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
    auto amb = pistoris::readAmb(bytes);
    REQUIRE(amb);
    REQUIRE(amb->tracks.size() == 1);
    REQUIRE(amb->tracks.front().keys.size() == 2);
    CHECK(amb->tracks.front().keys.front().start_ms == 100);
    CHECK(pistoris::validate(*amb));

    auto written = pistoris::writeAmb(*amb);
    REQUIRE(written);
    CHECK(!written->empty());
  }

  TEST_CASE("AmbReadAndWriteAreTransactional") {
    pistoris::Amb amb = makeAmbData();
    std::vector<std::uint8_t> truncated = makeAmbBytes();
    truncated.pop_back();
    auto read = pistoris::readAmb(truncated);
    CHECK(read.code() == ARX_UNEXPECTED_EOF);

    amb.tracks.front().sample_path.clear();
    CHECK(pistoris::writeAmb(amb).code() == ARX_AMB_BAD_SAMPLE_PATH);
  }

  TEST_CASE("AmbJsonRoundtrip") {
    pistoris::Amb source = makeAmbData();
    auto json = pistoris::toAmbJson(source, true);
    REQUIRE(json);
    CHECK(json->find("https://arx-tools.github.io/schemas/amb.schema.json") != std::string::npos);

    auto result = pistoris::fromAmbJson(*json);
    REQUIRE(result);
    REQUIRE(result->tracks.size() == 1);
    CHECK(result->tracks.front().sample_path == source.tracks.front().sample_path);
    CHECK(result->tracks.front().keys.size() == source.tracks.front().keys.size());
  }
}
