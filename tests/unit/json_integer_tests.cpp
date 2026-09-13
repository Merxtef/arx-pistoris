// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "external/json/native_common.h"

#include <cstdint>
#include <limits>

TEST_SUITE("json integers") {
  TEST_CASE("Signed conversion preserves range") {
    std::int32_t value = 0;
    CHECK(pistoris::json_detail::getSigned(
        pistoris::json_detail::Json(static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())), value));
    CHECK(value == std::numeric_limits<std::int32_t>::max());
    CHECK(pistoris::json_detail::getSigned(pistoris::json_detail::Json(std::int64_t{-1}), value));
    CHECK(value == -1);

    value = 7;
    CHECK_FALSE(pistoris::json_detail::getSigned(
        pistoris::json_detail::Json(static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1U), value));
    CHECK(value == 7);
    CHECK_FALSE(pistoris::json_detail::getSigned(pistoris::json_detail::Json(std::numeric_limits<std::uint64_t>::max()),
                                                 value));
    CHECK(value == 7);
  }

  TEST_CASE("Unsigned conversion preserves range") {
    std::uint32_t value = 0;
    CHECK(pistoris::json_detail::getUnsigned(pistoris::json_detail::Json(std::numeric_limits<std::uint32_t>::max()),
                                             value));
    CHECK(value == std::numeric_limits<std::uint32_t>::max());
    CHECK(pistoris::json_detail::getUnsigned(pistoris::json_detail::Json(std::int64_t{0}), value));
    CHECK(value == 0);

    value = 7;
    CHECK_FALSE(pistoris::json_detail::getUnsigned(pistoris::json_detail::Json(std::int64_t{-1}), value));
    CHECK(value == 7);
    CHECK_FALSE(pistoris::json_detail::getUnsigned(
        pistoris::json_detail::Json(std::numeric_limits<std::uint64_t>::max()), value));
    CHECK(value == 7);
  }
}
