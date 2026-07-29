// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.hpp"

#include <type_traits>

using namespace pistoris;

TEST_SUITE("math::vec2") {
  TEST_CASE("Operators") {
    ArxVector2 a{1, 2}, b{4, 6};
    ArxVector2 sum{5, 8};
    ArxVector2 delta{3, 4};
    ArxVector2 negative{-1, -2};
    ArxVector2 scaled{2, 4};
    ArxVector2 halved{2, 3};
    CHECK((a + b) == sum);
    CHECK((b - a) == delta);
    CHECK((-a) == negative);
    CHECK((a * 2.0f) == scaled);
    CHECK((2.0f * a) == scaled);
    CHECK((b / 2.0f) == halved);
  }

  TEST_CASE("DotCrossLength") {
    ArxVector2 a{3, 4}, b{5, -2};
    CHECK(math::dotf(a, b) == doctest::Approx(7.0f));
    static_assert(std::is_same_v<decltype(math::dot(a, b)), double>);
    CHECK(math::dot(a, b) == doctest::Approx(7.0));
    CHECK(math::cross(a, b) == doctest::Approx(-26.0));
    CHECK(math::lengthf(a) == doctest::Approx(5.0f));
    static_assert(std::is_same_v<decltype(math::length(a)), double>);
    CHECK(math::length(a) == doctest::Approx(5.0));
    CHECK(math::lengthSquared(a) == doctest::Approx(25.0));
  }
}
