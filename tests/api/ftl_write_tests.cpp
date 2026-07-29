// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/ftl.hpp"

#include "helpers.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

TEST_SUITE("ftl") {
  TEST_CASE("WriteNullHandle") {
    uint8_t* out = nullptr;
    size_t sz = 0;
    ArxReturnCode rc = arx_pistoris_ftl_write(nullptr, 1, &out, &sz);
    CHECK(rc == ARX_INVALID_HANDLE);
    CHECK(out == nullptr);
    CHECK(sz == 0);
  }

  TEST_CASE("WriteNullOut") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    size_t sz = 0;
    ArxReturnCode rc = arx_pistoris_ftl_write(h, 1, nullptr, &sz);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("WriteNullSize") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    arx_pistoris_ftl_parse(buf.data(), buf.size(), &h);

    uint8_t* out = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_write(h, 1, &out, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("WriteSmoke") {
    std::vector<uint8_t> fixture = makeMinimalFtl();
    ArxFtlHandle h = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(fixture.data(), fixture.size(), &h) == ARX_OK);

    uint8_t* out = nullptr;
    size_t sz = 0;
    ArxReturnCode rc = arx_pistoris_ftl_write(h, 1, &out, &sz);
    CHECK(rc == ARX_OK);
    CHECK(out != nullptr);
    CHECK(sz > 0);

    arx_pistoris_free_bytes(out);
    arx_pistoris_ftl_free(h);
  }

  TEST_CASE("WriteUsesCompressedStorageAndParseAcceptsIt") {
    std::vector<uint8_t> fixture = makeMinimalFtl();
    ArxFtlHandle source = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(fixture.data(), fixture.size(), &source) == ARX_OK);

    uint8_t* out = nullptr;
    size_t size = 0;
    REQUIRE(arx_pistoris_ftl_write(source, 1, &out, &size) == ARX_OK);
    REQUIRE(out != nullptr);
    REQUIRE(size >= 2);
    CHECK(out[0] == 0);
    CHECK(out[1] == 6);

    ArxFtlHandle loaded = nullptr;
    CHECK(arx_pistoris_ftl_parse(out, size, &loaded) == ARX_OK);
    CHECK(loaded != nullptr);

    arx_pistoris_ftl_free(loaded);
    arx_pistoris_free_bytes(out);
    arx_pistoris_ftl_free(source);
  }

  TEST_CASE("WriteCanEmitRawStorage") {
    std::vector<uint8_t> fixture = makeMinimalFtl();
    ArxFtlHandle source = nullptr;
    REQUIRE(arx_pistoris_ftl_parse(fixture.data(), fixture.size(), &source) == ARX_OK);

    uint8_t* out = nullptr;
    size_t size = 0;
    REQUIRE(arx_pistoris_ftl_write(source, 0, &out, &size) == ARX_OK);
    REQUIRE(out != nullptr);
    REQUIRE(size >= sizeof(pistoris::kFtlMagic));
    CHECK(std::memcmp(out, pistoris::kFtlMagic, sizeof(pistoris::kFtlMagic)) == 0);

    ArxFtlHandle loaded = nullptr;
    CHECK(arx_pistoris_ftl_parse(out, size, &loaded) == ARX_OK);
    CHECK(loaded != nullptr);

    arx_pistoris_ftl_free(loaded);
    arx_pistoris_free_bytes(out);
    arx_pistoris_ftl_free(source);
  }

  TEST_CASE("OverwriteTexturePathsSmokeAndFailures") {
    SUBCASE("NullHandle") {
      CHECK(arx_pistoris_ftl_overwrite_texture_paths(nullptr, "GRAPH\\NEW.BMP") == ARX_INVALID_HANDLE);
    }

    SUBCASE("NullPath") {
      std::vector<uint8_t> buf = makeMinimalFtl();
      ArxFtlHandle h = nullptr;
      REQUIRE(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

      CHECK(arx_pistoris_ftl_overwrite_texture_paths(h, nullptr) == ARX_INVALID_DATA_POINTER);

      arx_pistoris_ftl_free(h);
    }

    SUBCASE("Smoke") {
      std::vector<uint8_t> buf = makeMinimalFtl();
      ArxFtlHandle h = nullptr;
      REQUIRE(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

      CHECK(arx_pistoris_ftl_overwrite_texture_paths(h, "GRAPH\\NEW.BMP") == ARX_OK);

      arx_pistoris_ftl_free(h);
    }
  }

  TEST_CASE("ApplyXformIdentityAndInvalid") {
    SUBCASE("Identity") {
      std::vector<uint8_t> buf = makeMinimalFtl();
      ArxFtlHandle h = nullptr;
      REQUIRE(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

      CHECK(arx_pistoris_ftl_apply_xform(h, 0, 0, 0, 1, 1, 1, 0, 0, 0) == ARX_OK);

      arx_pistoris_ftl_free(h);
    }

    SUBCASE("NullHandle") {
      CHECK(arx_pistoris_ftl_apply_xform(nullptr, 0, 0, 0, 1, 1, 1, 0, 0, 0) == ARX_INVALID_HANDLE);
    }

    SUBCASE("SingularMatrix") {
      std::vector<uint8_t> buf = makeMinimalFtl();
      ArxFtlHandle h = nullptr;
      REQUIRE(arx_pistoris_ftl_parse(buf.data(), buf.size(), &h) == ARX_OK);

      CHECK(arx_pistoris_ftl_apply_xform(h, 0, 0, 0, 0, 1, 1, 0, 0, 0) == ARX_INVALID_XFORM);  // sx=0

      arx_pistoris_ftl_free(h);
    }
  }

}  // TEST_SUITE("ftl")
