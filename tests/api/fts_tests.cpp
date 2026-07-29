// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/fts.hpp"

#include "helpers.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

TEST_SUITE("fts") {
  TEST_CASE("FtsNullData") {
    ArxFts* h = nullptr;
    ArxReturnCode rc = arx_pistoris_fts_parse(nullptr, 0, &h);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

  TEST_CASE("FtsNullOut") {
    uint8_t dummy = 0;
    ArxReturnCode rc = arx_pistoris_fts_parse(&dummy, sizeof(dummy), nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("FtsReadWriteSmoke") {
    std::vector<uint8_t> buf = makeMinimalFts();
    ArxFts* h = nullptr;
    REQUIRE(arx_pistoris_fts_parse(buf.data(), buf.size(), &h) == ARX_OK);
    REQUIRE(h != nullptr);

    CHECK(arx_pistoris_fts_validate(h) == ARX_OK);

    uint8_t* out = nullptr;
    size_t size = 0;
    CHECK(arx_pistoris_fts_write(h, 1, &out, &size) == ARX_OK);
    CHECK(out != nullptr);
    CHECK(size > 0);

    arx_pistoris_free_bytes(out);
    arx_pistoris_fts_destroy(h);
  }

  TEST_CASE("FtsWriteCompressesPayloadAndParseAcceptsIt") {
    std::vector<uint8_t> fixture = makeMinimalFts();
    ArxFts* source = nullptr;
    REQUIRE(arx_pistoris_fts_parse(fixture.data(), fixture.size(), &source) == ARX_OK);

    uint8_t* out = nullptr;
    size_t size = 0;
    REQUIRE(arx_pistoris_fts_write(source, 1, &out, &size) == ARX_OK);
    constexpr std::size_t kPrefixSize = sizeof(pistoris::fts::Header);
    REQUIRE(size >= kPrefixSize + 2);
    CHECK(out[kPrefixSize] == 0);
    CHECK(out[kPrefixSize + 1] == 6);

    ArxFts* loaded = nullptr;
    CHECK(arx_pistoris_fts_parse(out, size, &loaded) == ARX_OK);
    CHECK(loaded != nullptr);

    arx_pistoris_fts_destroy(loaded);
    arx_pistoris_free_bytes(out);
    arx_pistoris_fts_destroy(source);
  }

  TEST_CASE("FtsWriteCanEmitRawStorage") {
    std::vector<uint8_t> fixture = makeMinimalFts();
    ArxFts* source = nullptr;
    REQUIRE(arx_pistoris_fts_parse(fixture.data(), fixture.size(), &source) == ARX_OK);

    uint8_t* out = nullptr;
    size_t size = 0;
    REQUIRE(arx_pistoris_fts_write(source, 0, &out, &size) == ARX_OK);
    REQUIRE(size >= sizeof(pistoris::fts::Header));
    pistoris::fts::Header header{};
    std::memcpy(&header, out, sizeof(header));
    CHECK(header.uncompressedsize == 0);

    ArxFts* loaded = nullptr;
    CHECK(arx_pistoris_fts_parse(out, size, &loaded) == ARX_OK);
    CHECK(loaded != nullptr);

    arx_pistoris_fts_destroy(loaded);
    arx_pistoris_free_bytes(out);
    arx_pistoris_fts_destroy(source);
  }

  TEST_CASE("FtsWriteNulls") {
    uint8_t* out = nullptr;
    size_t size = 0;
    CHECK(arx_pistoris_fts_write(nullptr, 1, &out, &size) == ARX_INVALID_HANDLE);

    std::vector<uint8_t> buf = makeMinimalFts();
    ArxFts* h = nullptr;
    REQUIRE(arx_pistoris_fts_parse(buf.data(), buf.size(), &h) == ARX_OK);

    CHECK(arx_pistoris_fts_write(h, 1, nullptr, &size) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_fts_write(h, 1, &out, nullptr) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_fts_destroy(h);
  }
}
