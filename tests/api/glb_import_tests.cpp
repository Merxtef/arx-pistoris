// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <vector>

TEST_SUITE("glb_import") {
  TEST_CASE("FromGlbNullHandling") {
    SUBCASE("NullData") {
      ArxFtlHandle ftl   = nullptr;
      ArxTeaHandle* teas = nullptr;
      size_t tea_count   = 0;
      ArxReturnCode rc   = arx_pistoris_from_glb(nullptr, 0, nullptr, &ftl, &teas, &tea_count);
      CHECK(rc == ARX_INVALID_DATA_POINTER);
    }

    SUBCASE("NullOutFtl") {
      uint8_t dummy      = 0;
      ArxTeaHandle* teas = nullptr;
      size_t tea_count   = 0;
      ArxReturnCode rc   = arx_pistoris_from_glb(&dummy, sizeof(dummy), nullptr, nullptr, &teas, &tea_count);
      CHECK(rc == ARX_INVALID_HANDLE);
    }

    SUBCASE("NullOutTeas") {
      uint8_t dummy    = 0;
      ArxFtlHandle ftl = nullptr;
      size_t tea_count = 0;
      ArxReturnCode rc = arx_pistoris_from_glb(&dummy, sizeof(dummy), nullptr, &ftl, nullptr, &tea_count);
      CHECK(rc == ARX_INVALID_DATA_POINTER);
    }

    SUBCASE("NullOutTeaCount") {
      uint8_t dummy      = 0;
      ArxFtlHandle ftl   = nullptr;
      ArxTeaHandle* teas = nullptr;
      ArxReturnCode rc   = arx_pistoris_from_glb(&dummy, sizeof(dummy), nullptr, &ftl, &teas, nullptr);
      CHECK(rc == ARX_INVALID_DATA_POINTER);
    }
  }

  // to_glb -> from_glb roundtrip; mesh-only (no TEAs supplied)
  TEST_CASE("FromGlbRoundtripMeshOnly") {
    std::vector<uint8_t> ftl_buf = makeTriangleFtlWithTexture();
    ArxFtlHandle ftl_h           = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(ftl_buf.data(), ftl_buf.size(), &ftl_h) == ARX_OK);

    uint8_t* glb_data = nullptr;
    size_t glb_size   = 0;
    REQUIRE(arx_pistoris_to_glb(ftl_h, nullptr, 0, &glb_data, &glb_size) == ARX_OK);

    ArxFtlHandle imp_ftl = nullptr;
    ArxTeaHandle* teas   = nullptr;
    size_t tea_count     = 0;
    ArxReturnCode rc     = arx_pistoris_from_glb(glb_data, glb_size, "test.glb", &imp_ftl, &teas, &tea_count);
    CHECK(rc == ARX_OK);
    CHECK(imp_ftl != nullptr);
    CHECK(tea_count == 0);
    CHECK(teas == nullptr);

    arx_pistoris_free_tea_array(teas, tea_count);
    arx_pistoris_ftl_free(imp_ftl);
    arx_pistoris_free_bytes(glb_data);
    arx_pistoris_ftl_free(ftl_h);
  }

}  // TEST_SUITE("glb_import")
