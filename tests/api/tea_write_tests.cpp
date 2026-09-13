// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstddef>
#include <cstdint>
#include <vector>

TEST_SUITE("tea") {
  TEST_CASE("WriteNullHandle") {
    uint8_t* out = nullptr;
    size_t sz = 0;
    ArxReturnCode rc = arx_pistoris_tea_write(nullptr, &out, &sz);
    CHECK(rc == ARX_INVALID_HANDLE);
    CHECK(out == nullptr);
    CHECK(sz == 0);
  }

  TEST_CASE("WriteNullOut") {
    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTea* h = nullptr;
    arx_pistoris_tea_read(buf.data(), buf.size(), &h);

    size_t sz = 0;
    ArxReturnCode rc = arx_pistoris_tea_write(h, nullptr, &sz);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_destroy(h);
  }

  TEST_CASE("WriteNullSize") {
    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTea* h = nullptr;
    arx_pistoris_tea_read(buf.data(), buf.size(), &h);

    uint8_t* out = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_write(h, &out, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_destroy(h);
  }

  TEST_CASE("WriteSmoke") {
    std::vector<uint8_t> fixture = makeMinimalTea();
    setNumKeyframes(fixture, 1);
    appendKeyframe2014(fixture);
    ArxTea* h = nullptr;
    REQUIRE(arx_pistoris_tea_read(fixture.data(), fixture.size(), &h) == ARX_OK);

    uint8_t* out = nullptr;
    size_t sz = 0;
    ArxReturnCode rc = arx_pistoris_tea_write(h, &out, &sz);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);
    CHECK(sz > 0);

    arx_pistoris_free_bytes(out);
    arx_pistoris_tea_destroy(h);
  }

}  // TEST_SUITE("tea")
