// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include <cstdint>
#include <cstring>

TEST_SUITE("obj") {
  // --- Format error ---

  TEST_CASE("ImportBadFormat") {
    const char* obj = "v foo bar baz\n";
    ArxFtlHandle h  = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj), std::strlen(obj), nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_OBJ_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  // --- Null guard tests ---

  TEST_CASE("ImportNullObjData") {
    ArxFtlHandle h   = nullptr;
    ArxReturnCode rc = arx_pistoris_obj_parse(nullptr, 0, nullptr, 0, nullptr, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

  TEST_CASE("ImportNullOut") {
    const char* obj = "# empty\n";
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj), std::strlen(obj), nullptr, 0, nullptr, nullptr);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("ImportNullMtlDataWithNonZeroSize") {
    const char* obj = "# empty\n";
    ArxFtlHandle h  = nullptr;
    ArxReturnCode rc =
        arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj), std::strlen(obj), nullptr, 1, nullptr, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

}  // TEST_SUITE("obj")
