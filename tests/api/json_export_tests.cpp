// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/text.h"

#include "helpers.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::string makeMinimalFtsJson() {
  std::string result =
      R"({"$schema":"https://arx-tools.github.io/schemas/fts.schema.json","header":{"levelIdx":7,"mScenePosition":{"x":0,"y":0,"z":0}},"uniqueHeaders":[],"textureContainers":[],"cells":[)";
  for (std::size_t index = 0; index < 160U * 160U; ++index) {
    if (index != 0) result += ',';
    result += "{}";
  }
  result +=
      R"(],"polygons":[],"anchors":[],"portals":[],"rooms":[{"portals":[],"polygons":[]}],"roomDistances":[{"distance":-1,"startPosition":{"x":0,"y":0,"z":0},"endPosition":{"x":0,"y":0,"z":0}}]})";
  return result;
}

}  // namespace

TEST_SUITE("json") {
  TEST_CASE("JsonToJsonNullHandle") {
    char* out = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &out);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("JsonToJsonNullOut") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtl* h = nullptr;
    arx_pistoris_ftl_read(buf.data(), buf.size(), &h);

    ArxReturnCode rc = arx_pistoris_ftl_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_destroy(h);
  }

  TEST_CASE("JsonFromJsonNullData") {
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("JsonFromJsonNullOut") {
    const uint8_t data[] = "{}";
    ArxReturnCode rc = arx_pistoris_ftl_from_json(data, sizeof(data) - 1, ARX_NATIVE_TEXT_UTF8, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("JsonRoundtrip") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtl* h = nullptr;
    CHECK(arx_pistoris_ftl_read(buf.data(), buf.size(), &h) == ARX_OK);

    char* json_str = nullptr;
    ArxReturnCode rc1 = arx_pistoris_ftl_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, &json_str);
    CHECK(rc1 == ARX_OK);
    CHECK(json_str != nullptr);

    ArxFtl* h2 = nullptr;
    ArxReturnCode rc2 = arx_pistoris_ftl_from_json(
        reinterpret_cast<const uint8_t*>(json_str), std::strlen(json_str), ARX_NATIVE_TEXT_UTF8, &h2);
    CHECK(rc2 == ARX_OK);
    CHECK(h2 != nullptr);

    arx_pistoris_free_string(json_str);
    arx_pistoris_ftl_destroy(h);
    arx_pistoris_ftl_destroy(h2);
  }

  TEST_CASE("JsonBadFormat") {
    const char* bad = "not json";
    ArxFtl* h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_ftl_from_json(reinterpret_cast<const uint8_t*>(bad), std::strlen(bad), ARX_NATIVE_TEXT_UTF8, &h);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  TEST_CASE("JsonEmptyBadFormat") {
    const uint8_t empty = 0;
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(&empty, 0, ARX_NATIVE_TEXT_UTF8, &h);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  TEST_CASE("NativeFtsJsonRoundtrip") {
    const std::string source = makeMinimalFtsJson();
    ArxFts* fts = nullptr;
    REQUIRE(arx_pistoris_fts_from_json(
                reinterpret_cast<const uint8_t*>(source.data()), source.size(), ARX_NATIVE_TEXT_UTF8, &fts) == ARX_OK);
    CHECK(arx_pistoris_fts_validate(fts) == ARX_OK);

    char* exported = nullptr;
    REQUIRE(arx_pistoris_fts_to_json(fts, 1, ARX_NATIVE_TEXT_UTF8, &exported) == ARX_OK);
    CHECK(std::strstr(exported, "\"levelIdx\": 7") != nullptr);

    ArxFts* roundtrip = nullptr;
    REQUIRE(arx_pistoris_fts_from_json(
                reinterpret_cast<const uint8_t*>(exported), std::strlen(exported), ARX_NATIVE_TEXT_UTF8, &roundtrip) ==
            ARX_OK);
    CHECK(arx_pistoris_fts_validate(roundtrip) == ARX_OK);

    arx_pistoris_fts_destroy(roundtrip);
    arx_pistoris_free_string(exported);
    arx_pistoris_fts_destroy(fts);
  }

  TEST_CASE("TeaJsonToJsonNullHandle") {
    char* out = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_to_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &out);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("TeaJsonToJsonNullOut") {
    std::vector<uint8_t> buf = makeKeyframeTea();
    ArxTea* h = nullptr;
    arx_pistoris_tea_read(buf.data(), buf.size(), &h);

    ArxReturnCode rc = arx_pistoris_tea_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_destroy(h);
  }

  TEST_CASE("TeaJsonFromJsonNullData") {
    ArxTea* h = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_from_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("TeaJsonFromJsonNullOut") {
    const uint8_t data[] = "{}";
    ArxReturnCode rc = arx_pistoris_tea_from_json(data, sizeof(data) - 1, ARX_NATIVE_TEXT_UTF8, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("TeaJsonRoundtrip") {
    std::vector<uint8_t> buf = makeKeyframeTea();
    ArxTea* h = nullptr;
    CHECK(arx_pistoris_tea_read(buf.data(), buf.size(), &h) == ARX_OK);

    char* json_str = nullptr;
    ArxReturnCode rc1 = arx_pistoris_tea_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, &json_str);
    CHECK(rc1 == ARX_OK);
    CHECK(json_str != nullptr);

    ArxTea* h2 = nullptr;
    ArxReturnCode rc2 = arx_pistoris_tea_from_json(
        reinterpret_cast<const uint8_t*>(json_str), std::strlen(json_str), ARX_NATIVE_TEXT_UTF8, &h2);
    CHECK(rc2 == ARX_OK);
    CHECK(h2 != nullptr);

    arx_pistoris_free_string(json_str);
    arx_pistoris_tea_destroy(h);
    arx_pistoris_tea_destroy(h2);
  }

  TEST_CASE("TeaJsonBadFormat") {
    const char* bad = "not json";
    ArxTea* h = nullptr;
    ArxReturnCode rc =
        arx_pistoris_tea_from_json(reinterpret_cast<const uint8_t*>(bad), std::strlen(bad), ARX_NATIVE_TEXT_UTF8, &h);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }
}
