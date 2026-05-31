// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <vector>

TEST_SUITE("obj") {
  // --- Null guard tests ---

  TEST_CASE("ObjNullHandle") {
    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_obj(nullptr, "test", &out);
    CHECK(rc == ARX_INVALID_HANDLE);
    CHECK(out == nullptr);
  }

  TEST_CASE("ObjNullStem") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_obj(h, nullptr, &out);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("ObjNullOut") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    ArxReturnCode rc = arx_pistoris_ftl_to_obj(h, "test", nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("MtlNullHandle") {
    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_mtl(nullptr, &out);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("MtlNullOut") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    ArxReturnCode rc = arx_pistoris_ftl_to_mtl(h, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  // --- Smoke tests ---

  TEST_CASE("ObjSmoke") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    CHECK(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_obj(h, "test", &out);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);

    arx_pistoris_free_string(out);
    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("MtlSmoke") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h           = nullptr;
    CHECK(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

    char* out        = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_mtl(h, &out);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);

    arx_pistoris_free_string(out);
    arx_pistoris_ftl_free(h);
  }

}  // TEST_SUITE("obj")
