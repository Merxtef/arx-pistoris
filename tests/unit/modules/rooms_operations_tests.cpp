// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "modules/geometry.h"
#include "modules/rooms.h"

#include <array>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

using namespace pistoris;

TEST_SUITE("rooms::operations") {
  TEST_CASE("Adds rooms with stable ids") {
    RoomsData rooms;
    Room first{"room_1"};
    rooms::repairRoomName(rooms, first);
    REQUIRE(rooms::validateRoom(first) == rooms::Error::kNone);
    RoomIndex room = rooms::addRoom(rooms, std::move(first));
    CHECK(room == 0);
    Room second{"room_2"};
    rooms::repairRoomName(rooms, second);
    REQUIRE(rooms::validateRoom(second) == rooms::Error::kNone);
    room = rooms::addRoom(rooms, std::move(second));
    CHECK(room == 1);

    REQUIRE(rooms.definitions.size() == 2);
    CHECK(rooms.definitions[0].name == "room_1");
    CHECK(rooms.definitions[1].name == "room_2");
  }

  TEST_CASE("Repairs duplicate room names before mutation") {
    RoomsData rooms;
    CHECK(rooms::addRoom(rooms, {"room"}) == 0);
    Room duplicate{"room"};
    rooms::repairRoomName(rooms, duplicate);
    CHECK(duplicate.name == "room_1");
    REQUIRE(rooms.definitions.size() == 1);
    CHECK(rooms.definitions[0].name == "room");
  }

  TEST_CASE("Appends face room assignments") {
    RoomsData rooms;
    rooms.definitions = {{"room_0"}, {"room_1"}};
    std::array<RoomIndex, 2> first = {0, 1};
    std::array<RoomIndex, 1> second = {0};

    REQUIRE(rooms::validateFaceRoomIndices(first, rooms.definitions.size()) == rooms::Error::kNone);
    rooms::appendFaceRooms(rooms, first);
    REQUIRE(rooms::validateFaceRoomIndices(second, rooms.definitions.size()) == rooms::Error::kNone);
    rooms::appendFaceRooms(rooms, second);

    CHECK(rooms.face_rooms == std::vector<RoomIndex>{0, 1, 0});

    const std::array<RoomIndex, 1> invalid = {2};
    CHECK(rooms::validateFaceRoomIndices(invalid, rooms.definitions.size()) == rooms::Error::kBadFaceRoomIndex);
    CHECK(rooms.face_rooms == std::vector<RoomIndex>{0, 1, 0});
  }

  TEST_CASE("Compacts face room assignments in place") {
    RoomsData rooms;
    rooms.face_rooms = {0, 1, 2, 3};
    const RoomIndex* storage = rooms.face_rooms.data();
    const std::array<FaceIndex, 4> remap = {kInvalidFaceIndex, 0, 1, kInvalidFaceIndex};

    rooms::remapFaceRooms(rooms, remap);

    CHECK(rooms.face_rooms == std::vector<RoomIndex>{1, 2});
    CHECK(rooms.face_rooms.data() == storage);
  }

  TEST_CASE("Adds portals with stable ids") {
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    Portal portal;
    portal.name = "portal";
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};

    REQUIRE(rooms::validatePortal(portal, rooms.definitions.size()) == rooms::Error::kNone);
    PortalIndex portal_index = rooms::addPortal(rooms, portal);
    CHECK(portal_index == 0);
    portal.name = "portal_2";
    REQUIRE(rooms::validatePortal(portal, rooms.definitions.size()) == rooms::Error::kNone);
    portal_index = rooms::addPortal(rooms, portal);
    CHECK(portal_index == 1);

    REQUIRE(rooms.portals.size() == 2);
    CHECK(rooms.portals[0].name == "portal");
    CHECK(rooms.portals[1].name == "portal_2");
  }

  TEST_CASE("Repairs duplicate portal names before mutation") {
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    Portal portal;
    portal.name = "portal";
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}};

    REQUIRE(rooms::addPortal(rooms, portal) == 0);
    rooms::repairPortalName(rooms, portal);
    CHECK(portal.name == "portal_1");
    REQUIRE(rooms.portals.size() == 1);
    CHECK(rooms.portals[0].name == "portal");
  }

  TEST_CASE("Room distance edits validate their canonical pair before allocation") {
    RoomsData rooms;
    rooms.definitions = {{"room_1"}, {"room_2"}};
    const RoomDistance unavailable = {
        .distance = -1.0f,
        .low_room_portal = kInvalidPortalIndex,
        .high_room_portal = kInvalidPortalIndex,
    };

    REQUIRE(rooms::validateRoomDistance(unavailable, rooms, 0, 1) == rooms::Error::kNone);
    rooms::setRoomDistance(rooms, 0, 1, unavailable);
    REQUIRE(rooms.distances.size() == 1);
    CHECK(rooms::validateRoomDistance(unavailable, rooms, 1, 1) == rooms::Error::kBadRoomDistance);
    CHECK(rooms::validateRoomDistance(unavailable, rooms, 0, 2) == rooms::Error::kBadRoomDistance);
    CHECK(rooms::validateRoomDistance({.distance = 1.0f}, rooms, 0, 1) == rooms::Error::kBadRoomDistance);
    CHECK(rooms.distances[0].distance == doctest::Approx(-1.0f));
  }

  TEST_CASE("Vertex weld segment collection is transactional") {
    GeometryData geometry;
    geometry.vertices = {{{0.0f, 0.0f, 0.0f}}};
    geometry.faces.resize(1);
    geometry.faces[0].corners[0].vertex = 1;

    RoomsData rooms;
    rooms.definitions = {{"room"}};
    rooms.face_rooms = {0};

    rooms::VertexWeldSegments segments{{7}, {0, 1}, {7}};
    CHECK(rooms::collectVertexWeldSegments(geometry, rooms, 0.1f, segments) == rooms::Error::kBadFaceVertex);
    CHECK(segments.vertices == std::vector<VertexIndex>{7});
    CHECK(segments.offsets == std::vector<std::size_t>{0, 1});
    CHECK(segments.protected_vertices == std::vector<VertexIndex>{7});

    geometry.faces[0].corners[0].vertex = 0;
    CHECK(rooms::collectVertexWeldSegments(geometry, rooms, std::numeric_limits<float>::quiet_NaN(), segments) ==
          rooms::Error::kInvalidOptions);
    CHECK(segments.vertices == std::vector<VertexIndex>{7});
    CHECK(segments.offsets == std::vector<std::size_t>{0, 1});
    CHECK(segments.protected_vertices == std::vector<VertexIndex>{7});
  }
}
