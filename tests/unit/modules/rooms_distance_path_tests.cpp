// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"

#include "modules/rooms/internal.h"

#include <array>
#include <cstdint>
#include <vector>

using namespace pistoris;

namespace {

rooms::RoomGraph makePathGraph() {
  rooms::RoomGraph graph;
  rooms::addRoomNode(graph, {.position = {0.0f, 0.0f, 0.0f}, .room = 0});
  rooms::addRoomNode(graph, {.position = {3.0f, 0.0f, 0.0f}, .room = 0});
  rooms::addRoomNode(graph, {.position = {3.0f, 4.0f, 0.0f}, .room = 0});
  rooms::addGraphEdge(graph, 0, 1, 3.0f);
  rooms::addGraphEdge(graph, 1, 2, 4.0f);
  rooms::addGraphEdge(graph, 0, 2, 20.0f);
  return graph;
}

}  // namespace

TEST_SUITE("rooms::distance_path") {
  TEST_CASE("Finds shortest room graph path") {
    rooms::RoomGraph graph = makePathGraph();

    rooms::DijkstraPath path = rooms::shortestPath(graph, 0, 2);

    REQUIRE(path.found);
    CHECK(path.distance == doctest::Approx(7.0f));
    CHECK(path.nodes == std::vector<std::uint32_t>{0, 1, 2});
  }

  TEST_CASE("Handles same, invalid, and unreachable room graph paths") {
    rooms::RoomGraph graph = makePathGraph();

    rooms::DijkstraPath same = rooms::shortestPath(graph, 1, 1);
    REQUIRE(same.found);
    CHECK(same.distance == doctest::Approx(0.0f));
    CHECK(same.nodes == std::vector<std::uint32_t>{1});

    CHECK_FALSE(rooms::shortestPath(graph, 99, 1).found);

    rooms::addRoomNode(graph, {.position = {10.0f, 0.0f, 0.0f}, .room = 0});
    CHECK_FALSE(rooms::shortestPath(graph, 0, 3).found);
  }

  TEST_CASE("Converts path ids to positions and measures distance") {
    rooms::RoomGraph graph = makePathGraph();
    std::array<std::uint32_t, 3> path = {0, 1, 2};

    std::vector<ArxVector3> points = rooms::pathPositions(graph, path);

    REQUIRE(points.size() == 3);
    CHECK(points[0].x == doctest::Approx(0.0f));
    CHECK(points[1].x == doctest::Approx(3.0f));
    CHECK(points[2].y == doctest::Approx(4.0f));
    CHECK(rooms::pathDistance(points) == doctest::Approx(7.0f));
  }

  TEST_CASE("Finds shortest global path from several starts to several goals") {
    std::vector<std::vector<rooms::RoomDistanceEdge>> adjacency(5);
    rooms::addGlobalEdge(adjacency, 0, 2, 10.0f);
    rooms::addGlobalEdge(adjacency, 1, 2, 2.0f);
    rooms::addGlobalEdge(adjacency, 2, 3, 3.0f);
    rooms::addGlobalEdge(adjacency, 2, 4, 20.0f);
    std::array<std::uint32_t, 2> starts = {0, 1};
    std::array<std::uint32_t, 2> goals = {3, 4};

    rooms::GlobalPath path = rooms::shortestGlobalPath(adjacency, starts, goals);

    REQUIRE(path.found);
    CHECK(path.distance == doctest::Approx(5.0f));
    CHECK(path.nodes == std::vector<std::uint32_t>{1, 2, 3});
  }

  TEST_CASE("Handles empty, invalid, and unreachable global paths") {
    std::vector<std::vector<rooms::RoomDistanceEdge>> adjacency(3);
    rooms::addGlobalEdge(adjacency, 0, 1, 1.0f);
    std::array<std::uint32_t, 1> valid_start = {0};
    std::array<std::uint32_t, 1> invalid_start = {99};
    std::array<std::uint32_t, 1> valid_goal = {2};
    std::array<std::uint32_t, 0> empty = {};

    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, empty, valid_goal).found);
    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, valid_start, empty).found);
    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, invalid_start, valid_goal).found);
    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, valid_start, valid_goal).found);
  }
}
