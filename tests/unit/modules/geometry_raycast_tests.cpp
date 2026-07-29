// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "modules/geometry.h"

using namespace pistoris;

TEST_SUITE("geometry::raycast") {
  TEST_CASE("Finds segment triangle intersection t") {
    double t = 0.0;

    CHECK(geometry::segmentTriangleIntersectionT(
        {0.25f, 0.25f, -1.0f}, {0.25f, 0.25f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, t));
    CHECK(t == doctest::Approx(0.5));
  }

  TEST_CASE("Rejects misses and intersections outside the segment") {
    double t = 0.0;

    CHECK_FALSE(geometry::segmentTriangleIntersectionT(
        {0.1f, 0.1f, 1.0f}, {0.8f, 0.1f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, t));
    CHECK_FALSE(geometry::segmentTriangleIntersectionT(
        {2.0f, 2.0f, -1.0f}, {2.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, t));
    CHECK_FALSE(geometry::segmentTriangleIntersectionT(
        {0.25f, 0.25f, 1.0f}, {0.25f, 0.25f, 2.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, t));
  }

  TEST_CASE("Boolean segment triangle helper wraps t variant") {
    CHECK(geometry::segmentIntersectsTriangle(
        {0.25f, 0.25f, -1.0f}, {0.25f, 0.25f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
    CHECK_FALSE(geometry::segmentIntersectsTriangle(
        {2.0f, 2.0f, -1.0f}, {2.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
  }
}
