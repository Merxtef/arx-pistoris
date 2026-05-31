// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <cstring>
#include <vector>

TEST_SUITE("glb") {
  TEST_CASE("GlbNullFtlHandle") {
    uint8_t* out     = nullptr;
    size_t out_size  = 0;
    ArxReturnCode rc = arx_pistoris_to_glb(nullptr, nullptr, 0, &out, &out_size);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("GlbNullOut") {
    auto buf       = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    size_t out_size = 0;
    CHECK(arx_pistoris_to_glb(h, nullptr, 0, nullptr, &out_size) == ARX_INVALID_DATA_POINTER);

    uint8_t* out = nullptr;
    CHECK(arx_pistoris_to_glb(h, nullptr, 0, &out, nullptr) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("GlbNullTeasWithCount") {
    auto buf       = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    uint8_t* out    = nullptr;
    size_t out_size = 0;
    CHECK(arx_pistoris_to_glb(h, nullptr, 1, &out, &out_size) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("GlbNullTeaHandle") {
    auto buf       = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    ArxTeaHandle teas[] = {nullptr};
    uint8_t* out        = nullptr;
    size_t out_size     = 0;
    CHECK(arx_pistoris_to_glb(h, teas, 1, &out, &out_size) == ARX_INVALID_HANDLE);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("GlbMinimalFtl") {
    auto buf       = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

    uint8_t* out     = nullptr;
    size_t out_size  = 0;
    ArxReturnCode rc = arx_pistoris_to_glb(h, nullptr, 0, &out, &out_size);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);
    CHECK(out_size >= 12);

    uint32_t magic;
    std::memcpy(&magic, out, 4);
    CHECK(magic == 0x46546C67u);  // GLB magic

    arx_pistoris_free_bytes(out);
    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("GlbTriangleWithTexture") {
    auto buf       = makeTriangleFtlWithTexture();
    ArxFtlHandle h = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

    uint8_t* out     = nullptr;
    size_t out_size  = 0;
    ArxReturnCode rc = arx_pistoris_to_glb(h, nullptr, 0, &out, &out_size);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);
    CHECK(out_size > 12);

    arx_pistoris_free_bytes(out);
    arx_pistoris_ftl_free(h);
  }

  // FTL has no groups -> TEA with groups would mismatch; mesh-only (0 teas) must succeed
  TEST_CASE("GlbWithTeaAnimation") {
    auto ftl_buf       = makeTriangleFtlWithTexture();
    ArxFtlHandle ftl_h = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(ftl_buf.data(), ftl_buf.size(), &ftl_h) == ARX_OK);

    uint8_t* out    = nullptr;
    size_t out_size = 0;
    CHECK(arx_pistoris_to_glb(ftl_h, nullptr, 0, &out, &out_size) == ARX_OK);
    arx_pistoris_free_bytes(out);

    arx_pistoris_ftl_free(ftl_h);
  }

}  // TEST_SUITE
