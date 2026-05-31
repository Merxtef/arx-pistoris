// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <cstring>
#include <vector>

TEST_SUITE("json") {
  // --- arx_pistoris_ftl_to_json null guards ---

  TEST_CASE("JsonToJsonNullHandle") {
    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_json(nullptr, 0, &out);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("JsonToJsonNullOut") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    ArxReturnCode rc = arx_pistoris_ftl_to_json(h, 0, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  // --- arx_pistoris_ftl_from_json null guards ---

  TEST_CASE("JsonFromJsonNullData") {
    ArxFtlHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(nullptr, 0, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("JsonFromJsonNullOut") {
    const uint8_t data[] = "{}";
    ArxReturnCode rc     = arx_pistoris_ftl_from_json(data, sizeof(data) - 1, nullptr);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  // --- happy path ---

  TEST_CASE("JsonRoundtrip") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    CHECK(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

    char* json_str    = nullptr;
    ArxReturnCode rc1 = arx_pistoris_ftl_to_json(h, 0, &json_str);
    CHECK(rc1 == ARX_OK);
    CHECK(json_str != nullptr);

    ArxFtlHandle h2 = nullptr;
    ArxReturnCode rc2 =
        arx_pistoris_ftl_from_json(reinterpret_cast<const uint8_t*>(json_str), std::strlen(json_str), &h2);
    CHECK(rc2 == ARX_OK);
    CHECK(h2 != nullptr);

    arx_pistoris_free_string(json_str);
    arx_pistoris_ftl_free(h);
    arx_pistoris_ftl_free(h2);
  }

  TEST_CASE("JsonBadFormat") {
    const char* bad  = "not json";
    ArxFtlHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(reinterpret_cast<const uint8_t*>(bad), std::strlen(bad), &h);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  TEST_CASE("JsonEmptyBadFormat") {
    const uint8_t empty = 0;
    ArxFtlHandle h      = nullptr;
    ArxReturnCode rc    = arx_pistoris_ftl_from_json(&empty, 0, &h);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  TEST_CASE("TeaJsonToJsonNullHandle") {
    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_to_json(nullptr, 0, &out);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("TeaJsonToJsonNullOut") {
    std::vector<uint8_t> buf = makeKeyframeTea();
    ArxTeaHandle h           = nullptr;
    arx_pistoris_tea_parse(buf.data(), buf.size(), &h);

    ArxReturnCode rc = arx_pistoris_tea_to_json(h, 0, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_free(h);
  }

  TEST_CASE("TeaJsonFromJsonNullData") {
    ArxTeaHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_from_json(nullptr, 0, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("TeaJsonFromJsonNullOut") {
    const uint8_t data[] = "{}";
    ArxReturnCode rc     = arx_pistoris_tea_from_json(data, sizeof(data) - 1, nullptr);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("TeaJsonRoundtrip") {
    std::vector<uint8_t> buf = makeKeyframeTea();
    ArxTeaHandle h           = nullptr;
    CHECK(arx_pistoris_tea_parse(buf.data(), buf.size(), &h) == ARX_OK);

    char* json_str    = nullptr;
    ArxReturnCode rc1 = arx_pistoris_tea_to_json(h, 0, &json_str);
    CHECK(rc1 == ARX_OK);
    CHECK(json_str != nullptr);

    ArxTeaHandle h2 = nullptr;
    ArxReturnCode rc2 =
        arx_pistoris_tea_from_json(reinterpret_cast<const uint8_t*>(json_str), std::strlen(json_str), &h2);
    CHECK(rc2 == ARX_OK);
    CHECK(h2 != nullptr);

    arx_pistoris_free_string(json_str);
    arx_pistoris_tea_free(h);
    arx_pistoris_tea_free(h2);
  }

  TEST_CASE("TeaJsonBadFormat") {
    const char* bad  = "not json";
    ArxTeaHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_from_json(reinterpret_cast<const uint8_t*>(bad), std::strlen(bad), &h);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }
}
