// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.hpp"

#include "utils/math/mat4.h"
#include "utils/math/quat.h"
#include "utils/math/vec3.h"

#include <cmath>

using namespace pistoris;
using math::Mat4;

static void checkMat4Approx(const Mat4& a, const Mat4& b) {
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row) CHECK(a(row, col) == doctest::Approx(b(row, col)));
}

TEST_SUITE("math::mat4") {
  TEST_CASE("IdentityAccessor") {
    const Mat4& id = math::kIdentityMat4;
    for (int r = 0; r < 4; ++r)
      for (int c = 0; c < 4; ++c) CHECK(id(r, c) == (r == c ? 1.0f : 0.0f));
  }

  TEST_CASE("IdentityMultiply") {
    Mat4 m  = math::kIdentityMat4;
    m(0, 3) = 7.0f;
    m(1, 3) = 8.0f;
    m(2, 3) = 9.0f;
    checkMat4Approx(m * math::kIdentityMat4, m);
    checkMat4Approx(math::kIdentityMat4 * m, m);
  }

  TEST_CASE("MultiplyAssociative") {
    Mat4 a  = math::kIdentityMat4;
    a(0, 0) = 2.0f;
    Mat4 b  = math::kIdentityMat4;
    b(1, 1) = 3.0f;
    Mat4 c  = math::kIdentityMat4;
    c(2, 2) = 5.0f;
    checkMat4Approx((a * b) * c, a * (b * c));
  }

  TEST_CASE("TransformPoint") {
    Mat4 m  = math::kIdentityMat4;
    m(0, 3) = 10.0f;
    m(1, 3) = 20.0f;
    m(2, 3) = 30.0f;
    auto p  = math::xformPoint(m, {1, 2, 3});
    CHECK(p.x == doctest::Approx(11));
    CHECK(p.y == doctest::Approx(22));
    CHECK(p.z == doctest::Approx(33));
  }

  TEST_CASE("TransformDirIgnoresTranslation") {
    Mat4 m  = math::kIdentityMat4;
    m(0, 3) = 100.0f;
    m(1, 3) = 200.0f;
    m(2, 3) = 300.0f;
    auto v  = math::xformDir(m, {1, 2, 3});
    CHECK(v.x == doctest::Approx(1));
    CHECK(v.y == doctest::Approx(2));
    CHECK(v.z == doctest::Approx(3));
  }

  TEST_CASE("FromQuatIdentity") {
    auto m = math::fromQuat(math::kIdentityQuat);
    checkMat4Approx(m, math::kIdentityMat4);
  }

  // 180 deg about Y: (x,y,z) -> (-x, y, -z)
  TEST_CASE("FromQuat180Y") {
    ArxQuat q{0.0f, 0.0f, 1.0f, 0.0f};
    Mat4 m = math::fromQuat(q);
    auto p = math::xformPoint(m, {1, 2, 3});
    CHECK(p.x == doctest::Approx(-1));
    CHECK(p.y == doctest::Approx(2));
    CHECK(p.z == doctest::Approx(-3));
  }

  TEST_CASE("FromTrsTranslationColumn") {
    ArxVector3 t{5, 6, 7};
    Mat4 m = math::fromTrs(t, math::kIdentityQuat, {1, 1, 1});
    auto k = math::translation(m);
    CHECK(k.x == doctest::Approx(5));
    CHECK(k.y == doctest::Approx(6));
    CHECK(k.z == doctest::Approx(7));
  }

  TEST_CASE("FromTrsAppliesScaleThenTranslate") {
    Mat4 m = math::fromTrs({10, 20, 30}, math::kIdentityQuat, {2, 3, 4});
    auto p = math::xformPoint(m, {1, 1, 1});
    CHECK(p.x == doctest::Approx(12));
    CHECK(p.y == doctest::Approx(23));
    CHECK(p.z == doctest::Approx(34));
  }

  TEST_CASE("InverseIdentityIsIdentity") {
    auto inv = math::inverseAffine(math::kIdentityMat4);
    REQUIRE(inv.has_value());
    checkMat4Approx(*inv, math::kIdentityMat4);
  }

  TEST_CASE("InverseTranslation") {
    Mat4 m   = math::kIdentityMat4;
    m(0, 3)  = 7;
    m(1, 3)  = 8;
    m(2, 3)  = 9;
    auto inv = math::inverseAffine(m);
    REQUIRE(inv.has_value());
    auto k = math::translation(*inv);
    CHECK(k.x == doctest::Approx(-7));
    CHECK(k.y == doctest::Approx(-8));
    CHECK(k.z == doctest::Approx(-9));
  }

  TEST_CASE("InverseRoundtripWithTrs") {
    float s = std::sqrt(0.5f);
    ArxQuat r{s, s, 0, 0};  // 90 deg X
    Mat4 m   = math::fromTrs({1, 2, 3}, r, {1, 1, 1});
    auto inv = math::inverseAffine(m);
    REQUIRE(inv.has_value());
    checkMat4Approx(m * *inv, math::kIdentityMat4);
    checkMat4Approx(*inv * m, math::kIdentityMat4);
  }

  TEST_CASE("InverseSingularYieldsNullopt") {
    Mat4 m   = math::kIdentityMat4;
    m(0, 0)  = 0;
    m(1, 1)  = 0;
    m(2, 2)  = 0;
    auto inv = math::inverseAffine(m);
    CHECK_FALSE(inv.has_value());
  }

  TEST_CASE("IsRotationUniformScaleAcceptsIdentity") { CHECK(math::isRotationUniformScale(math::kIdentityMat4)); }

  TEST_CASE("IsRotationUniformScaleAcceptsUniformScaledRotation") {
    float s = std::sqrt(0.5f);
    ArxQuat r{s, s, 0, 0};
    Mat4 m = math::fromTrs({0, 0, 0}, r, {2.5f, 2.5f, 2.5f});
    CHECK(math::isRotationUniformScale(m));
  }

  TEST_CASE("IsRotationUniformScaleRejectsNonUniformScale") {
    Mat4 m  = math::kIdentityMat4;
    m(0, 0) = 2.0f;
    m(1, 1) = 1.0f;
    m(2, 2) = 1.0f;
    CHECK_FALSE(math::isRotationUniformScale(m));
  }

  TEST_CASE("IsRotationUniformScaleRejectsShear") {
    Mat4 m  = math::kIdentityMat4;
    m(0, 1) = 0.5f;  // X column has a Y component -> not orthogonal
    CHECK_FALSE(math::isRotationUniformScale(m));
  }
}
