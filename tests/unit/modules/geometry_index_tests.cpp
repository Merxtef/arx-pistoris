// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "utils/spatial/arx_level_grid.h"
#include "utils/spatial/hash_grid.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using namespace pistoris;

namespace {

geometry::IndexedTriangle makeIndexed(std::array<ArxVector3, 3> vertices) {
  return {vertices, geometry::triangleBounds(vertices)};
}

}  // namespace

TEST_SUITE("geometry::index") {
  TEST_CASE("Cell coordinates saturate finite extremes with neighbor headroom") {
    const float max = std::numeric_limits<float>::max();
    CHECK(spatial::cellCoord(max, 1.0f) == spatial::kMaxCellCoord);
    CHECK(spatial::cellCoord(-max, 1.0f) == spatial::kMinCellCoord);
    CHECK(spatial::cellCoord(max, 1.0f) + 1 == std::numeric_limits<int>::max());
    CHECK(spatial::cellCoord(-max, 1.0f) - 1 == std::numeric_limits<int>::min());
    CHECK(spatial::cellCoord(std::numeric_limits<float>::quiet_NaN(), 1.0f) == 0);
  }

  TEST_CASE("Arx level grid reserves one overflow ring around the native world") {
    using Grid = spatial::ArxLevelGrid;
    CHECK(Grid::cellCoord(-1.0f) == 0);
    CHECK(Grid::cellCoord(0.0f) == 1);
    CHECK(Grid::cellCoord(99.999f) == 1);
    CHECK(Grid::cellCoord(100.0f) == 2);
    CHECK(Grid::cellCoord(15999.0f) == 160);
    CHECK(Grid::cellCoord(16000.0f) == 160);
    CHECK(Grid::cellCoord(16000.1f) == 161);
    CHECK_FALSE(Grid::cellCoord(std::numeric_limits<float>::infinity()).has_value());

    CHECK(Grid::cellKey(-1.0f, -1.0f) == Grid::cellKey(-1.0e9f, -1.0e9f));
    CHECK(Grid::cellKey(-1.0f, 50.0f) == Grid::cellKey(-1.0e9f, 50.0f));
    CHECK(Grid::cellKey(-1.0f, 50.0f) != Grid::cellKey(-1.0f, 150.0f));
    CHECK(Grid::cellKey(16001.0f, 50.0f) == Grid::cellKey(1.0e9f, 50.0f));
  }

  TEST_CASE("Finds candidates by XZ cell") {
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{0.0f, 0.0f, 0.0f}, ArxVector3{1.0f, 0.0f, 0.0f}, ArxVector3{0.0f, 0.0f, 1.0f}}),
        makeIndexed({ArxVector3{20.0f, 0.0f, 20.0f}, ArxVector3{21.0f, 0.0f, 20.0f}, ArxVector3{20.0f, 0.0f, 21.0f}}),
    };
    geometry::TriangleIndex index(triangles);
    std::vector<std::uint32_t> candidates;

    index.findCandidatesForXz(candidates, 0.5f, 0.5f);
    CHECK(candidates == std::vector<std::uint32_t>{0});
    index.findCandidatesForXz(candidates, 20.5f, 20.5f);
    CHECK(candidates == std::vector<std::uint32_t>{1});
    index.findCandidatesForXz(candidates, 50.0f, 50.0f);
    CHECK(candidates.empty());
  }

  TEST_CASE("Filters and deduplicates AABB candidates") {
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{0.0f, 0.0f, 0.0f}, ArxVector3{20.0f, 0.0f, 0.0f}, ArxVector3{0.0f, 0.0f, 20.0f}}),
        makeIndexed({ArxVector3{8.0f, 0.0f, 8.0f}, ArxVector3{9.0f, 0.0f, 8.0f}, ArxVector3{8.0f, 0.0f, 9.0f}}),
    };
    geometry::TriangleIndex index(triangles);
    std::vector<std::uint32_t> candidates;

    index.findCandidatesForAabb(candidates, {{5.0f, 0.0f, 5.0f}, {15.0f, 0.0f, 15.0f}});
    CHECK(candidates == std::vector<std::uint32_t>{0, 1});
    index.findCandidatesForAabb(candidates, {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}});
    CHECK(candidates == std::vector<std::uint32_t>{0});
  }

  TEST_CASE("Finds segment candidates through segment bounds") {
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{0.0f, 0.0f, 0.0f}, ArxVector3{1.0f, 0.0f, 0.0f}, ArxVector3{0.0f, 0.0f, 1.0f}}),
        makeIndexed({ArxVector3{20.0f, 0.0f, 20.0f}, ArxVector3{21.0f, 0.0f, 20.0f}, ArxVector3{20.0f, 0.0f, 21.0f}}),
    };
    geometry::TriangleIndex index(triangles);
    std::vector<std::uint32_t> candidates;

    index.findCandidatesForSegment(candidates, {20.5f, -1.0f, 20.5f}, {20.5f, 1.0f, 20.5f});
    CHECK(candidates == std::vector<std::uint32_t>{1});
  }

  TEST_CASE("Indexes triangles spanning excessive cell counts") {
    constexpr float kExtent = 1.0e9f;
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{0.0f, 0.0f, 0.0f}, ArxVector3{kExtent, 0.0f, 0.0f}, ArxVector3{0.0f, 0.0f, kExtent}}),
    };
    geometry::TriangleIndex index(triangles);
    std::vector<std::uint32_t> candidates;

    index.findCandidatesForXz(candidates, 1.0f, 1.0f);
    CHECK(candidates == std::vector<std::uint32_t>{0});
    index.findCandidatesForAabb(candidates, {{0.5f, -1.0f, 0.5f}, {1.5f, 1.0f, 1.5f}});
    CHECK(candidates == std::vector<std::uint32_t>{0});
    index.findCandidatesForXz(candidates, -1.0f, -1.0f);
    CHECK(candidates.empty());
  }

  TEST_CASE("Indexes finite triangles beyond the grid coordinate range") {
    const float max = std::numeric_limits<float>::max();
    const float low = std::nextafter(max, 0.0f);
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{low, 0.0f, low}, ArxVector3{max, 0.0f, low}, ArxVector3{low, 0.0f, max}}),
    };
    geometry::TriangleIndex index(triangles);
    std::vector<std::uint32_t> candidates;

    index.findCandidatesForXz(candidates, low, low);
    CHECK(candidates == std::vector<std::uint32_t>{0});
    index.findCandidatesForAabb(candidates, {{low, -1.0f, low}, {max, 1.0f, max}});
    CHECK(candidates == std::vector<std::uint32_t>{0});
  }

  TEST_CASE("Scans triangle bounds for excessive queries") {
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{0.0f, 0.0f, 0.0f}, ArxVector3{1.0f, 0.0f, 0.0f}, ArxVector3{0.0f, 0.0f, 1.0f}}),
    };
    geometry::TriangleIndex index(triangles);
    const float max = std::numeric_limits<float>::max();
    std::vector<std::uint32_t> candidates;

    index.findCandidatesForAabb(candidates, {{-max, -1.0f, -max}, {max, 1.0f, max}});
    CHECK(candidates == std::vector<std::uint32_t>{0});
    index.findCandidatesForSegment(candidates, {-max, 0.0f, -max}, {max, 0.0f, max});
    CHECK(candidates == std::vector<std::uint32_t>{0});
  }

  TEST_CASE("Nonfinite queries yield no candidates") {
    std::vector<geometry::IndexedTriangle> triangles = {
        makeIndexed({ArxVector3{0.0f, 0.0f, 0.0f}, ArxVector3{1.0f, 0.0f, 0.0f}, ArxVector3{0.0f, 0.0f, 1.0f}}),
    };
    geometry::TriangleIndex index(triangles);
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<std::uint32_t> candidates;
    index.findCandidatesForXz(candidates, nan, 0.5f);
    CHECK(candidates.empty());
    index.findCandidatesForAabb(candidates, {{nan, 0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}});
    CHECK(candidates.empty());
  }
}
