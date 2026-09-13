// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"

#include "utils/spatial/xz_point_index.h"

#include <cstddef>
#include <cstdint>
#include <vector>

TEST_SUITE("spatial::XzPointIndex") {
  TEST_CASE("Returns all points in neighboring cells in source order") {
    const std::vector<ArxVector3> points = {
        {80.0f, 0.0f, 90.0f},
        {120.0f, 0.0f, 20.0f},
        {-10.0f, 0.0f, 20.0f},
        {10.0f, 0.0f, 20.0f},
        {250.0f, 0.0f, 20.0f},
    };
    pistoris::spatial::XzPointIndex index;
    index.rebuild(points.size(), 100.0f, [&](std::size_t i) { return points[i]; });

    std::vector<std::uint32_t> candidates;
    index.findNeighborCellCandidates(candidates, 10.0f, 20.0f);

    CHECK(candidates == std::vector<std::uint32_t>{0, 1, 2, 3});
  }

  TEST_CASE("Rebuild replaces entries and retains negative-cell behavior") {
    std::vector<ArxVector3> points = {{-101.0f, 0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}};
    pistoris::spatial::XzPointIndex index;
    index.rebuild(points.size(), 100.0f, [&](std::size_t i) { return points[i]; });

    std::vector<std::uint32_t> candidates;
    index.findNeighborCellCandidates(candidates, -1.0f, 0.0f);
    CHECK(candidates == std::vector<std::uint32_t>{0, 1});

    points = {{500.0f, 0.0f, 500.0f}};
    index.rebuild(points.size(), 100.0f, [&](std::size_t i) { return points[i]; });
    index.findNeighborCellCandidates(candidates, -1.0f, 0.0f);
    CHECK(candidates.empty());
  }
}
