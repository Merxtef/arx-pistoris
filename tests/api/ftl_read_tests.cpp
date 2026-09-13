// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <vector>

TEST_SUITE("ftl") {
  TEST_CASE("FtlNullData") {
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_read(nullptr, 0, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

  TEST_CASE("FtlNullOut") {
    uint8_t dummy = 0;
    ArxReturnCode rc = arx_pistoris_ftl_read(&dummy, sizeof(dummy), nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("FtlReadSmoke") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_read(buf.data(), buf.size(), &h);
    CHECK(rc == ARX_OK);
    CHECK(h != nullptr);
    arx_pistoris_ftl_destroy(h);
  }

}  // TEST_SUITE("ftl")
