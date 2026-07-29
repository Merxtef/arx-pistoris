// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"

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

    CHECK(rooms::blockedByRoomGeometry(rooms, geometry, 0, {0.0f, -50.0f, 50.0f}, {100.0f, -50.0f, 50.0f}));
    CHECK_FALSE(rooms::blockedByRoomGeometry(rooms, geometry, 1, {0.0f, -50.0f, 50.0f}, {100.0f, -50.0f, 50.0f}));

    geometry.faces[0].flags = kFaceBitTrans;
    CHECK_FALSE(rooms::blockedByRoomGeometry(rooms, geometry, 0, {0.0f, -50.0f, 50.0f}, {100.0f, -50.0f, 50.0f}));
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
    rooms::RoomDistanceGenDiagnostics diagnostics;

    rooms::addVisibilityEdges(rooms, geometry, options, graph, &diagnostics);

    REQUIRE(graph.rooms[0].adjacency[0].size() == 1);
    CHECK(graph.rooms[0].adjacency[0][0].to == 1);
    CHECK(graph.rooms[0].adjacency[0][0].cost == doctest::Approx(100.0f));
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

    rooms::addVisibilityEdges(rooms, geometry, options, graph);

    CHECK(graph.rooms[0].adjacency[0].empty());
    CHECK(graph.rooms[0].adjacency[1].empty());
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

    rooms::addVisibilityEdges(rooms, geometry, options, graph);
    CHECK(graph.rooms[0].adjacency[0].empty());

    rooms.face_rooms[0] = 1;
    rooms::addVisibilityEdges(rooms, geometry, options, graph);
    CHECK(graph.rooms[0].adjacency[0].size() == 1);
  }
}
