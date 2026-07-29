// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/flags.h"
#include "arx_pistoris/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"
#include "modules/rooms/internal.h"

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

void addSquare(GeometryData& geometry, RoomsData& rooms, RoomIndex room, float min_x, float max_x, float min_z,
               float max_z, float y = 0.0f, FaceType flags = 0) {
  const VertexIndex base = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back({{min_x, y, min_z}});
  geometry.vertices.push_back({{max_x, y, min_z}});
  geometry.vertices.push_back({{max_x, y, max_z}});
  geometry.vertices.push_back({{min_x, y, max_z}});
  geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2, flags));
  geometry.faces.push_back(makeFace(base + 0, base + 2, base + 3, flags));
  rooms.face_rooms.push_back(room);
  rooms.face_rooms.push_back(room);
}

void addHorizontalBlocker(GeometryData& geometry, RoomsData& rooms, RoomIndex room, FaceType flags = 0) {
  const VertexIndex base = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back({{0.0f, -30.0f, 0.0f}});
  geometry.vertices.push_back({{100.0f, -30.0f, 0.0f}});
  geometry.vertices.push_back({{0.0f, -30.0f, 100.0f}});
  geometry.faces.push_back(makeFace(base + 0, base + 1, base + 2, flags));
  rooms.face_rooms.push_back(room);
}

}  // namespace

TEST_SUITE("rooms::distance_sampling") {
  TEST_CASE("Selects room face ids and skips ignored flags") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    geometry.vertices.resize(5);
    geometry.faces.push_back(makeFace(0, 1, 2));
    geometry.faces.push_back(makeFace(0, 2, 3));
    geometry.faces.push_back(makeFace(0, 3, 4, kFaceBitTrans));
    rooms.face_rooms = {0, 1, 0};

    CHECK(rooms::faceIndicesForRoom(rooms, geometry, 0) == std::vector<FaceIndex>{0});
    CHECK(rooms::faceIndicesForRoom(rooms, geometry, 0, 0) == std::vector<FaceIndex>{0, 2});
    CHECK(rooms::faceIndicesForRoom(rooms, geometry, 1) == std::vector<FaceIndex>{1});

    rooms.face_rooms = {0};
    CHECK(rooms::faceIndicesForRoom(rooms, geometry, 0) == std::vector<FaceIndex>{0});
  }

  TEST_CASE("Builds support index from selected room faces") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    addSquare(geometry, rooms, 0, 0.0f, 100.0f, 0.0f, 100.0f);
    addSquare(geometry, rooms, 1, 200.0f, 300.0f, 0.0f, 100.0f);

    geometry::SurfaceSupportIndex support = rooms::buildRoomSupportIndex(rooms, geometry, 0);

    CHECK(support.hasBounds());
    CHECK_FALSE(support.hitsAt(25.0f, 25.0f).empty());
    CHECK(support.hitsAt(225.0f, 25.0f).empty());
    REQUIRE(support.triangles().size() == 2);
    CHECK(support.triangles()[0].face == 0);
    CHECK(support.triangles()[1].face == 1);
  }

  TEST_CASE("Offsets samples by support normal sign") {
    geometry::SurfaceSupportHit hit;
    hit.position = {1.0f, 2.0f, 3.0f};
    rooms::RoomDistanceOptions options;
    options.sample_height_offset = 60.0f;

    hit.normal = {0.0f, 1.0f, 0.0f};
    CHECK(rooms::offsetRoomDistanceSample(hit, options).y == doctest::Approx(62.0f));

    hit.normal = {0.0f, -1.0f, 0.0f};
    CHECK(rooms::offsetRoomDistanceSample(hit, options).y == doctest::Approx(-58.0f));
  }

  TEST_CASE("Checks sample offset clearance against same-room geometry") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    addSquare(geometry, rooms, 0, 0.0f, 100.0f, 0.0f, 100.0f);
    addHorizontalBlocker(geometry, rooms, 0);

    CHECK_FALSE(rooms::sampleOffsetClear(rooms, geometry, 0, {50.0f, 0.0f, 50.0f}, {50.0f, -60.0f, 50.0f}));

    rooms.face_rooms.back() = 1;
    CHECK(rooms::sampleOffsetClear(rooms, geometry, 0, {50.0f, 0.0f, 50.0f}, {50.0f, -60.0f, 50.0f}));

    rooms.face_rooms.back() = 0;
    geometry.faces.back().flags = kFaceBitTrans;
    CHECK(rooms::sampleOffsetClear(rooms, geometry, 0, {50.0f, 0.0f, 50.0f}, {50.0f, -60.0f, 50.0f}));
  }

  TEST_CASE("Adds sample nodes only for rooms with multiple portal nodes") {
    GeometryData geometry;
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    addSquare(geometry, rooms, 0, 0.0f, 200.0f, 0.0f, 200.0f);
    rooms::RoomDistanceOptions options;
    options.sample_spacing = 100.0f;
    options.sample_height_offset = 60.0f;

    rooms::RoomDistanceGenerationGraph skipped_graph;
    skipped_graph.rooms.resize(rooms.definitions.size());
    rooms::addRoomNode(skipped_graph.rooms[0], {{0.0f, -60.0f, 0.0f}, 0, 0, 0});
    skipped_graph.rooms[0].portal_nodes = {0};
    rooms::RoomDistanceGenDiagnostics diagnostics;
    rooms::addSampleNodes(rooms, geometry, options, skipped_graph, &diagnostics);
    CHECK(skipped_graph.rooms[0].nodes.size() == 1);
    REQUIRE(diagnostics.sampled_points_by_room.size() == 2);
    CHECK(diagnostics.sampled_points_by_room[0].empty());

    rooms::RoomDistanceGenerationGraph graph;
    graph.rooms.resize(rooms.definitions.size());
    rooms::addRoomNode(graph.rooms[0], {{0.0f, -60.0f, 0.0f}, 0, 0, 0});
    rooms::addRoomNode(graph.rooms[0], {{200.0f, -60.0f, 0.0f}, 0, 1, 0});
    graph.rooms[0].portal_nodes = {0, 1};
    diagnostics = {};
    rooms::addSampleNodes(rooms, geometry, options, graph, &diagnostics);

    CHECK(graph.rooms[0].nodes.size() > 2);
    REQUIRE(diagnostics.sampled_points_by_room.size() == 2);
    CHECK_FALSE(diagnostics.sampled_points_by_room[0].empty());
    for (const rooms::RoomDistanceDebugPoint& point : diagnostics.sampled_points_by_room[0])
      CHECK(point.position.y == doctest::Approx(-60.0f));
  }
}
