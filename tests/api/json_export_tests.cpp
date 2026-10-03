// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/text.h"

#include "helpers.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string makeMinimalFtsJson() {
  std::string result =
      R"({"$schema":"https://arx-tools.github.io/schemas/fts.schema.json","header":{"levelIdx":7,"mScenePosition":{"x":0,"y":0,"z":0}},"uniqueHeaders":[],"textureContainers":[],"cells":[)";
  for (std::size_t index = 0; index < 160U * 160U; ++index) {
    if (index != 0) result += ',';
    result += "{}";
  }
  result +=
      R"(],"polygons":[],"anchors":[],"portals":[],"rooms":[{"portals":[],"polygons":[]}],"roomDistances":[{"distance":-1,"startPosition":{"x":0,"y":0,"z":0},"endPosition":{"x":0,"y":0,"z":0}}]})";
  return result;
}

}  // namespace

TEST_SUITE("json") {
  TEST_CASE("JsonToJsonNullHandle") {
    char* out = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_to_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &out, nullptr);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("JsonToJsonNullOut") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtl* h = nullptr;
    arx_pistoris_ftl_read(buf.data(), buf.size(), &h, nullptr);

    ArxReturnCode rc = arx_pistoris_ftl_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, nullptr, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_ftl_destroy(h);
  }

  TEST_CASE("JsonFromJsonNullData") {
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &h, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("JsonFromJsonNullOut") {
    const uint8_t data[] = "{}";
    ArxReturnCode rc = arx_pistoris_ftl_from_json(data, sizeof(data) - 1, ARX_NATIVE_TEXT_UTF8, nullptr, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("JsonRoundtrip") {
    std::vector<uint8_t> buf = makeMinimalFtl();
    ArxFtl* h = nullptr;
    CHECK(arx_pistoris_ftl_read(buf.data(), buf.size(), &h, nullptr) == ARX_OK);

    char* json_str = nullptr;
    ArxReturnCode rc1 = arx_pistoris_ftl_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, &json_str, nullptr);
    CHECK(rc1 == ARX_OK);
    CHECK(json_str != nullptr);

    ArxFtl* h2 = nullptr;
    ArxReturnCode rc2 = arx_pistoris_ftl_from_json(
        reinterpret_cast<const uint8_t*>(json_str), std::strlen(json_str), ARX_NATIVE_TEXT_UTF8, &h2, nullptr);
    CHECK(rc2 == ARX_OK);
    CHECK(h2 != nullptr);

    arx_pistoris_free_string(json_str);
    arx_pistoris_ftl_destroy(h);
    arx_pistoris_ftl_destroy(h2);
  }

  TEST_CASE("JsonBadFormat") {
    const char* bad = "not json";
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(
        reinterpret_cast<const uint8_t*>(bad), std::strlen(bad), ARX_NATIVE_TEXT_UTF8, &h, nullptr);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  TEST_CASE("JsonErrorsDistinguishRootNestedAndUnknownLocations") {
    ArxFtl* ftl = nullptr;
    ArxError error = ARX_ERROR_INIT;

    constexpr std::string_view kRoot = "[]";
    CHECK(arx_pistoris_ftl_from_json(
              reinterpret_cast<const std::uint8_t*>(kRoot.data()), kRoot.size(), ARX_NATIVE_TEXT_UTF8, &ftl, &error) ==
          ARX_JSON_BAD_SCHEMA);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_JSON);
    CHECK(error.location.json_pointer.data != nullptr);
    CHECK(error.location.json_pointer.size == 0);

    constexpr std::string_view kNested =
        R"({"header":{"origin":0,"name":"test"},"vertices":[{"vector":{"x":"bad","y":0,"z":0},"norm":{"x":0,"y":0,"z":0}}]})";
    CHECK(arx_pistoris_ftl_from_json(reinterpret_cast<const std::uint8_t*>(kNested.data()),
                                     kNested.size(),
                                     ARX_NATIVE_TEXT_UTF8,
                                     &ftl,
                                     &error) == ARX_JSON_BAD_SCHEMA);
    CHECK(std::string_view(error.location.json_pointer.data, error.location.json_pointer.size).compare("/vertices/0") ==
          0);

    constexpr std::string_view kSemantic =
        R"({"header":{"origin":0,"name":"test"},"vertices":[{"vector":{"x":0,"y":0,"z":0},"norm":{"x":0,"y":1,"z":0}}],"faces":[],"textureContainers":[],"groups":[{"name":"root","origin":0,"indices":[1],"blobShadowSize":0}],"actions":[],"selections":[]})";
    CHECK(arx_pistoris_ftl_from_json(reinterpret_cast<const std::uint8_t*>(kSemantic.data()),
                                     kSemantic.size(),
                                     ARX_NATIVE_TEXT_UTF8,
                                     &ftl,
                                     &error) == ARX_FTL_BAD_GROUP_IDX);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_JSON);
    CHECK(std::string_view(error.location.json_pointer.data, error.location.json_pointer.size)
              .compare("/groups/0/indices/0") == 0);

    CHECK(arx_pistoris_ftl_from_json(reinterpret_cast<const std::uint8_t*>(kRoot.data()),
                                     kRoot.size(),
                                     static_cast<ArxNativeTextMode>(99),
                                     &ftl,
                                     &error) == ARX_INVALID_OPTIONS);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_NONE);
    CHECK(error.location.json_pointer.data == nullptr);
    CHECK(error.location.input_index == SIZE_MAX);

    constexpr std::array<std::uint8_t, 3> kInvalidUtf8 = {'{', 0xff, '}'};
    CHECK(arx_pistoris_ftl_from_json(kInvalidUtf8.data(), kInvalidUtf8.size(), ARX_NATIVE_TEXT_UTF8, &ftl, &error) ==
          ARX_TEXT_INVALID_UTF8);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_JSON);
    CHECK(error.location.byte_offset == 1);
    CHECK(error.location.json_pointer.data != nullptr);
    CHECK(error.location.json_pointer.size == 0);
    arx_pistoris_error_clear(&error);
  }

  TEST_CASE("JsonEmptyBadFormat") {
    const uint8_t empty = 0;
    ArxFtl* h = nullptr;
    ArxReturnCode rc = arx_pistoris_ftl_from_json(&empty, 0, ARX_NATIVE_TEXT_UTF8, &h, nullptr);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }

  TEST_CASE("NativeFtsJsonRoundtrip") {
    const std::string source = makeMinimalFtsJson();
    ArxFts* fts = nullptr;
    std::uint32_t level = 0;
    REQUIRE(arx_pistoris_fts_from_json(reinterpret_cast<const uint8_t*>(source.data()),
                                       source.size(),
                                       ARX_NATIVE_TEXT_UTF8,
                                       &fts,
                                       &level,
                                       nullptr) == ARX_OK);
    CHECK(level == 7);
    CHECK(arx_pistoris_fts_validate(fts, nullptr) == ARX_OK);

    char* exported = nullptr;
    REQUIRE(arx_pistoris_fts_to_json(fts, level, 1, ARX_NATIVE_TEXT_UTF8, &exported, nullptr) == ARX_OK);
    CHECK(std::strstr(exported, "\"levelIdx\": 7") != nullptr);

    ArxFts* roundtrip = nullptr;
    std::uint32_t roundtrip_level = 0;
    REQUIRE(arx_pistoris_fts_from_json(reinterpret_cast<const uint8_t*>(exported),
                                       std::strlen(exported),
                                       ARX_NATIVE_TEXT_UTF8,
                                       &roundtrip,
                                       &roundtrip_level,
                                       nullptr) == ARX_OK);
    CHECK(roundtrip_level == 7);
    CHECK(arx_pistoris_fts_validate(roundtrip, nullptr) == ARX_OK);

    constexpr std::uint8_t kInvalidJson[] = {'{'};
    ArxFts* failed = roundtrip;
    std::uint32_t failed_level = 42;
    CHECK(arx_pistoris_fts_from_json(
              kInvalidJson, sizeof(kInvalidJson), ARX_NATIVE_TEXT_UTF8, &failed, &failed_level, nullptr) ==
          ARX_JSON_BAD_FORMAT);
    CHECK(failed == nullptr);
    CHECK(failed_level == 0);

    ArxFts* missing_level = roundtrip;
    CHECK(arx_pistoris_fts_from_json(reinterpret_cast<const uint8_t*>(exported),
                                     std::strlen(exported),
                                     ARX_NATIVE_TEXT_UTF8,
                                     &missing_level,
                                     nullptr,
                                     nullptr) == ARX_INVALID_DATA_POINTER);

    arx_pistoris_fts_destroy(roundtrip);
    arx_pistoris_free_string(exported);
    arx_pistoris_fts_destroy(fts);
  }

  TEST_CASE("NativeFtsJsonDiscardsLegacySourceChecks") {
    std::string source = makeMinimalFtsJson();
    std::string source_checks = R"("uniqueHeaders":[{"path":"Level.scn","check":[)";
    for (std::size_t index = 0; index < 512; ++index) {
      if (index != 0) source_checks += ',';
      source_checks += '0';
    }
    source_checks += R"(]}])";
    constexpr std::string_view kEmptySourceChecks = R"("uniqueHeaders":[])";
    const std::size_t position = source.find(kEmptySourceChecks);
    REQUIRE(position != std::string::npos);
    source.replace(position, kEmptySourceChecks.size(), source_checks);

    ArxFts* fts = nullptr;
    std::uint32_t level = 0;
    REQUIRE(arx_pistoris_fts_from_json(reinterpret_cast<const std::uint8_t*>(source.data()),
                                       source.size(),
                                       ARX_NATIVE_TEXT_UTF8,
                                       &fts,
                                       &level,
                                       nullptr) == ARX_OK);

    char* exported = nullptr;
    REQUIRE(arx_pistoris_fts_to_json(fts, level, 1, ARX_NATIVE_TEXT_UTF8, &exported, nullptr) == ARX_OK);
    CHECK(std::strstr(exported, R"("uniqueHeaders": [])") != nullptr);
    CHECK(std::strstr(exported, "Level.scn") == nullptr);

    arx_pistoris_free_string(exported);
    arx_pistoris_fts_destroy(fts);

    std::string malformed = makeMinimalFtsJson();
    constexpr std::string_view kMalformedSourceChecks = R"("uniqueHeaders":[{"path":"Level.scn","check":[]}])";
    malformed.replace(malformed.find(kEmptySourceChecks), kEmptySourceChecks.size(), kMalformedSourceChecks);
    fts = nullptr;
    CHECK(arx_pistoris_fts_from_json(reinterpret_cast<const std::uint8_t*>(malformed.data()),
                                     malformed.size(),
                                     ARX_NATIVE_TEXT_UTF8,
                                     &fts,
                                     &level,
                                     nullptr) == ARX_JSON_BAD_SCHEMA);
    CHECK(fts == nullptr);
  }

  TEST_CASE("TeaJsonToJsonNullHandle") {
    char* out = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_to_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &out, nullptr);
    CHECK(rc == ARX_INVALID_HANDLE);
  }

  TEST_CASE("TeaJsonToJsonNullOut") {
    std::vector<uint8_t> buf = makeKeyframeTea();
    ArxTea* h = nullptr;
    arx_pistoris_tea_read(buf.data(), buf.size(), &h, nullptr);

    ArxReturnCode rc = arx_pistoris_tea_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, nullptr, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);

    arx_pistoris_tea_destroy(h);
  }

  TEST_CASE("TeaJsonFromJsonNullData") {
    ArxTea* h = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_from_json(nullptr, 0, ARX_NATIVE_TEXT_UTF8, &h, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("TeaJsonFromJsonNullOut") {
    const uint8_t data[] = "{}";
    ArxReturnCode rc = arx_pistoris_tea_from_json(data, sizeof(data) - 1, ARX_NATIVE_TEXT_UTF8, nullptr, nullptr);
    CHECK(rc == ARX_INVALID_DATA_POINTER);
  }

  TEST_CASE("TeaJsonRoundtrip") {
    std::vector<uint8_t> buf = makeKeyframeTea();
    ArxTea* h = nullptr;
    CHECK(arx_pistoris_tea_read(buf.data(), buf.size(), &h, nullptr) == ARX_OK);

    char* json_str = nullptr;
    ArxReturnCode rc1 = arx_pistoris_tea_to_json(h, 0, ARX_NATIVE_TEXT_UTF8, &json_str, nullptr);
    CHECK(rc1 == ARX_OK);
    CHECK(json_str != nullptr);

    ArxTea* h2 = nullptr;
    ArxReturnCode rc2 = arx_pistoris_tea_from_json(
        reinterpret_cast<const uint8_t*>(json_str), std::strlen(json_str), ARX_NATIVE_TEXT_UTF8, &h2, nullptr);
    CHECK(rc2 == ARX_OK);
    CHECK(h2 != nullptr);

    arx_pistoris_free_string(json_str);
    arx_pistoris_tea_destroy(h);
    arx_pistoris_tea_destroy(h2);
  }

  TEST_CASE("TeaJsonBadFormat") {
    const char* bad = "not json";
    ArxTea* h = nullptr;
    ArxReturnCode rc = arx_pistoris_tea_from_json(
        reinterpret_cast<const uint8_t*>(bad), std::strlen(bad), ARX_NATIVE_TEXT_UTF8, &h, nullptr);
    CHECK(rc == ARX_JSON_BAD_FORMAT);
    CHECK(h == nullptr);
  }
}
