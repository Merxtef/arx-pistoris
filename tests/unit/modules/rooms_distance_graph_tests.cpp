// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/indices.h"

#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace pistoris;

namespace {

Portal makePortal(RoomIndex first, RoomIndex second) {
  Portal portal;
  portal.name = "portal";
  portal.room_1 = first;
  portal.room_2 = second;
  portal.shape = PortalShape::kQuad;
  portal.vertices = {{
      {0.0f, 0.0f, 0.0f},
      {1.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
  }};
  return portal;
}

}  // namespace

TEST_SUITE("rooms::distance_graph") {
  TEST_CASE("Adds room nodes and undirected edges") {
    rooms::RoomGraph graph;

    CHECK(rooms::addRoomNode(graph, {.position = {1.0f, 2.0f, 3.0f}, .room = 0}) == 0);
    CHECK(rooms::addRoomNode(graph, {.position = {4.0f, 5.0f, 6.0f}, .room = 0}) == 1);
    rooms::addGraphEdge(graph, 0, 1, 12.0f);

    REQUIRE(graph.nodes.size() == 2);
    REQUIRE(graph.adjacency.size() == 2);
    REQUIRE(graph.adjacency[0].size() == 1);
    REQUIRE(graph.adjacency[1].size() == 1);
    CHECK(graph.adjacency[0][0].to == 1);
    CHECK(graph.adjacency[0][0].cost == doctest::Approx(12.0f));
    CHECK(graph.adjacency[1][0].to == 0);
    CHECK(graph.adjacency[1][0].cost == doctest::Approx(12.0f));
  }

  TEST_CASE("Adds global undirected edges") {
    std::vector<std::vector<rooms::RoomDistanceEdge>> adjacency(3);

    rooms::addGlobalEdge(adjacency, 0, 2, 5.0f);

    REQUIRE(adjacency[0].size() == 1);
    REQUIRE(adjacency[2].size() == 1);
    CHECK(adjacency[0][0].to == 2);
    CHECK(adjacency[0][0].cost == doctest::Approx(5.0f));
    CHECK(adjacency[2][0].to == 0);
    CHECK(adjacency[2][0].cost == doctest::Approx(5.0f));
  }

  TEST_CASE("Maps portal sides to global side ids") {
    CHECK(rooms::globalPortalSide(0, true) == 0);
    CHECK(rooms::globalPortalSide(0, false) == 1);
    CHECK(rooms::globalPortalSide(3, true) == 6);
    CHECK(rooms::globalPortalSide(3, false) == 7);
  }

  TEST_CASE("Lists portal sides for a room") {
    RoomsData data;
    data.definitions = {{"room_1"}, {"room_2"}, {"room_3"}};
    data.portals.push_back(makePortal(0, 1));
    data.portals.push_back(makePortal(2, 0));
    data.portals.push_back(makePortal(1, 2));

    CHECK(rooms::portalSidesForRoom(data, 0) == std::vector<std::uint32_t>{0, 3});
    CHECK(rooms::portalSidesForRoom(data, 1) == std::vector<std::uint32_t>{1, 4});
    CHECK(rooms::portalSidesForRoom(data, 2) == std::vector<std::uint32_t>{2, 5});
    CHECK(rooms::portalSidesForRoom(data, 9).empty());
  }

  TEST_CASE("Reads portal side positions") {
    rooms::RoomDistanceGenerationGraph graph;
    graph.portal_sides.resize(2);
    graph.portal_sides[0][0].position = {1.0f, 2.0f, 3.0f};
    graph.portal_sides[0][1].position = {4.0f, 5.0f, 6.0f};
    graph.portal_sides[1][0].position = {7.0f, 8.0f, 9.0f};

    CHECK(rooms::portalSidePosition(graph, 0).x == doctest::Approx(1.0f));
    CHECK(rooms::portalSidePosition(graph, 1).x == doctest::Approx(4.0f));
    CHECK(rooms::portalSidePosition(graph, 2).x == doctest::Approx(7.0f));
    CHECK(rooms::portalSidePosition(graph, 99).x == doctest::Approx(0.0f));
  }

  TEST_CASE("Converts global path to portal side positions") {
    rooms::RoomDistanceGenerationGraph graph;
    graph.portal_sides.resize(2);
    graph.portal_sides[0][0].position = {1.0f, 0.0f, 0.0f};
    graph.portal_sides[1][1].position = {2.0f, 0.0f, 0.0f};
    std::array<std::uint32_t, 2> path = {0, 3};

    std::vector<ArxVector3> points = rooms::globalPathPositions(graph, path);

    REQUIRE(points.size() == 2);
    CHECK(points[0].x == doctest::Approx(1.0f));
    CHECK(points[1].x == doctest::Approx(2.0f));
  }

  TEST_CASE("Adds portal side access points and diagnostics") {
    RoomsData data;
    data.definitions = {{"room_1"}, {"room_2"}};
    data.portals.push_back(makePortal(0, 1));
    rooms::RoomDistanceGenerationGraph graph;
    graph.rooms.resize(data.definitions.size());
    rooms::RoomDistanceGenDiagnostics diagnostics;

    rooms::addPortalSideAccessPoints(data, {.portal_side_offset = 10.0f}, graph, &diagnostics);

    REQUIRE(graph.portal_sides.size() == 1);
    REQUIRE(graph.rooms[0].portal_nodes.size() == 1);
    REQUIRE(graph.rooms[1].portal_nodes.size() == 1);
    CHECK(graph.portal_sides[0][0].position.z == doctest::Approx(10.0f));
    CHECK(graph.portal_sides[0][1].position.z == doctest::Approx(-10.0f));
    REQUIRE(diagnostics.portal_access_points_by_room.size() == 2);
    CHECK(diagnostics.portal_access_points_by_room[0].size() == 1);
    CHECK(diagnostics.portal_access_points_by_room[1].size() == 1);
    CHECK(diagnostics.portal_access_segments.size() == 2);
  }
}
