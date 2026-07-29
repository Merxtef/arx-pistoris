// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"

#include "modules/geometry.h"

#include <array>

using namespace pistoris;

namespace {

Face makeFace() {
  Face face;
  face.corners[0].vertex = 0;
  face.corners[1].vertex = 1;
  face.corners[2].vertex = 2;
  return face;
}

GeometryData makeGeometry() {
  GeometryData geometry;
  geometry.vertices = {
      {{0.0f, 0.0f, 0.0f}},
      {{1.0f, 0.0f, 0.0f}},
      {{0.0f, 0.0f, 1.0f}},
  };
  geometry.faces.push_back(makeFace());
  return geometry;
}

}  // namespace

TEST_SUITE("geometry::triangle") {
  TEST_CASE("Reads face positions") {
    GeometryData geometry = makeGeometry();

    std::array<ArxVector3, 3> positions = geometry::facePositions(geometry, geometry.faces[0]);

    CHECK(positions[0].x == doctest::Approx(0.0f));
    CHECK(positions[1].x == doctest::Approx(1.0f));
    CHECK(positions[2].z == doctest::Approx(1.0f));
  }

  TEST_CASE("Computes normals and uses fallback for degenerate faces") {
    GeometryData geometry = makeGeometry();

    ArxVector3 normal = geometry::faceNormalOr(geometry, geometry.faces[0], {0.0f, 1.0f, 0.0f});
    CHECK(normal.x == doctest::Approx(0.0f));
    CHECK(normal.y == doctest::Approx(-1.0f));
    CHECK(normal.z == doctest::Approx(0.0f));

    geometry.vertices[2].position = {2.0f, 0.0f, 0.0f};
    ArxVector3 fallback = geometry::faceNormalOr(geometry, geometry.faces[0], {0.0f, 1.0f, 0.0f});
    CHECK(fallback.x == doctest::Approx(0.0f));
    CHECK(fallback.y == doctest::Approx(1.0f));
    CHECK(fallback.z == doctest::Approx(0.0f));
  }

  TEST_CASE("Computes triangle bounds") {
    std::array<ArxVector3, 3> vertices = {
        ArxVector3{2.0f, -1.0f, 3.0f},
        ArxVector3{-4.0f, 5.0f, 0.0f},
        ArxVector3{1.0f, 6.0f, -2.0f},
    };

    ArxAabb bounds = geometry::triangleBounds(vertices);
    CHECK(bounds.min.x == doctest::Approx(-4.0f));
    CHECK(bounds.min.y == doctest::Approx(-1.0f));
    CHECK(bounds.min.z == doctest::Approx(-2.0f));
    CHECK(bounds.max.x == doctest::Approx(2.0f));
    CHECK(bounds.max.y == doctest::Approx(6.0f));
    CHECK(bounds.max.z == doctest::Approx(3.0f));
  }

  TEST_CASE("Detects degenerate triangles") {
    CHECK_FALSE(geometry::degenerateTriangle({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
    CHECK(geometry::degenerateTriangle({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}));
  }
}
