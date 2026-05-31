// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include "helpers.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

TEST_SUITE("basic") {
  TEST_CASE("version string is non-null and non-empty") {
    const char* ver = arx_pistoris_version();
    CHECK(ver != nullptr);
    CHECK(std::strlen(ver) > 0);
  }

  TEST_CASE("build_time is a plausible ISO-8601 UTC string") {
    const char* bt = arx_pistoris_build_time();
    REQUIRE(bt != nullptr);
    // "YYYY-MM-DDTHH:MM:SSZ" = 20 chars; non-deliverable presets append " (reconfig)"
    REQUIRE(std::strlen(bt) >= 20);
    CHECK(bt[4] == '-');
    CHECK(bt[7] == '-');
    CHECK(bt[10] == 'T');
    CHECK(bt[13] == ':');
    CHECK(bt[16] == ':');
    CHECK(bt[19] == 'Z');
  }

  // SHA256 of public headers; baked in by configure_file at CMake time
  TEST_CASE("layout hash is a 64-char lowercase hex string") {
    const char* h = arx_pistoris_get_layout_hash();
    REQUIRE(h != nullptr);
    REQUIRE(std::strlen(h) == 64);
    for (int i = 0; i < 64; ++i) {
      char c = h[i];
      CHECK(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
  }

  // bare 'usemtl' (no name) fires ARX_LOG_WARN; callback must capture it
  TEST_CASE("log callback fires") {
    std::string captured;
    ArxLogLevel captured_level                    = ARX_LOG_DEBUG;
    std::pair<std::string*, ArxLogLevel*> capture = {&captured, &captured_level};
    arx_pistoris_set_log_callback(
        [](ArxLogLevel level, const char* msg, void* ud) {
          auto* p    = static_cast<std::pair<std::string*, ArxLogLevel*>*>(ud);
          *p->first  = msg;
          *p->second = level;
        },
        &capture);

    const char* obj = "usemtl\n";
    ArxFtlHandle h  = nullptr;
    arx_pistoris_obj_parse(reinterpret_cast<const uint8_t*>(obj), std::strlen(obj), nullptr, 0, nullptr, &h);
    arx_pistoris_ftl_free(h);
    arx_pistoris_set_log_callback(nullptr, nullptr);

    CHECK(captured_level == ARX_LOG_WARN);
    CHECK(!captured.empty());
  }

  // -Werror=switch covers per-code completeness at compile time (non-Release); only fallthrough needs runtime check
  TEST_CASE("StrerrorFallthroughForUnknownCodes") {
    CHECK(std::string(arx_pistoris_strerror(ARX_RETURN_CODE_MAX)) == "unknown error code");
    CHECK(std::string(arx_pistoris_strerror(static_cast<ArxReturnCode>(9999))) == "unknown error code");
  }

  TEST_CASE("FreeTeaArrayNullSafe") {
    SUBCASE("NullPointerZeroCount") { arx_pistoris_free_tea_array(nullptr, 0); }

    SUBCASE("NullPointerNonZeroCount") { arx_pistoris_free_tea_array(nullptr, 5); }

    SUBCASE("HeapArrayOfTwoHandles") {
      std::vector<uint8_t> buf = makeMinimalTea();
      setNumKeyframes(buf, 1);
      appendKeyframe2014(buf);

      ArxTeaHandle h0 = nullptr;
      ArxTeaHandle h1 = nullptr;
      REQUIRE(arx_pistoris_tea_parse(buf.data(), buf.size(), &h0) == ARX_OK);
      REQUIRE(arx_pistoris_tea_parse(buf.data(), buf.size(), &h1) == ARX_OK);

      auto* arr = new ArxTeaHandle[2]{h0, h1};
      arx_pistoris_free_tea_array(arr, 2);
    }
  }

  TEST_CASE("NewCUtilitiesRejectNullHandles") {
    CHECK(arx_pistoris_ftl_validate(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_tea_validate(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_ftl_snap_bone_origins_to_reference(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_ftl_snap_action_points_to_reference(nullptr, nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_ftl_copy_synthetic_selection_affiliations(nullptr, nullptr) == ARX_INVALID_HANDLE);
  }

}  // TEST_SUITE("basic")
