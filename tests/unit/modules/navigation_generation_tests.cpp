// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/navigation.h"

#include <cstdint>
#include <utility>
#include <vector>

using namespace pistoris;

namespace {

Face makeFace(GeometryData&, VertexIndex a, VertexIndex b, VertexIndex c,
              const ArxVector3& normal = {0.0f, -1.0f, 0.0f}, FaceType flags = 0) {
  Face face;
  face.flags = flags;
  face.corners[0].vertex = a;
  face.corners[1].vertex = b;
  face.corners[2].vertex = c;
  for (Corner& corner : face.corners) corner.normal = normal;
  return face;
}

void addFloor(GeometryData& geometry, float min_x, float max_x, float y, float min_z, float max_z, FaceType flags = 0) {
  const VertexIndex base = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back({{min_x, y, min_z}});
  geometry.vertices.push_back({{max_x, y, min_z}});
  geometry.vertices.push_back({{max_x, y, max_z}});
  geometry.vertices.push_back({{min_x, y, max_z}});
  geometry.faces.push_back(makeFace(geometry, base + 0, base + 1, base + 2, {0.0f, -1.0f, 0.0f}, flags));
  geometry.faces.push_back(makeFace(geometry, base + 0, base + 2, base + 3, {0.0f, -1.0f, 0.0f}, flags));
}

GeometryData makeFloorGeometry(float size = 200.0f) {
  GeometryData geometry;
  addFloor(geometry, 0.0f, size, 0.0f, 0.0f, size);
  return geometry;
}

NavSurface makeSurface() {
  return {
      {{{0.0f, 0.0f, 0.0f}}, {{200.0f, 0.0f, 0.0f}}, {{200.0f, 0.0f, 200.0f}}, {{0.0f, 0.0f, 200.0f}}},
      {{{{0, 1, 2}}}, {{{0, 2, 3}}}},
  };
}

}  // namespace

