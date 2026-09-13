// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <utility>
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

geometry::SurfaceSupportIndex makeSupportIndex(std::initializer_list<geometry::SurfaceSupportTriangle> triangles) {
  geometry::SurfaceSupportIndexBuilder builder(triangles.size());
  for (const geometry::SurfaceSupportTriangle& triangle : triangles)
    builder.addTriangle(triangle.face, triangle.vertices);
  return std::move(builder).build();
}

geometry::SurfaceSupportHit supportHit(FaceIndex face, float y) { return {face, {0.0f, y, 0.0f}, {0.0f, 1.0f, 0.0f}}; }

}  // namespace

TEST_SUITE("geometry::surface_support") {
  TEST_CASE("Empty support index has no bounds or hits") {
    geometry::SurfaceSupportIndex index = makeSupportIndex({});
    std::vector<geometry::SurfaceSupportHit> hits;

    CHECK(index.empty());
    CHECK_FALSE(index.hasBounds());
    CHECK(index.size() == 0);
    index.findHitsAt(hits, 0.25f, 0.25f);
    CHECK(hits.empty());
    CHECK_FALSE(index.closestDownwardHit(0.25f, 0.25f, 0.0f).has_value());
    CHECK_FALSE(index.closestHit(0.25f, 0.25f, 0.0f, 1.0f).has_value());
  }

  TEST_CASE("Reports sorted hits without merging face ids") {
    geometry::SurfaceSupportIndex index = makeSupportIndex({
        {7, supportTriangle(20.0f)},
        {8, supportTriangle(10.0f)},
    });

    REQUIRE_FALSE(index.empty());
    REQUIRE(index.hasBounds());
    CHECK(index.bounds().min.y == doctest::Approx(10.0f));
    CHECK(index.bounds().max.y == doctest::Approx(20.0f));

    std::vector<geometry::SurfaceSupportHit> hits;
    index.findHitsAt(hits, 0.25f, 0.25f);
    REQUIRE(hits.size() == 2);
    CHECK(hits[0].face == 8);
    CHECK(hits[0].position.y == doctest::Approx(10.0f));
    CHECK(hits[1].face == 7);
    CHECK(hits[1].position.y == doctest::Approx(20.0f));
  }

  TEST_CASE("Merges against retained representatives without chaining proximity") {
    std::vector<geometry::SurfaceSupportHit> hits = {
        supportHit(2, 1.5f),
        supportHit(1, 0.75f),
        supportHit(0, 0.0f),
    };

    geometry::mergeSurfaceSupportHits(hits, 1.0f);

    REQUIRE(hits.size() == 2);
    CHECK(hits[0].face == 0);
    CHECK(hits[1].face == 2);
  }

  TEST_CASE("Merge distance is inclusive and equal heights use face order") {
    std::vector<geometry::SurfaceSupportHit> hits = {
        supportHit(9, 0.0f),
        supportHit(3, 0.0f),
        supportHit(7, 1.0f),
        supportHit(8, 2.0f),
    };

    geometry::mergeSurfaceSupportHits(hits, 1.0f);

    REQUIRE(hits.size() == 2);
    CHECK(hits[0].face == 3);
    CHECK(hits[1].face == 8);
  }

  TEST_CASE("Subsets use indices local to their source") {
    geometry::SurfaceSupportIndex index = makeSupportIndex({
        {7, supportTriangle(30.0f)},
        {8, supportTriangle(20.0f)},
        {9, supportTriangle(10.0f)},
    });
    const std::array<std::uint32_t, 2> first_indices = {0, 2};
    geometry::SurfaceSupportIndex first = index.subset(first_indices);
    const std::array<std::uint32_t, 1> nested_indices = {1};
    geometry::SurfaceSupportIndex nested = first.subset(nested_indices);

    REQUIRE(nested.size() == 1);
    CHECK(nested.triangle(0).face == 9);
    CHECK(nested.bounds().min.y == doctest::Approx(10.0f));
    CHECK(nested.bounds().max.y == doctest::Approx(10.0f));

    std::vector<geometry::SurfaceSupportHit> hits;
    nested.findHitsAt(hits, 0.25f, 0.25f);
    REQUIRE(hits.size() == 1);
    CHECK(hits[0].face == 9);
  }

  TEST_CASE("Finds closest support hits") {
    geometry::SurfaceSupportIndex index = makeSupportIndex({
        {7, supportTriangle(20.0f)},
        {8, supportTriangle(10.0f)},
    });

    std::optional<geometry::SurfaceSupportHit> downward = index.closestDownwardHit(0.25f, 0.25f, 5.0f);
    REQUIRE(downward.has_value());
    CHECK(downward->face == 8);
    CHECK(downward->position.y == doctest::Approx(10.0f));

    std::vector<geometry::SurfaceSupportHit> downward_hits;
    index.findDownwardHitsAt(downward_hits, 0.25f, 0.25f, 5.0f);
    REQUIRE(downward_hits.size() == 2);
    CHECK(downward_hits[0].face == 8);
    CHECK(downward_hits[1].face == 7);

    downward = index.closestDownwardHit(0.25f, 0.25f, 15.0f);
    REQUIRE(downward.has_value());
    CHECK(downward->face == 7);
    CHECK(downward->position.y == doctest::Approx(20.0f));

    index.findDownwardHitsAt(downward_hits, 0.25f, 0.25f, 15.0f);
    REQUIRE(downward_hits.size() == 1);
    CHECK(downward_hits[0].face == 7);

    CHECK_FALSE(index.closestDownwardHit(0.25f, 0.25f, 25.0f).has_value());
    index.findDownwardHitsAt(downward_hits, 0.25f, 0.25f, 25.0f);
    CHECK(downward_hits.empty());

    std::optional<geometry::SurfaceSupportHit> closest = index.closestHit(0.25f, 0.25f, 18.0f, 5.0f);
    REQUIRE(closest.has_value());
    CHECK(closest->face == 7);
    CHECK_FALSE(index.closestHit(0.25f, 0.25f, 18.0f, 1.0f).has_value());
  }

  TEST_CASE("Visits hits without scratch allocation and rejects empty query cells") {
    geometry::SurfaceSupportIndex index = makeSupportIndex({{7, supportTriangle(20.0f)}});
    struct Context {
      std::uint32_t count = 0;
      FaceIndex face = kInvalidFaceIndex;
    } context;
    index.visitHitsAt(
        0.25f,
        0.25f,
        [](const geometry::SurfaceSupportHit& hit, void* raw) {
          auto& context = *static_cast<Context*>(raw);
          ++context.count;
          context.face = hit.face;
        },
        &context);
    CHECK(context.count == 1);
    CHECK(context.face == 7);
    CHECK(index.mayHaveHitsInAabb({{-1.0f, 0.0f, -1.0f}, {2.0f, 30.0f, 2.0f}}));
    CHECK_FALSE(index.mayHaveHitsInAabb({{200.0f, 0.0f, 200.0f}, {300.0f, 30.0f, 300.0f}}));
  }

  TEST_CASE("Builds support index from all or selected face ids") {
    GeometryData geometry = makeGeometry();

    geometry::SurfaceSupportIndex all = geometry::buildSurfaceSupportIndex(geometry);
    CHECK(all.size() == 2);

    std::vector<FaceIndex> face_scratch;
    geometry::SurfaceSupportIndex default_predicate =
        geometry::buildSurfaceSupportIndex(geometry, geometry::FacePredicate{}, face_scratch);
    CHECK(default_predicate.size() == 2);

    std::array<FaceIndex, 1> selected = {1};
    geometry::SurfaceSupportIndex selected_index = geometry::buildSurfaceSupportIndex(geometry, selected);
    REQUIRE(selected_index.size() == 1);
    CHECK(selected_index.triangle(0).face == 1);

    std::array<FaceIndex, 1> invalid = {99};
    geometry::SurfaceSupportIndex invalid_index = geometry::buildSurfaceSupportIndex(geometry, invalid);
    CHECK(invalid_index.empty());
  }
}
