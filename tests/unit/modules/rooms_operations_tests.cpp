// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/indices.h"

#include "modules/rooms.h"

#include <array>
#include <vector>

using namespace pistoris;

TEST_SUITE("rooms::operations") {
  TEST_CASE("Adds rooms with stable ids") {
    RoomsData rooms;

    CHECK(rooms::addRoom(rooms, "room_1") == 0);
    CHECK(rooms::addRoom(rooms, "room_2") == 1);

    REQUIRE(rooms.definitions.size() == 2);
    CHECK(rooms.definitions[0].name == "room_1");
    CHECK(rooms.definitions[1].name == "room_2");
  }

  TEST_CASE("Appends face room assignments") {
    RoomsData rooms;
    std::array<RoomIndex, 2> first = {0, 1};
    std::array<RoomIndex, 1> second = {0};

    rooms::addFaceRooms(rooms, first);
    rooms::addFaceRooms(rooms, second);

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
    Portal portal;
    portal.name = "portal";
    portal.room_1 = 0;
    portal.room_2 = 1;

    CHECK(rooms::addPortal(rooms, portal) == 0);
    portal.name = "portal_2";
    CHECK(rooms::addPortal(rooms, portal) == 1);

    REQUIRE(rooms.portals.size() == 2);
    CHECK(rooms.portals[0].name == "portal");
    CHECK(rooms.portals[1].name == "portal_2");
  }
}
