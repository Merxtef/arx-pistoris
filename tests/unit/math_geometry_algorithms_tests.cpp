// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.hpp"

#include "utils/math/geometry_algorithms.h"

#include <array>

using namespace pistoris;

TEST_SUITE("math::geometry") {
  TEST_CASE("PointInCircle") {
    CHECK(math::pointInCircle({3.0, 4.0}, {0.0, 0.0}, 5.0));
    CHECK(math::pointInCircle({1.0, 1.0}, {1.0, 1.0}, 0.0));
    CHECK_FALSE(math::pointInCircle({5.1, 0.0}, {0.0, 0.0}, 5.0));
  }

  TEST_CASE("ProjectedAreaAndBarycentricXz") {
    ArxVector3 a{0, 10, 0};
    ArxVector3 b{10, 20, 0};
    ArxVector3 c{0, 30, 10};
    CHECK(math::projectedAreaXz2(a, b, c) == doctest::Approx(100.0));

    std::array<double, 3> weights{};
    REQUIRE(math::barycentricXz(a, b, c, 2.5f, 2.5f, weights));
    CHECK(weights[0] == doctest::Approx(0.5));
    CHECK(weights[1] == doctest::Approx(0.25));
    CHECK(weights[2] == doctest::Approx(0.25));

    ArxVector3 p = math::interpolate(a, b, c, weights);
    CHECK(p.x == doctest::Approx(2.5f));
    CHECK(p.y == doctest::Approx(17.5f));
    CHECK(p.z == doctest::Approx(2.5f));
  }

  TEST_CASE("TriangleArea") { CHECK(math::triangleArea({0, 0, 0}, {3, 0, 0}, {0, 4, 0}) == doctest::Approx(6.0f)); }

  TEST_CASE("PointTriangleDistanceSquared") {
    const Vec3<double> a{0.0, 0.0, 0.0};
    const Vec3<double> b{2.0, 0.0, 0.0};
    const Vec3<double> c{0.0, 2.0, 0.0};

    CHECK(math::distancePointTriangleSquared({0.5, 0.5, 3.0}, a, b, c) == doctest::Approx(9.0));
    CHECK(math::distancePointTriangleSquared({-1.0, -1.0, 0.0}, a, b, c) == doctest::Approx(2.0));
    CHECK(math::distancePointTriangleSquared({1.0, -1.0, 0.0}, a, b, c) == doctest::Approx(1.0));

    const Vec3<double> degenerate_b{2.0, 0.0, 0.0};
    const Vec3<double> degenerate_c{4.0, 0.0, 0.0};
    CHECK(math::distancePointTriangleSquared({1.0, 1.0, 0.0}, a, degenerate_b, degenerate_c) == doctest::Approx(1.0));
  }

  TEST_CASE("PointSegmentDistanceSquared") {
    CHECK(math::distancePointSegmentSquared({1.0, 2.0, 0.0}, {0.0, 0.0, 0.0}, {2.0, 0.0, 0.0}) == doctest::Approx(4.0));
    CHECK(math::distancePointSegmentSquared({3.0, 4.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 0.0, 0.0}) ==
          doctest::Approx(20.0));
  }

  TEST_CASE("ClosestPointOnTriangle") {
    const Vec3<double> a{0.0, 0.0, 0.0};
    const Vec3<double> b{2.0, 0.0, 0.0};
    const Vec3<double> c{0.0, 2.0, 0.0};

    CHECK(math::closestPointOnTriangle({0.5, 0.5, 3.0}, a, b, c) == Vec3<double>{0.5, 0.5, 0.0});
    CHECK(math::closestPointOnTriangle({2.0, 2.0, 0.0}, a, b, c) == Vec3<double>{1.0, 1.0, 0.0});
    CHECK(math::closestPointOnTriangle({-1.0, -1.0, 0.0}, a, b, c) == a);

    const Vec3<double> degenerate_c{4.0, 0.0, 0.0};
    CHECK(math::closestPointOnTriangle({3.0, 1.0, 0.0}, a, b, degenerate_c) == Vec3<double>{3.0, 0.0, 0.0});

    const Vec3<double> edge_point{1.0, 1.0, 0.0};
    const Vec3<double> edge_projection{1.0, 0.0, 0.0};
    CHECK(math::closestPointOnTriangle(edge_point, a, a, b) == edge_projection);
    CHECK(math::closestPointOnTriangle(edge_point, a, b, b) == edge_projection);
    CHECK(math::closestPointOnTriangle(edge_point, a, b, a) == edge_projection);
    CHECK(math::closestPointOnTriangle(edge_point, a, a, a) == a);
    CHECK(math::distancePointTriangleSquared(edge_point, a, a, b) == doctest::Approx(1.0));
  }
}
