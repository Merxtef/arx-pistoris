// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"

#include "amb_helpers.h"
#include "external/json.h"
#include "nlohmann/json.hpp"
#include "support/native_equivalence.h"

#include <cstdint>
#include <string>

TEST_SUITE("amb_json") {
  TEST_CASE("ExportsArxConvertShapeAndLogicalOrder") {
    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().sample_path = "SFX\\AMBIANCE\\TEST.WAV";
    data.tracks.front().keys.front().loop_minus_one = UINT32_MAX;

    std::string text;
    REQUIRE(pistoris::exportAmbToJson(data, false, text) == ARX_OK);
    const nlohmann::json json = nlohmann::json::parse(text);
    CHECK(json["$schema"] == "https://arx-tools.github.io/schemas/amb.schema.json");
    CHECK(json["tracks"][0]["filename"] == "sfx/ambiance/test.wav");
    CHECK(json["tracks"][0]["keys"][0]["start"] == 100);
    CHECK(json["tracks"][0]["keys"][1]["start"] == 200);
    CHECK(json["tracks"][0]["keys"][0]["loop"] == 4294967296ULL);
  }

  TEST_CASE("RoundtripPreservesNativeFields") {
    pistoris::amb::Data source = makeAmbData();
    source.tracks.front().keys.front().loop_minus_one = UINT32_MAX;

    std::string text;
    REQUIRE(pistoris::exportAmbToJson(source, true, text) == ARX_OK);
    pistoris::amb::Data result;
    REQUIRE(pistoris::importJsonToAmb(text, &result) == ARX_OK);
    test_support::checkEquivalent(source, result);
  }

  TEST_CASE("ImportCanonicalizesInactiveAndConstantSettings") {
    nlohmann::json setting = {{"min", 0.0}, {"max", 0.0}, {"interval", 0}, {"flags", 0}};
    nlohmann::json key = {
        {"start", 0},
        {"loop", 1},
        {"delayMin", 0},
        {"delayMax", 0},
        {"volume", setting},
        {"pitch", {{"min", 2.0}, {"max", 2.0}, {"interval", 99}, {"flags", 3}}},
        {"pan", {{"min", -9.0}, {"max", 9.0}, {"interval", 17}, {"flags", 3}}},
        {"x", setting},
        {"y", setting},
        {"z", setting},
    };
    const nlohmann::json json = {
        {"$schema", "https://arx-tools.github.io/schemas/amb.schema.json"},
        {"tracks",
         {{{"filename", "SFX\\AMBIANCE\\TEST.WAV"},
           {"flags", pistoris::amb::kTrackPosition | pistoris::amb::kTrackMaster},
           {"keys", {key}}}}},
    };

    pistoris::amb::Data result;
    REQUIRE(pistoris::importJsonToAmb(json.dump(), &result) == ARX_OK);
    const pistoris::amb::Key& imported = result.tracks.front().keys.front();
    CHECK(result.tracks.front().sample_path == "sfx/ambiance/test.wav");
    CHECK(imported.pitch.min == 2.0f);
    CHECK(imported.pitch.max == 2.0f);
    CHECK(imported.pitch.interval_ms == 0);
    CHECK(imported.pitch.flags == 0);
    CHECK(imported.pan.min == 0.0f);
    CHECK(imported.pan.max == 0.0f);
    CHECK(imported.pan.interval_ms == 0);
    CHECK(imported.pan.flags == 0);
  }

  TEST_CASE("ImportValidatesSchemaTransactionally") {
    pistoris::amb::Data data = makeAmbData();
    data.tracks.front().sample_path = "unchanged";
    CHECK(pistoris::importJsonToAmb("{}", &data) == ARX_JSON_BAD_SCHEMA);
    CHECK(data.tracks.front().sample_path == "unchanged");

    const char* bad_loop =
        R"({"tracks":[{"filename":"a.wav","flags":4,"keys":[{"start":0,"loop":4294967297,"delayMin":0,"delayMax":0,"volume":{"min":0,"max":0,"interval":0,"flags":0},"pitch":{"min":0,"max":0,"interval":0,"flags":0},"pan":{"min":0,"max":0,"interval":0,"flags":0},"x":{"min":0,"max":0,"interval":0,"flags":0},"y":{"min":0,"max":0,"interval":0,"flags":0},"z":{"min":0,"max":0,"interval":0,"flags":0}}]}]})";
    CHECK(pistoris::importJsonToAmb(bad_loop, &data) == ARX_JSON_BAD_SCHEMA);
    CHECK(data.tracks.front().sample_path == "unchanged");
  }
}
