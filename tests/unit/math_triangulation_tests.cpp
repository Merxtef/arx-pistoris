// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"

#include "utils/math/triangulation.h"

#include <array>
#include <cstdint>
#include <vector>

TEST_SUITE("math triangulation") {
  TEST_CASE("Concave simple polygons triangulate without a fan") {
    constexpr std::array<pistoris::ArxVector2, 5> kPoints = {
        {{0.0f, 0.0f}, {4.0f, 0.0f}, {4.0f, 4.0f}, {2.0f, 1.0f}, {0.0f, 4.0f}}};
    std::vector<std::uint32_t> triangles;
    REQUIRE(pistoris::math::triangulateSimplePolygon(kPoints, triangles) ==
            pistoris::math::TriangulationResult::kSuccess);
    CHECK(triangles.size() == 9);
  }

  TEST_CASE("Self-crossing polygons are reported without partial output") {
    constexpr std::array<pistoris::ArxVector2, 4> kPoints = {{{0.0f, 0.0f}, {4.0f, 4.0f}, {0.0f, 4.0f}, {4.0f, 0.0f}}};
    std::vector<std::uint32_t> triangles = {99};
    REQUIRE(pistoris::math::triangulateSimplePolygon(kPoints, triangles) ==
            pistoris::math::TriangulationResult::kNonSimple);
    CHECK(triangles.empty());
  }
}
