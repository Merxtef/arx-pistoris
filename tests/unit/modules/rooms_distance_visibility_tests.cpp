// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"

#include <cstdint>
#include <span>
#include <vector>

using namespace pistoris;

namespace {

Face makeFace(VertexIndex a, VertexIndex b, VertexIndex c, FaceType flags = 0) {
  Face face;
  face.corners[0].vertex = a;
  face.corners[1].vertex = b;
  face.corners[2].vertex = c;
  face.flags = flags;
  return face;
}

void addVerticalBlocker(GeometryData& geometry, RoomsData& rooms, RoomIndex room, FaceType flags = 0) {
  const VertexIndex base = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back({{50.0f, -100.0f, 0.0f}});
  geometry.vertices.push_back({{50.0f, 0.0f, 0.0f}});
  geometry.vertices.push_back({{50.0f, -100.0f, 100.0f}});
  geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2, flags));
  rooms.face_rooms.push_back(room);
}

}  // namespace

TEST_SUITE("rooms::distance_visibility") {
  TEST_CASE("Detects only interior segment triangle intersections") {
    CHECK(rooms::segmentIntersectsRoomDistanceTriangle(
        {0.25f, 0.25f, -1.0f}, {0.25f, 0.25f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
    CHECK_FALSE(rooms::segmentIntersectsRoomDistanceTriangle(
        {0.25f, 0.25f, 0.0f}, {0.25f, 0.25f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
    CHECK_FALSE(rooms::segmentIntersectsRoomDistanceTriangle(
        {2.0f, 2.0f, -1.0f}, {2.0f, 2.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}));
  }

  TEST_CASE("Checks blockers by room and flags") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    addVerticalBlocker(geometry, rooms, 0);
    std::vector<std::uint32_t> scratch;
    rooms::RoomGeometryIndex blocked(rooms, geometry);

    CHECK(rooms::blockedByRoomGeometry(blocked, 0, {0.0f, -50.0f, 50.0f}, {100.0f, -50.0f, 50.0f}, scratch));
    CHECK_FALSE(rooms::blockedByRoomGeometry(blocked, 1, {0.0f, -50.0f, 50.0f}, {100.0f, -50.0f, 50.0f}, scratch));

    geometry.faces[0].flags = kFaceBitTrans;
    rooms::RoomGeometryIndex ignored(rooms, geometry);
    CHECK_FALSE(rooms::blockedByRoomGeometry(ignored, 0, {0.0f, -50.0f, 50.0f}, {100.0f, -50.0f, 50.0f}, scratch));
  }

  TEST_CASE("Adds visibility edges and diagnostics") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room"}};
    rooms::RoomDistanceGenerationGraph graph;
    graph.rooms.resize(1);
    rooms::addRoomNode(graph.rooms[0], {{0.0f, -50.0f, 0.0f}, 0});
    rooms::addRoomNode(graph.rooms[0], {{100.0f, -50.0f, 0.0f}, 0});
    rooms::RoomDistanceOptions options;
    options.sample_spacing = 100.0f;
    options.max_link_distance = 200.0f;
    rooms::RoomDistanceGenerationDiagnostics diagnostics;
    rooms::RoomGeometryIndex room_geometry(rooms, geometry);

    rooms::addVisibilityEdges(rooms, room_geometry, options, graph, &diagnostics);
    const rooms::RoomDistanceAdjacency adjacency =
        rooms::buildAdjacency(graph.rooms[0].nodes.size(), graph.rooms[0].edges);

    const std::span<const rooms::RoomDistanceEdge> edges = rooms::adjacentEdges(adjacency, 0);
    REQUIRE(edges.size() == 1);
    CHECK(edges[0].to == 1);
    CHECK(edges[0].cost == doctest::Approx(100.0f));
    REQUIRE(diagnostics.in_room_visibility_edges.size() == 1);
    CHECK(diagnostics.in_room_visibility_edges[0].room_1 == 0);
  }

  TEST_CASE("Respects max link distance") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room"}};
    rooms::RoomDistanceGenerationGraph graph;
    graph.rooms.resize(1);
    rooms::addRoomNode(graph.rooms[0], {{0.0f, -50.0f, 0.0f}, 0});
    rooms::addRoomNode(graph.rooms[0], {{100.0f, -50.0f, 0.0f}, 0});
    rooms::RoomDistanceOptions options;
    options.sample_spacing = 100.0f;
    options.max_link_distance = 50.0f;
    rooms::RoomGeometryIndex room_geometry(rooms, geometry);

    rooms::addVisibilityEdges(rooms, room_geometry, options, graph);
    const rooms::RoomDistanceAdjacency adjacency =
        rooms::buildAdjacency(graph.rooms[0].nodes.size(), graph.rooms[0].edges);

    CHECK(rooms::adjacentEdges(adjacency, 0).empty());
    CHECK(rooms::adjacentEdges(adjacency, 1).empty());
  }

  TEST_CASE("Finds links across more than one sampling cell") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room"}};
    rooms::RoomDistanceGenerationGraph graph;
    graph.rooms.resize(1);
    rooms::addRoomNode(graph.rooms[0], {{99.0f, -50.0f, 0.0f}, 0});
    rooms::addRoomNode(graph.rooms[0], {{201.0f, -50.0f, 0.0f}, 0});
    rooms::RoomDistanceOptions options;
    options.sample_spacing = 100.0f;
    options.max_link_distance = 150.0f;
    rooms::RoomGeometryIndex room_geometry(rooms, geometry);

    rooms::addVisibilityEdges(rooms, room_geometry, options, graph);

    REQUIRE(graph.rooms[0].edges.size() == 1);
    CHECK(graph.rooms[0].edges[0].first == 0);
    CHECK(graph.rooms[0].edges[0].second == 1);
  }

  TEST_CASE("Skips blocked visibility edges") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    addVerticalBlocker(geometry, rooms, 0);
    rooms::RoomDistanceGenerationGraph graph;
    graph.rooms.resize(1);
    rooms::addRoomNode(graph.rooms[0], {{0.0f, -50.0f, 50.0f}, 0});
    rooms::addRoomNode(graph.rooms[0], {{100.0f, -50.0f, 50.0f}, 0});
    rooms::RoomDistanceOptions options;
    options.sample_spacing = 100.0f;
    options.max_link_distance = 200.0f;
    rooms::RoomGeometryIndex blocked(rooms, geometry);

    rooms::addVisibilityEdges(rooms, blocked, options, graph);
    CHECK(graph.rooms[0].edges.empty());

    rooms.face_rooms[0] = 1;
    rooms::RoomGeometryIndex unblocked(rooms, geometry);
    rooms::addVisibilityEdges(rooms, unblocked, options, graph);
    CHECK(graph.rooms[0].edges.size() == 1);
  }
}
