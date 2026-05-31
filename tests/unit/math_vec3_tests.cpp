// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.hpp"

#include "utils/math/vec3.h"

using namespace pistoris;

TEST_SUITE("math::vec3") {
  TEST_CASE("Addition") {
    ArxVector3 a{1, 2, 3}, b{4, 5, 6};
    auto c = a + b;
    CHECK(c.x == 5);
    CHECK(c.y == 7);
    CHECK(c.z == 9);
  }

  TEST_CASE("Subtraction") {
    ArxVector3 a{5, 7, 9}, b{1, 2, 3};
    auto c = a - b;
    CHECK(c.x == 4);
    CHECK(c.y == 5);
    CHECK(c.z == 6);
  }

  TEST_CASE("UnaryNegate") {
    ArxVector3 v{1, -2, 3};
    auto n = -v;
    CHECK(n.x == -1);
    CHECK(n.y == 2);
    CHECK(n.z == -3);
  }

  TEST_CASE("ScalarMul") {
    ArxVector3 v{1, 2, 3};
    auto a = v * 2.0f;
    auto b = 2.0f * v;
    CHECK(a.x == 2);
    CHECK(a.y == 4);
    CHECK(a.z == 6);
    CHECK(b == a);
  }

  TEST_CASE("ScalarDiv") {
    ArxVector3 v{2, 4, 6};
    auto q = v / 2.0f;
    CHECK(q.x == 1);
    CHECK(q.y == 2);
    CHECK(q.z == 3);
  }

  TEST_CASE("Equality") {
    ArxVector3 a{1, 2, 3}, b{1, 2, 3}, c{1, 2, 4};
    CHECK(a == b);
    CHECK_FALSE(a == c);
  }

  TEST_CASE("Dot") {
    ArxVector3 a{1, 2, 3}, b{4, -5, 6};
    CHECK(math::dot(a, b) == doctest::Approx(4 - 10 + 18));
  }

  TEST_CASE("CrossXYeqZ") {
    auto c = math::cross({1, 0, 0}, {0, 1, 0});
    CHECK(c.x == 0);
    CHECK(c.y == 0);
    CHECK(c.z == 1);
  }

  TEST_CASE("CrossAntiCommutative") {
    ArxVector3 a{1, 2, 3}, b{4, 5, 6};
    auto ab = math::cross(a, b);
    auto ba = math::cross(b, a);
    CHECK(ab.x == -ba.x);
    CHECK(ab.y == -ba.y);
    CHECK(ab.z == -ba.z);
  }

  TEST_CASE("Length") {
    ArxVector3 v{3, 4, 0};
    CHECK(math::length(v) == doctest::Approx(5.0f));
    CHECK(math::lengthSquared(v) == doctest::Approx(25.0f));
  }

  TEST_CASE("Normalize") {
    auto n = math::normalize({0, 3, 4});
    CHECK(n.x == doctest::Approx(0));
    CHECK(n.y == doctest::Approx(0.6f));
    CHECK(n.z == doctest::Approx(0.8f));
    CHECK(math::length(n) == doctest::Approx(1.0f));
  }

  TEST_CASE("NormalizeZeroYieldsZero") {
    auto n = math::normalize({0, 0, 0});
    CHECK(n.x == 0);
    CHECK(n.y == 0);
    CHECK(n.z == 0);
  }

  TEST_CASE("NormalizeOrZeroYieldsFallback") {
    auto n = math::normalizeOr({0, 0, 0}, {1, 0, 0});
    CHECK(n.x == 1);
    CHECK(n.y == 0);
    CHECK(n.z == 0);
  }

  TEST_CASE("NormalizeOrNonzeroYieldsUnit") {
    auto n = math::normalizeOr({0, 3, 4}, {1, 0, 0});
    CHECK(math::length(n) == doctest::Approx(1.0f));
  }

  TEST_CASE("ComponentMinMax") {
    ArxVector3 a{1, 5, 3}, b{4, 2, 6};
    auto mn = math::componentMin(a, b);
    auto mx = math::componentMax(a, b);
    CHECK(mn.x == 1);
    CHECK(mn.y == 2);
    CHECK(mn.z == 3);
    CHECK(mx.x == 4);
    CHECK(mx.y == 5);
    CHECK(mx.z == 6);
  }
}
