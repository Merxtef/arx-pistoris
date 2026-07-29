// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"

#include <array>
#include <optional>
#include <vector>

using namespace pistoris;

namespace {

std::array<ArxVector3, 3> supportTriangle(float y) {
  return {
      ArxVector3{0.0f, y, 0.0f},
      ArxVector3{1.0f, y, 0.0f},
      ArxVector3{0.0f, y, 1.0f},
  };
}

Face makeFace(VertexIndex a, VertexIndex b, VertexIndex c) {
  Face face;
  face.corners[0].vertex = a;
  face.corners[1].vertex = b;
  face.corners[2].vertex = c;
  return face;
}

GeometryData makeGeometry() {
  GeometryData geometry;
  geometry.vertices = {
      {{0.0f, 0.0f, 0.0f}},
      {{1.0f, 0.0f, 0.0f}},
      {{0.0f, 0.0f, 1.0f}},
  };
  geometry.faces.push_back(makeFace(0, 1, 2));
  geometry.faces.push_back(makeFace(0, 2, 1));
  return geometry;
}

}  // namespace

TEST_SUITE("geometry::surface_support") {
  TEST_CASE("Empty support index has no bounds or hits") {
    geometry::SurfaceSupportIndex index({});

    CHECK(index.empty());
    CHECK_FALSE(index.hasBounds());
    CHECK(index.triangles().empty());
    CHECK(index.hitsAt(0.25f, 0.25f).empty());
    CHECK_FALSE(index.closestDownwardHit(0.25f, 0.25f, 0.0f).has_value());
    CHECK_FALSE(index.closestHit(0.25f, 0.25f, 0.0f, 1.0f).has_value());
  }

  TEST_CASE("Reports sorted hits without merging face ids") {
    geometry::SurfaceSupportIndex index({
        {7, supportTriangle(20.0f)},
        {8, supportTriangle(10.0f)},
    });

    REQUIRE_FALSE(index.empty());
    REQUIRE(index.hasBounds());
    CHECK(index.bounds().min.y == doctest::Approx(10.0f));
    CHECK(index.bounds().max.y == doctest::Approx(20.0f));

    std::vector<geometry::SurfaceSupportHit> hits = index.hitsAt(0.25f, 0.25f);
    REQUIRE(hits.size() == 2);
    CHECK(hits[0].face == 8);
    CHECK(hits[0].position.y == doctest::Approx(10.0f));
    CHECK(hits[1].face == 7);
    CHECK(hits[1].position.y == doctest::Approx(20.0f));
  }

  TEST_CASE("Finds closest support hits") {
    geometry::SurfaceSupportIndex index({
        {7, supportTriangle(20.0f)},
        {8, supportTriangle(10.0f)},
    });

    std::optional<geometry::SurfaceSupportHit> downward = index.closestDownwardHit(0.25f, 0.25f, 5.0f);
    REQUIRE(downward.has_value());
    CHECK(downward->face == 8);
    CHECK(downward->position.y == doctest::Approx(10.0f));

    std::vector<geometry::SurfaceSupportHit> downward_hits = index.downwardHitsAt(0.25f, 0.25f, 5.0f);
    REQUIRE(downward_hits.size() == 2);
    CHECK(downward_hits[0].face == 8);
    CHECK(downward_hits[1].face == 7);

    downward = index.closestDownwardHit(0.25f, 0.25f, 15.0f);
    REQUIRE(downward.has_value());
    CHECK(downward->face == 7);
    CHECK(downward->position.y == doctest::Approx(20.0f));

    downward_hits = index.downwardHitsAt(0.25f, 0.25f, 15.0f);
    REQUIRE(downward_hits.size() == 1);
    CHECK(downward_hits[0].face == 7);

    CHECK_FALSE(index.closestDownwardHit(0.25f, 0.25f, 25.0f).has_value());
    CHECK(index.downwardHitsAt(0.25f, 0.25f, 25.0f).empty());

    std::optional<geometry::SurfaceSupportHit> closest = index.closestHit(0.25f, 0.25f, 18.0f, 5.0f);
    REQUIRE(closest.has_value());
    CHECK(closest->face == 7);
    CHECK_FALSE(index.closestHit(0.25f, 0.25f, 18.0f, 1.0f).has_value());
  }

  TEST_CASE("Builds support index from all or selected face ids") {
    GeometryData geometry = makeGeometry();

    geometry::SurfaceSupportIndex all = geometry::buildSurfaceSupportIndex(geometry);
    CHECK(all.triangles().size() == 2);

    geometry::SurfaceSupportIndex default_predicate =
        geometry::buildSurfaceSupportIndex(geometry, geometry::FacePredicate{});
    CHECK(default_predicate.triangles().size() == 2);

    std::array<FaceIndex, 1> selected = {1};
    geometry::SurfaceSupportIndex selected_index = geometry::buildSurfaceSupportIndex(geometry, selected);
    std::vector<geometry::SurfaceSupportTriangle> selected_triangles = selected_index.triangles();
    REQUIRE(selected_triangles.size() == 1);
    CHECK(selected_triangles[0].face == 1);

    std::array<FaceIndex, 1> invalid = {99};
    geometry::SurfaceSupportIndex invalid_index = geometry::buildSurfaceSupportIndex(geometry, invalid);
    CHECK(invalid_index.empty());
  }
}
