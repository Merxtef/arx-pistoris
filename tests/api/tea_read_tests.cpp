// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <vector>

TEST_SUITE("tea") {
  // --- Null guard tests ---

  TEST_CASE("TeaNameNullAndValid") {
    CHECK(arx_pistoris_tea_name(nullptr) == nullptr);

    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTeaHandle h = nullptr;
    REQUIRE(arx_pistoris_tea_parse(buf.data(), buf.size(), &h) == ARX_OK);

    const char* name = arx_pistoris_tea_name(h);
    CHECK(name != nullptr);
    CHECK(name[0] == '\0');  // makeMinimalTea leaves name zero-initialized

    arx_pistoris_tea_free(h);
  }

  TEST_CASE("TeaApiNullData") {
    ArxTeaHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_parse(nullptr, 0, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

  TEST_CASE("TeaApiNullOut") {
    uint8_t dummy    = 0;
    ArxReturnCode rc = arx_pistoris_tea_parse(&dummy, sizeof(dummy), nullptr);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  // --- Smoke test ---

  TEST_CASE("TeaApiReadSmoke") {
    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTeaHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_parse(buf.data(), buf.size(), &h);
    CHECK(rc == ARX_OK);
    CHECK(h != nullptr);
    arx_pistoris_tea_free(h);
  }

}  // TEST_SUITE("tea")
