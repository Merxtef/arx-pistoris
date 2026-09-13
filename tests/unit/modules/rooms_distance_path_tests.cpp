// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"

#include "modules/rooms/internal.h"

#include <algorithm>
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
    const rooms::RoomDistanceAdjacency adjacency = rooms::buildAdjacency(graph.nodes.size(), graph.edges);
    rooms::DijkstraScratch scratch;

    rooms::DijkstraPath path = rooms::shortestPath(adjacency, 0, 2, scratch);

    REQUIRE(path.found);
    CHECK(path.distance == doctest::Approx(7.0f));
    CHECK(std::ranges::equal(path.nodes, std::array<std::uint32_t, 3>{0, 1, 2}));
  }

  TEST_CASE("Handles same, invalid, and unreachable room graph paths") {
    rooms::RoomGraph graph = makePathGraph();
    const rooms::RoomDistanceAdjacency adjacency = rooms::buildAdjacency(graph.nodes.size(), graph.edges);
    rooms::DijkstraScratch scratch;

    rooms::DijkstraPath same = rooms::shortestPath(adjacency, 1, 1, scratch);
    REQUIRE(same.found);
    CHECK(same.distance == doctest::Approx(0.0f));
    CHECK(std::ranges::equal(same.nodes, std::array<std::uint32_t, 1>{1}));

    CHECK_FALSE(rooms::shortestPath(adjacency, 99, 1, scratch).found);

    rooms::addRoomNode(graph, {.position = {10.0f, 0.0f, 0.0f}, .room = 0});
    const rooms::RoomDistanceAdjacency expanded_adjacency = rooms::buildAdjacency(graph.nodes.size(), graph.edges);
    CHECK_FALSE(rooms::shortestPath(expanded_adjacency, 0, 3, scratch).found);
  }

  TEST_CASE("Converts path ids to positions") {
    rooms::RoomGraph graph = makePathGraph();
    std::array<std::uint32_t, 3> path = {0, 1, 2};

    std::vector<ArxVector3> points = rooms::pathPositions(graph, path);

    REQUIRE(points.size() == 3);
    CHECK(points[0].x == doctest::Approx(0.0f));
    CHECK(points[1].x == doctest::Approx(3.0f));
    CHECK(points[2].y == doctest::Approx(4.0f));
  }

  TEST_CASE("Finds shortest global path from several starts to several goals") {
    const std::array<rooms::RoomDistanceUndirectedEdge, 4> edges = {
        {{0, 2, 10.0f}, {1, 2, 2.0f}, {2, 3, 3.0f}, {2, 4, 20.0f}}};
    const rooms::RoomDistanceAdjacency adjacency = rooms::buildAdjacency(5, edges);
    std::array<std::uint32_t, 2> starts = {0, 1};
    std::array<std::uint32_t, 2> goals = {3, 4};
    rooms::DijkstraScratch scratch;

    rooms::GlobalPath path = rooms::shortestGlobalPath(adjacency, starts, goals, scratch);

    REQUIRE(path.found);
    CHECK(path.distance == doctest::Approx(5.0f));
    CHECK(std::ranges::equal(path.nodes, std::array<std::uint32_t, 3>{1, 2, 3}));
  }

  TEST_CASE("Handles empty, invalid, and unreachable global paths") {
    const std::array<rooms::RoomDistanceUndirectedEdge, 1> edges = {{{0, 1, 1.0f}}};
    const rooms::RoomDistanceAdjacency adjacency = rooms::buildAdjacency(3, edges);
    std::array<std::uint32_t, 1> valid_start = {0};
    std::array<std::uint32_t, 1> invalid_start = {99};
    std::array<std::uint32_t, 1> valid_goal = {2};
    std::array<std::uint32_t, 0> empty = {};
    rooms::DijkstraScratch scratch;

    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, empty, valid_goal, scratch).found);
    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, valid_start, empty, scratch).found);
    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, invalid_start, valid_goal, scratch).found);
    CHECK_FALSE(rooms::shortestGlobalPath(adjacency, valid_start, valid_goal, scratch).found);
  }
}
