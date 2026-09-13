// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "arx_pistoris/arx_pistoris.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

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
    const char* h = arx_pistoris_layout_hash();
    REQUIRE(h != nullptr);
    REQUIRE(std::strlen(h) == 64);
    for (int i = 0; i < 64; ++i) {
      char c = h[i];
      CHECK(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
    }
  }

  TEST_CASE("log callback fires") {
    std::string captured;
    ArxLogLevel captured_level = ARX_LOG_DEBUG;
    std::pair<std::string*, ArxLogLevel*> capture = {&captured, &captured_level};
    arx_pistoris_set_log_callback(
        [](ArxLogLevel level, const char* msg, void* ud) {
          auto* p = static_cast<std::pair<std::string*, ArxLogLevel*>*>(ud);
          if (level != ARX_LOG_WARN) return;
          *p->first = msg;
          *p->second = level;
        },
        &capture);

    const char* obj = "# arx_unknown\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    ArxModel* model = nullptr;
    arx_pistoris_model_import_obj(reinterpret_cast<const uint8_t*>(obj), std::strlen(obj), nullptr, 0, &model, nullptr);
    arx_pistoris_model_destroy(model);
    arx_pistoris_set_log_callback(nullptr, nullptr);

    CHECK(captured_level == ARX_LOG_WARN);
    CHECK(!captured.empty());
  }

  // tests/check_rc_coverage.py checks assigned-code completeness; runtime verifies unknown values
  TEST_CASE("StrerrorFallthroughForUnknownCodes") {
    CHECK(std::string(arx_pistoris_strerror(INT32_MAX)) == "unknown error code");
    CHECK(std::string(arx_pistoris_strerror(static_cast<ArxReturnCode>(9999))) == "unknown error code");
  }

  TEST_CASE("Native validation rejects null handles") {
    CHECK(arx_pistoris_ftl_validate(nullptr) == ARX_INVALID_HANDLE);
    CHECK(arx_pistoris_tea_validate(nullptr) == ARX_INVALID_HANDLE);
  }

}  // TEST_SUITE("basic")
