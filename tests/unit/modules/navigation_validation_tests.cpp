// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "modules/geometry.h"
#include "modules/navigation.h"

#include <limits>
#include <optional>
#include <string>

using namespace pistoris;

namespace {

NavSurface makeSurface() {
  return {
      {{{0.0f, 0.0f, 0.0f}}, {{100.0f, 0.0f, 0.0f}}, {{100.0f, 0.0f, 100.0f}}, {{0.0f, 0.0f, 100.0f}}},
      {{{{0, 1, 2}}}, {{{0, 2, 3}}}},
  };
}

NavigationData makeNavigation() {
  NavigationData navigation;
  navigation.surface = makeSurface();
  navigation.anchors.push_back({{50.0f, 0.0f, 50.0f}, 30.0f, -80.0f, 0, {}});
  navigation.anchors.push_back({{150.0f, 0.0f, 50.0f}, 30.0f, -80.0f, 0, {}});
  navigation.connections.push_back({0, 1});
  return navigation;
}

}  // namespace

TEST_SUITE("navigation::validation") {
  TEST_CASE("Accepts valid navigation data") {
    NavigationData navigation = makeNavigation();

    CHECK(navigation::validateSurface(navigation.surface) == navigation::Error::kNone);
    CHECK(navigation::validateAnchor(navigation.anchors[0]) == navigation::Error::kNone);
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kNone);
    CHECK(navigation::validateConnections(navigation.anchors, navigation.connections) == navigation::Error::kNone);
    CHECK(navigation::validate(navigation) == navigation::Error::kNone);
  }

  TEST_CASE("Rejects invalid anchors") {
    NavigationData navigation = makeNavigation();
    navigation.anchors[0].position = {300.0f, 0.0f, 50.0f};
    CHECK(navigation::validateAnchor(navigation.anchors[0]) == navigation::Error::kNone);
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kNone);

    navigation = makeNavigation();
    navigation.anchors[0].radius = -1.0f;
    CHECK(navigation::validateAnchor(navigation.anchors[0]) == navigation::Error::kBadAnchorRadius);
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kBadAnchorRadius);

    navigation = makeNavigation();
    navigation.anchors[0].flags = navigation::kAnchorFlagsAll | 0x40;
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kBadAnchorFlags);

    navigation = makeNavigation();
    navigation.anchors[0].name = "anchor__one";
    CHECK(navigation::validateAnchor(navigation.anchors[0]) == navigation::Error::kBadAnchorName);

    navigation = makeNavigation();
    navigation.anchors[0].name = std::string("anchor\0one", 10);
    CHECK(navigation::validateAnchor(navigation.anchors[0]) == navigation::Error::kBadAnchorName);
  }

  TEST_CASE("Nonempty anchor names are unique and empty names may repeat") {
    NavigationData navigation = makeNavigation();
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kNone);

    navigation.anchors[0].name = "marker";
    navigation.anchors[1].name = "marker";
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kDuplicateAnchorName);
    CHECK(navigation::repairAnchorNames(navigation.anchors) == 1);
    CHECK(navigation.anchors[0].name == "marker");
    CHECK(navigation.anchors[1].name == "marker_1");
    CHECK(navigation::validateAnchorDefinitions(navigation.anchors) == navigation::Error::kNone);
  }

  TEST_CASE("Rejects invalid connections") {
    NavigationData navigation = makeNavigation();
    navigation.connections[0].second = 4;
    CHECK(navigation::validateConnections(navigation.anchors, navigation.connections) ==
          navigation::Error::kBadConnectionIndex);

    navigation = makeNavigation();
    navigation.connections[0].first = 1;
    navigation.connections[0].second = 0;
    CHECK(navigation::validateConnections(navigation.anchors, navigation.connections) ==
          navigation::Error::kBadConnectionOrder);

    navigation.connections = {{0, 1}, {0, 1}};
    CHECK(navigation::validateConnections(navigation.anchors, navigation.connections) ==
          navigation::Error::kDuplicateConnection);
  }

  TEST_CASE("Inserts connections in canonical order") {
    NavigationData navigation = makeNavigation();
    navigation.anchors.push_back({{250.0f, 0.0f, 50.0f}, 30.0f, -80.0f, 0, {}});
    navigation.connections = {{1, 2}};

    AnchorConnectionIndex index = kInvalidAnchorConnectionIndex;
    REQUIRE(navigation::validateConnectionInsertion(navigation, {0, 2}, index) == navigation::Error::kNone);
    navigation::insertConnection(navigation, index, {0, 2});
    CHECK(index == 0);
    REQUIRE(navigation.connections.size() == 2);
    CHECK(navigation.connections[0].first == 0);
    CHECK(navigation.connections[0].second == 2);
    CHECK(navigation.connections[1].first == 1);
    CHECK(navigation.connections[1].second == 2);

    CHECK(navigation::validateConnectionInsertion(navigation, {0, 2}, index) ==
          navigation::Error::kDuplicateConnection);
    CHECK(index == kInvalidAnchorConnectionIndex);
  }

  TEST_CASE("Set connection distinguishes duplicates from ordering errors") {
    NavigationData navigation = makeNavigation();
    navigation.anchors.push_back({{250.0f, 0.0f, 50.0f}, 30.0f, -80.0f, 0, {}});
    navigation.connections = {{0, 1}, {1, 2}};

    CHECK(navigation::validateConnectionPlacement(navigation, 1, {0, 1}) == navigation::Error::kDuplicateConnection);
    REQUIRE(navigation::validateConnectionPlacement(navigation, 1, {0, 2}) == navigation::Error::kNone);
    navigation::setConnection(navigation, 1, {0, 2});
    CHECK(navigation::validateConnectionPlacement(navigation, 0, {1, 2}) == navigation::Error::kBadConnectionOrder);
  }

  TEST_CASE("Rejects invalid surfaces") {
    NavSurface surface = makeSurface();
    CHECK(navigation::validateSurface(surface) == navigation::Error::kNone);
    CHECK(navigation::validateSurface(std::optional<NavSurface>{}) == navigation::Error::kNone);

    for (Vertex& vertex : surface.vertices) {
      vertex.position.x -= 1000.0f;
      vertex.position.z += 20000.0f;
    }
    CHECK(navigation::validateSurface(surface) == navigation::Error::kNone);

    surface.vertices.clear();
    CHECK(navigation::validateSurface(surface) == navigation::Error::kBadSurface);

    surface = makeSurface();
    surface.vertices[0].position.x = std::numeric_limits<float>::infinity();
    CHECK(navigation::validateSurface(surface) == navigation::Error::kBadSurfaceVertex);

    surface = makeSurface();
    surface.triangles[0].vertices = {0, 1, 9};
    CHECK(navigation::validateSurface(surface) == navigation::Error::kBadSurfaceTriangle);

    surface = makeSurface();
    surface.triangles[0].vertices = {0, 1, 1};
    CHECK(navigation::validateSurface(surface) == navigation::Error::kDegenerateSurfaceTriangle);
  }
}
