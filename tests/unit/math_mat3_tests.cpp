// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.hpp"

#include "utils/math/mat3.h"
#include "utils/math/quat.h"

#include <cmath>
#include <numbers>

using namespace pistoris;

static constexpr float kPi = std::numbers::pi_v<float>;

static void checkMat3Approx(const ArxMat3& a, const ArxMat3& b) {
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) CHECK(a(r, c) == doctest::Approx(b(r, c)));
}

TEST_SUITE("math::mat3") {
  TEST_CASE("IdentityAccessor") {
    const ArxMat3& id = math::kIdentityMat3;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c) CHECK(id(r, c) == (r == c ? 1.0f : 0.0f));
  }

  TEST_CASE("IdentityMultiply") {
    ArxMat3 m = math::kIdentityMat3;
    m(0, 1)   = 2.0f;
    m(2, 0)   = 3.0f;
    checkMat3Approx(m * math::kIdentityMat3, m);
    checkMat3Approx(math::kIdentityMat3 * m, m);
  }

  TEST_CASE("MultiplyAssociative") {
    ArxMat3 a = math::kIdentityMat3;
    a(0, 0)   = 2.0f;
    ArxMat3 b = math::fromEulerXYZ(0.3f, 0.4f, 0.5f);
    ArxMat3 c = math::kIdentityMat3;
    c(2, 2)   = 3.0f;
    checkMat3Approx((a * b) * c, a * (b * c));
  }

  TEST_CASE("TransformVectorIdentity") {
    auto v = math::kIdentityMat3 * ArxVector3{1, 2, 3};
    CHECK(v.x == doctest::Approx(1));
    CHECK(v.y == doctest::Approx(2));
    CHECK(v.z == doctest::Approx(3));
  }

  // Rz(90 deg): (x, y, z) -> (-y, x, z)
  TEST_CASE("TransformVectorRotation") {
    ArxMat3 rz = math::fromEulerXYZ(0, 0, kPi / 2.0f);
    auto v     = rz * ArxVector3{1, 0, 0};
    CHECK(v.x == doctest::Approx(0));
    CHECK(v.y == doctest::Approx(1));
    CHECK(v.z == doctest::Approx(0));
  }

  TEST_CASE("DeterminantIdentity") { CHECK(math::determinant(math::kIdentityMat3) == doctest::Approx(1.0f)); }

  TEST_CASE("DeterminantScaled") {
    ArxMat3 m = math::scaleColumns(math::kIdentityMat3, {2, 3, 5});
    CHECK(math::determinant(m) == doctest::Approx(30.0f));
  }

  TEST_CASE("DeterminantOfRotationIsOne") {
    ArxMat3 m = math::fromEulerXYZ(0.7f, 1.1f, -0.4f);
    CHECK(math::determinant(m) == doctest::Approx(1.0f));
  }

  // (M^-1)^T on identity = identity
  TEST_CASE("InverseTransposeIdentity") {
    checkMat3Approx(math::inverseTranspose(math::kIdentityMat3), math::kIdentityMat3);
  }

  // for orthonormal rotation R: (R^-1)^T = R
  TEST_CASE("InverseTransposeOfRotationIsRotation") {
    ArxMat3 r = math::fromEulerXYZ(0.5f, -0.3f, 0.9f);
    checkMat3Approx(math::inverseTranspose(r), r);
  }

  TEST_CASE("ExtractRotationIdentity") {
    ArxQuat q = math::extractRotation(math::kIdentityMat3);
    CHECK(q.w == doctest::Approx(1.0f));
    CHECK(q.x == doctest::Approx(0.0f));
    CHECK(q.y == doctest::Approx(0.0f));
    CHECK(q.z == doctest::Approx(0.0f));
  }

  // extract then re-apply should roundtrip a pure rotation's effect on a vector
  TEST_CASE("ExtractRotationRoundtripViaQuat") {
    ArxMat3 r = math::fromEulerXYZ(0.6f, 0.2f, -0.4f);
    ArxQuat q = math::extractRotation(r);

    ArxVector3 probe = {1.0f, 0.5f, -0.3f};
    ArxVector3 a     = r * probe;

    ArxQuat vq{0, probe.x, probe.y, probe.z};
    ArxQuat rq = q * vq * math::conjugate(q);
    CHECK(a.x == doctest::Approx(rq.x));
    CHECK(a.y == doctest::Approx(rq.y));
    CHECK(a.z == doctest::Approx(rq.z));
  }

  // Shepperd should recover rotation even when columns are scaled
  TEST_CASE("ExtractRotationIgnoresScale") {
    ArxMat3 r      = math::fromEulerXYZ(0.3f, -0.7f, 0.5f);
    ArxMat3 scaled = math::scaleColumns(r, {2, 3, 5});
    ArxQuat q1     = math::extractRotation(r);
    ArxQuat q2     = math::extractRotation(scaled);
    CHECK(q1.w == doctest::Approx(q2.w));
    CHECK(q1.x == doctest::Approx(q2.x));
    CHECK(q1.y == doctest::Approx(q2.y));
    CHECK(q1.z == doctest::Approx(q2.z));
  }

  TEST_CASE("ScaleColumnsIdentity") {
    ArxMat3 m = math::scaleColumns(math::kIdentityMat3, {2, 3, 5});
    CHECK(m(0, 0) == doctest::Approx(2));
    CHECK(m(1, 1) == doctest::Approx(3));
    CHECK(m(2, 2) == doctest::Approx(5));
  }

  // Euler(0,0,0) = identity
  TEST_CASE("FromEulerXYZZero") { checkMat3Approx(math::fromEulerXYZ(0, 0, 0), math::kIdentityMat3); }

  // Rx(90): (x,y,z) -> (x, -z, y)
  TEST_CASE("FromEulerXYZ90X") {
    ArxMat3 r = math::fromEulerXYZ(kPi / 2.0f, 0, 0);
    auto v    = r * ArxVector3{0, 1, 0};
    CHECK(v.x == doctest::Approx(0));
    CHECK(v.y == doctest::Approx(0));
    CHECK(v.z == doctest::Approx(1));
  }
}
