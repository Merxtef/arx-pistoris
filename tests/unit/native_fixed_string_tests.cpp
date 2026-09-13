// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/runtime/types.h"

#include "native/fixed_string.h"
#include "utils/log.h"

#include <cstring>
#include <string>
#include <string_view>

using namespace pistoris;

struct LogCapture {
  ArxLogLevel last_level = ARX_LOG_DEBUG;
  std::string last_msg;

  LogCapture() {
    log_fn = [](ArxLogLevel level, const char* msg, void* ud) {
      auto* self = static_cast<LogCapture*>(ud);
      self->last_level = level;
      self->last_msg = msg;
    };
    log_ud = this;
  }
  ~LogCapture() {
    log_fn = nullptr;
    log_ud = nullptr;
  }
};

TEST_SUITE("native fixed string") {
  TEST_CASE("BoundedStringSemantics") {
    char terminated[4] = {'a', '\0', 'x', 'x'};
    char unterminated[4] = {'a', 'b', 'c', 'd'};

    CHECK(isNullTerminated(terminated));
    CHECK_FALSE(isNullTerminated(unterminated));
    CHECK(fitsNativeString<4>("abc"));
    CHECK_FALSE(fitsNativeString<4>("abcd"));
    CHECK_FALSE(fitsNativeString<4>(std::string_view("a\0b", 3)));
  }

  TEST_CASE("ClampStrNoop") {
    char arr[8] = "hello";
    clampStr(arr, "test");
    CHECK(arr[7] == '\0');
    CHECK(std::string(arr) == "hello");
  }

  TEST_CASE("ClampStrClampsEnd") {
    char arr[4] = {'A', 'B', 'C', 'D'};
    clampStr(arr, "test");
    CHECK(arr[3] == '\0');
    CHECK(arr[0] == 'A');
    CHECK(arr[1] == 'B');
    CHECK(arr[2] == 'C');
  }

  TEST_CASE("ClampStrSizes") {
    char s2[2] = {'X', 'Y'};
    clampStr(s2, "test");
    CHECK(s2[1] == '\0');

    char s64[64];
    std::memset(s64, 'Z', sizeof(s64));
    clampStr(s64, "test");
    CHECK(s64[63] == '\0');

    char s256[256];
    std::memset(s256, 'Z', sizeof(s256));
    clampStr(s256, "test");
    CHECK(s256[255] == '\0');
  }

  TEST_CASE("ClampStrLogUnindexed") {
    LogCapture cap;
    char arr[4] = {'A', 'B', 'C', 'D'};
    clampStr(arr, "FTL: header.name");
    CHECK(cap.last_level == ARX_LOG_WARN);
    CHECK(cap.last_msg.find("FTL: header.name") != std::string::npos);
    CHECK(cap.last_msg.find("not null-terminated") != std::string::npos);
    CHECK(cap.last_msg.find('[') == std::string::npos);
  }

  TEST_CASE("ClampStrLogIndexed") {
    LogCapture cap;
    char arr[4] = {'A', 'B', 'C', 'D'};
    clampStr(arr, "FTL: group.name", 2);
    CHECK(cap.last_level == ARX_LOG_WARN);
    CHECK(cap.last_msg.find("FTL: group.name") != std::string::npos);
    CHECK(cap.last_msg.find("[2]") != std::string::npos);
    CHECK(cap.last_msg.find("not null-terminated") != std::string::npos);
  }

  TEST_CASE("ClampStrNoLogOnNoop") {
    LogCapture cap;
    char arr[4] = "hi";
    clampStr(arr, "FTL: header.name");
    CHECK(cap.last_msg.empty());
  }
}
