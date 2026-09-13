// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"

#include <new>
#include <stdexcept>

namespace {

struct LogCapture {
  int errors = 0;
};

void captureLog(ArxLogLevel level, const char*, void* userdata) {
  if (level == ARX_LOG_ERROR) ++static_cast<LogCapture*>(userdata)->errors;
}

}  // namespace

TEST_SUITE("api::status_boundary") {
  TEST_CASE("Preserves status results and translates escaping exceptions") {
    CHECK(pistoris::api_detail::statusBoundary([] { return ARX_INVALID_OPTIONS; }) == ARX_INVALID_OPTIONS);
    CHECK(pistoris::api_detail::statusBoundary([]() -> ArxReturnCode { throw std::bad_alloc(); }) == ARX_BAD_ALLOC);

    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);
    CHECK(pistoris::api_detail::statusBoundary([]() -> ArxReturnCode { throw std::runtime_error("failure"); }) ==
          ARX_INTERNAL_ERROR);
    CHECK(pistoris::api_detail::statusBoundary([]() -> ArxReturnCode { throw 1; }) == ARX_INTERNAL_ERROR);
    pistoris::setLogCallback(nullptr, nullptr);
    CHECK(capture.errors == 2);
  }
}