TEST_SUITE("navigation::generation") {
  TEST_CASE("Generates a navigation surface from geometry") {
    GeometryData geometry = makeFloorGeometry(200.0f);
    NavSurface surface;
    navigation::NavSurfaceGenerationDiagnostics diagnostics;

    CHECK(navigation::generateSurface(surface, geometry, {.radius = 50.0f, .height = -165.0f}, &diagnostics) ==
          navigation::Error::kNone);

    CHECK(!surface.vertices.empty());
    CHECK(!surface.triangles.empty());
    CHECK(!diagnostics.base.empty());
  }

  TEST_CASE("Generates anchors from an existing surface") {
    NavigationData navigation;
    navigation.surface = makeSurface();
    GeometryData geometry = makeFloorGeometry();
    GeometryDerived derived;
    REQUIRE(geometry::validate(geometry, 0, &derived) == geometry::Error::kNone);
    std::vector<Anchor> anchors;

    CHECK(navigation::generateAnchors(
              anchors, geometry, *navigation.surface, derived.referenced_bounds, {.sample_spacing = 100.0f}) ==
          navigation::Error::kNone);

    CHECK(!anchors.empty());
  }

  TEST_CASE("Generates anchor connections") {
    NavigationData navigation;
    navigation.anchors.push_back({{50.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
    navigation.anchors.push_back({{150.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}});
    GeometryData geometry = makeFloorGeometry(200.0f);
    std::vector<AnchorConnection> connections;

    CHECK(navigation::generateAnchorConnections(connections, geometry, navigation.anchors, {.max_distance = 150.0f}) ==
          navigation::Error::kNone);

    REQUIRE(connections.size() == 1);
    CHECK(connections[0].first == 0);
    CHECK(connections[0].second == 1);
  }

  TEST_CASE("Anchor connection distance is measured in XZ") {
    GeometryData geometry;
    geometry.vertices = {
        {{0.0f, 0.0f, 0.0f}},
        {{200.0f, -200.0f, 0.0f}},
        {{200.0f, -200.0f, 100.0f}},
        {{0.0f, 0.0f, 100.0f}},
    };
    geometry.faces.push_back(makeFace(geometry, 0, 1, 2));
    geometry.faces.push_back(makeFace(geometry, 0, 2, 3));
    std::vector<Anchor> anchors = {
        {{50.0f, -50.0f, 50.0f}, 10.0f, -80.0f, 0, {}},
        {{150.0f, -150.0f, 50.0f}, 10.0f, -80.0f, 0, {}},
    };
    std::vector<AnchorConnection> connections;

    REQUIRE(navigation::generateAnchorConnections(
                connections,
                geometry,
                anchors,
                {.max_distance = 110.0f, .max_step_distance = 40.0f, .max_step_up = 55.0f}) ==
            navigation::Error::kNone);
    REQUIRE(connections.size() == 1);
  }

  TEST_CASE("Failures do not overwrite outputs") {
    NavSurface surface = makeSurface();
    GeometryData geometry = makeFloorGeometry();
    navigation::NavSurfaceGenerationOptions bad_surface_options;
    bad_surface_options.radius = 0.0f;
    CHECK(navigation::generateSurface(surface, geometry, bad_surface_options) == navigation::Error::kInvalidOptions);
    CHECK(surface.vertices.size() == 4);

    std::vector<Anchor> anchors = {{{50.0f, 0.0f, 50.0f}, 50.0f, -80.0f, 0, {}}};
    GeometryDerived derived;
    REQUIRE(geometry::validate(geometry, 0, &derived) == geometry::Error::kNone);
    navigation::AnchorGenerationOptions bad_anchor_options;
    bad_anchor_options.radius = 0.0f;
    CHECK(navigation::generateAnchors(anchors, geometry, surface, derived.referenced_bounds, bad_anchor_options) ==
          navigation::Error::kInvalidOptions);
    CHECK(anchors.size() == 1);
  }

  TEST_CASE("Generates a navigation surface directly from filtered floor polygons") {
    GeometryData geometry;
    addFloor(geometry, 0.0f, 100.0f, 0.0f, 0.0f, 100.0f);
    addFloor(geometry, 200.0f, 300.0f, 0.0f, 0.0f, 100.0f, kFaceBitNopath);
    const VertexIndex wall_base = static_cast<VertexIndex>(geometry.vertices.size());
    geometry.vertices.push_back({{400.0f, 0.0f, 0.0f}});
    geometry.vertices.push_back({{400.0f, -100.0f, 0.0f}});
    geometry.vertices.push_back({{400.0f, 0.0f, 100.0f}});
    geometry.faces.push_back(makeFace(geometry, wall_base, wall_base + 1, wall_base + 2, {1.0f, 0.0f, 0.0f}));

    navigation::NavSurfaceSourceOptions options;
    options.clearance = 7.0f;
    options.support_ignore_flags = 0;
    NavSurface surface;
    navigation::NavSurfaceGenerationDiagnostics diagnostics;
    REQUIRE(navigation::generateSurfaceFromFloor(surface, geometry, options, &diagnostics) == navigation::Error::kNone);

    CHECK(surface.vertices.size() == 4);
    CHECK(surface.triangles.size() == 2);
    CHECK(diagnostics.support.size() == 2);
    CHECK(diagnostics.base.size() == 2);
    for (const Vertex& vertex : surface.vertices) {
      CHECK(vertex.position.x <= 100.0f);
      CHECK(vertex.position.y == doctest::Approx(-7.0f));
    }
  }

  TEST_CASE("Floor polygon generation supports large finite coordinate offsets") {
    constexpr float kOrigin = 1.0e15f;
    constexpr float kExtent = 1.0e9f;
    GeometryData geometry;
    addFloor(geometry, kOrigin, kOrigin + kExtent, 0.0f, kOrigin, kOrigin + kExtent);

    NavSurface surface;
    REQUIRE(navigation::generateSurfaceFromFloor(surface, geometry, {}) == navigation::Error::kNone);
    CHECK(surface.vertices.size() == 4);
    CHECK(surface.triangles.size() == 2);
  }

  TEST_CASE("Navigation surface components require a shared edge") {
    NavSurface surface{
        {{{0.0f, 0.0f, 0.0f}},
         {{10.0f, 0.0f, 0.0f}},
         {{0.0f, 0.0f, 10.0f}},
         {{10.0f, 0.0f, 10.0f}},
         {{0.0f, 0.0f, 11.0f}},
         {{1.0f, 0.0f, 10.0f}}},
        {{{{0, 1, 2}}}, {{{1, 3, 2}}}, {{{2, 4, 5}}}},
    };

    CHECK(navigation::surfaceComponentCount(surface) == 2);
    navigation::NavSurfacePruneDiagnostics diagnostics;
    REQUIRE(navigation::pruneSurfaceComponents(surface,
                                               {.min_component_area_ratio = 0.5f, .min_component_area = 0.0},
                                               &diagnostics) == navigation::Error::kNone);

    CHECK(surface.vertices.size() == 4);
    CHECK(surface.triangles.size() == 2);
    CHECK(diagnostics.pruned.size() == 1);
  }

  TEST_CASE("Navigation surface absolute area threshold keeps equality and preserves no-op storage") {
    NavSurface surface{
        {{{0.0f, 0.0f, 0.0f}},
         {{10.0f, 0.0f, 0.0f}},
         {{0.0f, 0.0f, 10.0f}},
         {{20.0f, 0.0f, 0.0f}},
         {{25.0f, 0.0f, 0.0f}},
         {{20.0f, 0.0f, 2.0f}}},
        {{{{0, 1, 2}}}, {{{3, 4, 5}}}},
    };
    const Vertex* vertices = surface.vertices.data();
    const NavSurfaceTriangle* triangles = surface.triangles.data();
    navigation::NavSurfacePruneDiagnostics diagnostics;

    REQUIRE(navigation::pruneSurfaceComponents(surface,
                                               {.min_component_area_ratio = 0.0f, .min_component_area = 5.0},
                                               &diagnostics) == navigation::Error::kNone);
    CHECK(surface.vertices.data() == vertices);
    CHECK(surface.triangles.data() == triangles);
    CHECK(diagnostics.pruned.empty());

    REQUIRE(navigation::pruneSurfaceComponents(
                surface, {.min_component_area_ratio = 0.0f, .min_component_area = 5.01}) == navigation::Error::kNone);
    CHECK(surface.vertices.size() == 3);
    CHECK(surface.triangles.size() == 1);
  }

  TEST_CASE("Navigation surface pruning handles large finite triangle areas") {
    constexpr float kEdge = 1.0e20f;
    NavSurface surface{
        {{{0.0f, 0.0f, 0.0f}}, {{kEdge, 0.0f, 0.0f}}, {{0.0f, 0.0f, kEdge}}},
        {{{{0, 1, 2}}}},
    };
    const Vertex* vertices = surface.vertices.data();
    const NavSurfaceTriangle* triangles = surface.triangles.data();

    REQUIRE(navigation::pruneSurfaceComponents(
                surface, {.min_component_area_ratio = 0.0f, .min_component_area = 0.0}) == navigation::Error::kNone);
    CHECK(surface.vertices.data() == vertices);
    CHECK(surface.triangles.data() == triangles);
  }

  TEST_CASE("Navigation surface pruning failure preserves the surface") {
    NavSurface surface = makeSurface();
    const Vertex* vertices = surface.vertices.data();
    const NavSurfaceTriangle* triangles = surface.triangles.data();
    navigation::NavSurfacePruneDiagnostics diagnostics;

    CHECK(navigation::pruneSurfaceComponents(surface,
                                             {.min_component_area_ratio = 0.0f, .min_component_area = 50000.0},
                                             &diagnostics) == navigation::Error::kEmptyResult);
    CHECK(surface.vertices.data() == vertices);
    CHECK(surface.triangles.data() == triangles);
    CHECK(surface.vertices.size() == 4);
    CHECK(surface.triangles.size() == 2);
    CHECK(diagnostics.pruned.size() == 2);
  }

  TEST_CASE("Anchor component pruning remaps retained graph components") {
    std::vector<Anchor> anchors;
    anchors.reserve(6);
    for (std::uint32_t i = 0; i < 6; ++i)
      anchors.push_back({{static_cast<float>(i * 10), 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}});
    std::vector<AnchorConnection> connections = {{0, 1}, {3, 4}, {4, 5}};
    navigation::AnchorComponentPruneDiagnostics diagnostics;

    REQUIRE(navigation::pruneAnchorComponents(anchors,
                                              connections,
                                              {.min_component_anchor_ratio = 0.75f, .min_component_anchor_count = 1},
                                              &diagnostics) == navigation::Error::kNone);

    REQUIRE(anchors.size() == 3);
    CHECK(anchors[0].position.x == doctest::Approx(30.0f));
    REQUIRE(connections.size() == 2);
    CHECK(connections[0].first == 0);
    CHECK(connections[0].second == 1);
    CHECK(connections[1].first == 1);
    CHECK(connections[1].second == 2);
    CHECK(diagnostics.pruned.size() == 3);
  }

  TEST_CASE("Component pruning plans do not mutate source data before publication") {
    NavSurface surface{{{{0.0f, 0.0f, 0.0f}},
                        {{10.0f, 0.0f, 0.0f}},
                        {{0.0f, 0.0f, 10.0f}},
                        {{20.0f, 0.0f, 0.0f}},
                        {{21.0f, 0.0f, 0.0f}},
                        {{20.0f, 0.0f, 1.0f}}},
                       {{{{0, 1, 2}}}, {{{3, 4, 5}}}}};
    const Vertex* surface_vertices = surface.vertices.data();
    navigation::NavSurfaceComponentPrunePlan surface_plan;
    REQUIRE(navigation::planSurfaceComponentPrune(
                surface_plan, surface, {.min_component_area_ratio = 0.5f, .min_component_area = 0.0}) ==
            navigation::Error::kNone);
    CHECK(surface.vertices.data() == surface_vertices);
    CHECK(surface.triangles.size() == 2);
    REQUIRE(surface_plan.replacement.has_value());
    navigation::applySurfaceComponentPrune(surface, std::move(surface_plan));
    CHECK(surface.triangles.size() == 1);

    std::vector<Anchor> anchors = {
        {{0.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
        {{10.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
        {{20.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
    };
    std::vector<AnchorConnection> connections = {{0, 1}};
    const Anchor* anchor_data = anchors.data();
    navigation::AnchorComponentPrunePlan anchor_plan;
    REQUIRE(
        navigation::planAnchorComponentPrune(
            anchor_plan, anchors, connections, {.min_component_anchor_ratio = 0.0f, .min_component_anchor_count = 2}) ==
        navigation::Error::kNone);
    CHECK(anchors.data() == anchor_data);
    CHECK(anchors.size() == 3);
    navigation::applyAnchorComponentPrune(anchors, connections, std::move(anchor_plan));
    CHECK(anchors.size() == 2);
    CHECK(connections.size() == 1);
  }

  TEST_CASE("Anchor component absolute threshold keeps equality and treats isolates as components") {
    std::vector<Anchor> anchors = {
        {{0.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
        {{10.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
        {{20.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
    };
    std::vector<AnchorConnection> connections = {{0, 1}};

    REQUIRE(navigation::pruneAnchorComponents(
                anchors, connections, {.min_component_anchor_ratio = 0.0f, .min_component_anchor_count = 2}) ==
            navigation::Error::kNone);
    CHECK(anchors.size() == 2);
    CHECK(connections.size() == 1);
  }

  TEST_CASE("Anchor component pruning preserves no-op storage and may remove every component") {
    std::vector<Anchor> anchors = {
        {{0.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
        {{10.0f, 0.0f, 0.0f}, 5.0f, -20.0f, 0, {}},
    };
    std::vector<AnchorConnection> connections;
    const Anchor* anchor_data = anchors.data();

    REQUIRE(navigation::pruneAnchorComponents(
                anchors, connections, {.min_component_anchor_ratio = 0.0f, .min_component_anchor_count = 1}) ==
            navigation::Error::kNone);
    CHECK(anchors.data() == anchor_data);

    REQUIRE(navigation::pruneAnchorComponents(
                anchors, connections, {.min_component_anchor_ratio = 0.0f, .min_component_anchor_count = 2}) ==
            navigation::Error::kNone);
    CHECK(anchors.empty());
    CHECK(connections.empty());
  }
}
