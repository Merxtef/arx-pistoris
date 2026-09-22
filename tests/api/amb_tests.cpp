// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/text.h"

#include "amb_helpers.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

TEST_SUITE("amb") {
  TEST_CASE("AmbNullArguments") {
    ArxAmb* amb = nullptr;
    CHECK(arx_pistoris_amb_read(nullptr, 0, &amb) == ARX_INVALID_DATA_POINTER);

    const std::uint8_t dummy = 0;
    CHECK(arx_pistoris_amb_read(&dummy, sizeof(dummy), nullptr) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_amb_validate(nullptr) == ARX_INVALID_HANDLE);

    std::uint8_t* out = nullptr;
    std::size_t size = 0;
    CHECK(arx_pistoris_amb_write(nullptr, &out, &size) == ARX_INVALID_HANDLE);
  }

  TEST_CASE("AmbReadWriteRoundtrip") {
    const std::vector<std::uint8_t> bytes = makeAmbBytes(pistoris::kAmbVersion1003);
    ArxAmb* amb = nullptr;
    REQUIRE(arx_pistoris_amb_read(bytes.data(), bytes.size(), &amb) == ARX_OK);
    REQUIRE(amb != nullptr);
    CHECK(arx_pistoris_amb_validate(amb) == ARX_OK);

    std::uint8_t* written = nullptr;
    std::size_t written_size = 0;
    REQUIRE(arx_pistoris_amb_write(amb, &written, &written_size) == ARX_OK);
    REQUIRE(written != nullptr);
    CHECK(written_size > 12);

    ArxAmb* roundtrip = nullptr;
    CHECK(arx_pistoris_amb_read(written, written_size, &roundtrip) == ARX_OK);
    CHECK(roundtrip != nullptr);

    arx_pistoris_amb_destroy(roundtrip);
    arx_pistoris_free_bytes(written);
    arx_pistoris_amb_destroy(amb);
  }

  TEST_CASE("AmbWriteNullOutputs") {
    const std::vector<std::uint8_t> bytes = makeAmbBytes();
    ArxAmb* amb = nullptr;
    REQUIRE(arx_pistoris_amb_read(bytes.data(), bytes.size(), &amb) == ARX_OK);

    std::uint8_t* out = nullptr;
    std::size_t size = 0;
    CHECK(arx_pistoris_amb_write(amb, nullptr, &size) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_amb_write(amb, &out, nullptr) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_amb_destroy(amb);
  }

  TEST_CASE("AmbJsonRoundtrip") {
    const std::vector<std::uint8_t> bytes = makeAmbBytes();
    ArxAmb* amb = nullptr;
    REQUIRE(arx_pistoris_amb_read(bytes.data(), bytes.size(), &amb) == ARX_OK);

    char* json = nullptr;
    REQUIRE(arx_pistoris_amb_to_json(amb, 1, ARX_NATIVE_TEXT_UTF8, &json) == ARX_OK);
    REQUIRE(json != nullptr);
    CHECK(std::strstr(json, "https://arx-tools.github.io/schemas/amb.schema.json") != nullptr);

    ArxAmb* imported = nullptr;
    REQUIRE(arx_pistoris_amb_from_json(
                reinterpret_cast<const std::uint8_t*>(json), std::strlen(json), ARX_NATIVE_TEXT_UTF8, &imported) ==
            ARX_OK);
    CHECK(arx_pistoris_amb_validate(imported) == ARX_OK);

    arx_pistoris_amb_destroy(imported);
    arx_pistoris_free_string(json);
    arx_pistoris_amb_destroy(amb);
  }

  TEST_CASE("AmbJsonNullArguments") {
    char* json = nullptr;
    CHECK(arx_pistoris_amb_to_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &json) == ARX_INVALID_HANDLE);

    const std::uint8_t empty = 0;
    ArxAmb* amb = nullptr;
    CHECK(arx_pistoris_amb_from_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &amb) == ARX_INVALID_DATA_POINTER);
    CHECK(arx_pistoris_amb_from_json(&empty, 0, ARX_NATIVE_TEXT_UTF8, nullptr) == ARX_INVALID_DATA_POINTER);
  }
}
