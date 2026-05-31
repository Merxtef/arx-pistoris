// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.hpp"

#include "utils/math/quat.h"

#include <cmath>

using namespace pistoris;

TEST_SUITE("math::quat") {
  TEST_CASE("Identity") {
    ArxQuat id = math::kIdentityQuat;
    CHECK(id.w == 1.0f);
    CHECK(id.x == 0.0f);
    CHECK(id.y == 0.0f);
    CHECK(id.z == 0.0f);
  }

  TEST_CASE("MultiplyByIdentity") {
    ArxQuat q{0.1f, 0.2f, 0.3f, 0.4f};
    auto lr = q * math::kIdentityQuat;
    auto rl = math::kIdentityQuat * q;
    CHECK(lr.w == doctest::Approx(q.w));
    CHECK(lr.x == doctest::Approx(q.x));
    CHECK(lr.y == doctest::Approx(q.y));
    CHECK(lr.z == doctest::Approx(q.z));
    CHECK(rl.w == doctest::Approx(q.w));
    CHECK(rl.x == doctest::Approx(q.x));
    CHECK(rl.y == doctest::Approx(q.y));
    CHECK(rl.z == doctest::Approx(q.z));
  }

  TEST_CASE("Conjugate") {
    ArxQuat q{1, 2, 3, 4};
    auto c = math::conjugate(q);
    CHECK(c.w == 1);
    CHECK(c.x == -2);
    CHECK(c.y == -3);
    CHECK(c.z == -4);
  }

  TEST_CASE("Norm") {
    CHECK(math::norm({1, 0, 0, 0}) == doctest::Approx(1.0f));
    CHECK(math::norm({2, 0, 0, 0}) == doctest::Approx(2.0f));
    CHECK(math::norm({0, 1, 2, 2}) == doctest::Approx(3.0f));
  }

  TEST_CASE("NormalizeProducesUnit") {
    auto n = math::normalize({2.0f, 0.0f, 0.0f, 0.0f});
    CHECK(math::norm(n) == doctest::Approx(1.0f));
  }

  TEST_CASE("NormalizeZeroYieldsIdentity") {
    auto n = math::normalize({0, 0, 0, 0});
    CHECK(n == math::kIdentityQuat);
  }

  // q * q^-1 = identity; for unit q, q^-1 = conjugate(q)
  TEST_CASE("MultiplyByConjugateYieldsIdentity") {
    float s = std::sqrt(0.5f);
    ArxQuat q{s, s, 0, 0};  // 90 deg about X
    auto r = q * math::conjugate(q);
    CHECK(r.w == doctest::Approx(1.0f));
    CHECK(r.x == doctest::Approx(0.0f));
    CHECK(r.y == doctest::Approx(0.0f));
    CHECK(r.z == doctest::Approx(0.0f));
  }

  TEST_CASE("MultiplyNonCommutative") {
    float s = std::sqrt(0.5f);
    ArxQuat rx{s, s, 0, 0};
    ArxQuat ry{s, 0, s, 0};
    CHECK_FALSE(rx * ry == ry * rx);
  }

  TEST_CASE("Equality") {
    CHECK(ArxQuat{1, 2, 3, 4} == ArxQuat{1, 2, 3, 4});
    CHECK_FALSE(ArxQuat{1, 2, 3, 4} == ArxQuat{1, 2, 3, 5});
  }
}
