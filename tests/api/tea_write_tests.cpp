// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstddef>
#include <cstdint>
#include <vector>

TEST_SUITE("tea") {
  // --- Null guard tests ---

  TEST_CASE("WriteNullHandle") {
    uint8_t* out     = nullptr;
    size_t sz        = 0;
    ArxReturnCode rc = arx_pistoris_tea_write(nullptr, &out, &sz);
    CHECK(rc == ARX_INVALID_HANDLE);
    CHECK(out == nullptr);
    CHECK(sz == 0);
  }

  TEST_CASE("WriteNullOut") {
    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTeaHandle h = nullptr;
    arx_pistoris_tea_parse(buf.data(), buf.size(), &h);

    size_t sz        = 0;
    ArxReturnCode rc = arx_pistoris_tea_write(h, nullptr, &sz);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_free(h);
  }

  TEST_CASE("WriteNullSize") {
    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTeaHandle h = nullptr;
    arx_pistoris_tea_parse(buf.data(), buf.size(), &h);

    uint8_t* out     = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_write(h, &out, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_free(h);
  }

  // --- Smoke test ---

  TEST_CASE("WriteSmoke") {
    std::vector<uint8_t> fixture = makeMinimalTea();
    setNumKeyframes(fixture, 1);
    appendKeyframe2014(fixture);
    ArxTeaHandle h = nullptr;
    REQUIRE(arx_pistoris_tea_parse(fixture.data(), fixture.size(), &h) == ARX_OK);

    uint8_t* out     = nullptr;
    size_t sz        = 0;
    ArxReturnCode rc = arx_pistoris_tea_write(h, &out, &sz);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);
    CHECK(sz > 0);

    arx_pistoris_free_bytes(out);
    arx_pistoris_tea_free(h);
  }

  TEST_CASE("ApplyXformIdentity") {
    std::vector<uint8_t> fixture = makeKeyframeTea();
    ArxTeaHandle h               = nullptr;
    REQUIRE(arx_pistoris_tea_parse(fixture.data(), fixture.size(), &h) == ARX_OK);

    CHECK(arx_pistoris_tea_apply_xform(h, 0, 0, 0, 1, 1, 1, 0, 0, 0) == ARX_OK);
    CHECK(arx_pistoris_tea_apply_xform(nullptr, 0, 0, 0, 1, 1, 1, 0, 0, 0) == ARX_INVALID_HANDLE);

    arx_pistoris_tea_free(h);
  }

}  // TEST_SUITE("tea")
