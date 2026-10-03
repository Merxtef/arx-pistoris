// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

TEST_SUITE("ftl") {
  TEST_CASE("FtlNullData") {
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_read(nullptr, 0, &h, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
    CHECK(h == nullptr);
  }

  TEST_CASE("FtlNullOut") {
    uint8_t dummy = 0;
    ArxReturnCode rc = arx_pistoris_ftl_read(&dummy, sizeof(dummy), nullptr, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("FtlReadSmoke") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_read(buf.data(), buf.size(), &h, nullptr);
    CHECK(rc == ARX_OK);
    CHECK(h != nullptr);
    arx_pistoris_ftl_destroy(h);
  }

  TEST_CASE("FtlReadPublishesAndClearsStructuredErrors") {
    const std::uint8_t truncated = 0;
    ArxFtl* h = nullptr;
    ArxError error = ARX_ERROR_INIT;

    CHECK(arx_pistoris_ftl_read(&truncated, 1, &h, &error) == ARX_UNEXPECTED_EOF);
    CHECK(h == nullptr);
    CHECK(error.code == ARX_UNEXPECTED_EOF);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_FTL_BINARY);
    CHECK(error.location.element == ARX_ERROR_ELEMENT_FTL_HEADER);
    CHECK(std::string_view(error.location.element_name.data, error.location.element_name.size).compare("FTL header") ==
          0);
    CHECK(std::string_view(error.location.field.data, error.location.field.size).compare("identifier") == 0);
    CHECK(error.location.byte_offset == 0);
    CHECK(error.location.requested_bytes > 1);

    std::vector<std::uint8_t> valid = makeMinimalFtl();
    CHECK(arx_pistoris_ftl_read(valid.data(), valid.size(), &h, &error) == ARX_OK);
    REQUIRE(h != nullptr);
    CHECK(error.code == ARX_OK);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_NONE);
    CHECK(error.location.input_index == SIZE_MAX);
    CHECK(error.location.index == SIZE_MAX);
    CHECK(error.location.subindex == SIZE_MAX);
    CHECK(error.location.byte_offset == SIZE_MAX);
    CHECK(error.private_data == nullptr);
    arx_pistoris_ftl_destroy(h);

    arx_pistoris_error_clear(&error);
    arx_pistoris_error_clear(&error);
    arx_pistoris_error_clear(nullptr);
  }

  TEST_CASE("FtlReadPublishesPostReadSemanticLocations") {
    std::vector<std::uint8_t> bytes = makeMinimalFtl();
    const std::int32_t one = 1;
    std::memcpy(bytes.data() + kFtlNFacesOff, &one, sizeof(one));
    bytes.resize(kFtlDataOff + kFtlVertexSize + kFtlFaceSize, 0);
    const std::uint16_t indices[3] = {1, 0, 0};
    std::memcpy(bytes.data() + kFtlDataOff + kFtlVertexSize + kFtlFaceOffVertIdx, indices, sizeof(indices));
    const std::int16_t no_texture = -1;
    std::memcpy(bytes.data() + kFtlDataOff + kFtlVertexSize + kFtlFaceOffTexId, &no_texture, sizeof(no_texture));

    ArxFtl* ftl = nullptr;
    ArxError error = ARX_ERROR_INIT;
    CHECK(arx_pistoris_ftl_read(bytes.data(), bytes.size(), &ftl, &error) == ARX_FTL_BAD_FACE_VERT_IDX);
    CHECK(ftl == nullptr);
    CHECK(error.code == ARX_FTL_BAD_FACE_VERT_IDX);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_FTL_BINARY);
    CHECK(error.location.element == ARX_ERROR_ELEMENT_FTL_FACE);
    CHECK(error.location.index == 0);
    CHECK(error.location.subindex == 0);
    CHECK(std::string(error.location.field.data, error.location.field.size) == "vertex_idx");
    CHECK(error.location.byte_offset == SIZE_MAX);
    CHECK(error.location.requested_bytes == 0);
    arx_pistoris_error_clear(&error);
  }

}  // TEST_SUITE("ftl")
