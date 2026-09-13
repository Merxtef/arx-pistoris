// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <vector>

TEST_SUITE("tea") {
  TEST_CASE("TeaApiNullData") {
    ArxTea* h = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_read(nullptr, 0, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

  TEST_CASE("TeaApiNullOut") {
    uint8_t dummy = 0;
    ArxReturnCode rc = arx_pistoris_tea_read(&dummy, sizeof(dummy), nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("TeaApiReadSmoke") {
    std::vector<uint8_t> buf = makeMinimalTea();
    setNumKeyframes(buf, 1);
    appendKeyframe2014(buf);
    ArxTea* h = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_read(buf.data(), buf.size(), &h);
    CHECK(rc == ARX_OK);
    CHECK(h != nullptr);
    arx_pistoris_tea_destroy(h);
  }

}  // TEST_SUITE("tea")
