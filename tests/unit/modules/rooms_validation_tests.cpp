// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "modules/rooms.h"

#include <limits>
#include <string>

using namespace pistoris;

namespace {

Portal makePortal() {
  Portal portal;
  portal.name = "portal";
  portal.room_1 = 0;
  portal.room_2 = 1;
  portal.shape = PortalShape::kQuad;
  portal.vertices = {{
      {0.0f, 0.0f, 0.0f},
      {1.0f, 0.0f, 0.0f},
      {1.0f, 1.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
  }};
  return portal;
}

RoomsData makeValidRooms() {
  RoomsData data;
  data.definitions = {{"room_1"}, {"room_2"}};
  data.face_rooms = {0, 1};
  data.portals.push_back(makePortal());
  return data;
}

}  // namespace

TEST_SUITE("rooms::validation") {
  TEST_CASE("Accepts valid room data") {
    RoomsData data = makeValidRooms();

    CHECK(rooms::validateRoomDefinitions(data) == rooms::Error::kNone);
    CHECK(rooms::validateFaceRooms(data, 2) == rooms::Error::kNone);
    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kNone);
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kNone);
    CHECK(rooms::validatePortalRoomRefs(data) == rooms::Error::kNone);
    CHECK(rooms::validatePortals(data) == rooms::Error::kNone);
    CHECK(rooms::validate(data, 2) == rooms::Error::kNone);
  }

  TEST_CASE("Rejects bad room definitions") {
    RoomsData data = makeValidRooms();
    CHECK(rooms::validateRoom(data.definitions[0]) == rooms::Error::kNone);
    data.definitions.clear();
    CHECK(rooms::validateRoomDefinitions(data) == rooms::Error::kNoRooms);

    data = makeValidRooms();
    data.definitions[0].name.clear();
    CHECK(rooms::validateRoomDefinitions(data) == rooms::Error::kBadRoomName);

    data = makeValidRooms();
    data.definitions[1].name = data.definitions[0].name;
    CHECK(rooms::validateRoomDefinitions(data) == rooms::Error::kDuplicateRoomName);

    data = makeValidRooms();
    data.definitions[0].name = "room__one";
    CHECK(rooms::validateRoom(data.definitions[0]) == rooms::Error::kBadRoomName);
    CHECK(rooms::validateRoomDefinitions(data) == rooms::Error::kBadRoomName);

    data = makeValidRooms();
    data.definitions[0].name = std::string("room\0one", 8);
    CHECK(rooms::validateRoom(data.definitions[0]) == rooms::Error::kBadRoomName);
    CHECK(rooms::validateRoomDefinitions(data) == rooms::Error::kBadRoomName);
  }

  TEST_CASE("Rejects bad face room assignments") {
    RoomsData data = makeValidRooms();
    CHECK(rooms::validateFaceRooms(data, 3) == rooms::Error::kBadFaceRoomCount);

    data = makeValidRooms();
    data.face_rooms[1] = 2;
    CHECK(rooms::validateFaceRooms(data, 2) == rooms::Error::kBadFaceRoomIndex);
  }

  TEST_CASE("Accepts valid room distances") {
    RoomsData data = makeValidRooms();
    data.distances = {{.distance = 12.0f, .low_room_portal = 0, .high_room_portal = 0}};

    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kNone);

    data.distances = {{.distance = -1.0f, .low_room_portal = 0, .high_room_portal = 0}};
    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kNone);

    data.distances = {{}};
    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kNone);
  }

  TEST_CASE("Rejects bad room distances") {
    RoomsData data = makeValidRooms();
    data.distances = {
        {.distance = std::numeric_limits<float>::infinity(), .low_room_portal = 0, .high_room_portal = 0}};
    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kBadRoomDistance);

    data = makeValidRooms();
    data.distances = {{.distance = 12.0f, .low_room_portal = 1, .high_room_portal = 1}};
    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kBadRoomDistance);

    data = makeValidRooms();
    data.distances = {{.distance = -1.0f, .low_room_portal = 0, .high_room_portal = kInvalidPortalIndex}};
    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kBadRoomDistance);
  }

  TEST_CASE("Rejects incomplete room distance storage") {
    RoomsData data = makeValidRooms();
    data.definitions.push_back({"room_3"});
    data.distances = {{.distance = 12.0f, .low_room_portal = 0, .high_room_portal = 0}};

    CHECK(rooms::validateRoomDistances(data) == rooms::Error::kBadRoomDistanceCount);
    CHECK(rooms::validate(data, 2) == rooms::Error::kBadRoomDistanceCount);
  }

  TEST_CASE("Rejects bad portal definitions") {
    RoomsData data = makeValidRooms();
    CHECK(rooms::validatePortal(data.portals[0], data.definitions.size()) == rooms::Error::kNone);
    data.portals[0].name.clear();
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kBadPortalName);

    data = makeValidRooms();
    data.portals[0].name = "portal__one";
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kBadPortalName);
    CHECK(rooms::validatePortal(data.portals[0], data.definitions.size()) == rooms::Error::kBadPortalName);

    data = makeValidRooms();
    data.portals[0].name = std::string("portal\0one", 10);
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kBadPortalName);
    CHECK(rooms::validatePortal(data.portals[0], data.definitions.size()) == rooms::Error::kBadPortalName);

    data = makeValidRooms();
    data.portals[0].vertices[2] = data.portals[0].vertices[1];
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kBadPortalVertex);

    data = makeValidRooms();
    data.portals[0].vertices[0].x = -0.01f;
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kNone);
  }

  TEST_CASE("Rejects and repairs duplicate portal names") {
    RoomsData data = makeValidRooms();
    data.portals.push_back(data.portals.front());
    data.portals.back().room_1 = 1;
    data.portals.back().room_2 = 0;

    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kDuplicatePortalName);
    CHECK(rooms::validatePortals(data) == rooms::Error::kDuplicatePortalName);
    CHECK(rooms::validate(data, 2) == rooms::Error::kDuplicatePortalName);
    CHECK(rooms::repairPortalNames(data.portals) == 1);
    CHECK(data.portals[0].name == "portal");
    CHECK(data.portals[1].name == "portal_1");
    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kNone);
  }

  TEST_CASE("Rejects bad portal room references") {
    RoomsData data = makeValidRooms();
    data.portals[0].room_2 = 2;
    CHECK(rooms::validatePortalRoomRefs(data) == rooms::Error::kBadPortalRoom);
    CHECK(rooms::validatePortals(data) == rooms::Error::kBadPortalRoom);

    data = makeValidRooms();
    data.portals[0].room_2 = 0;
    CHECK(rooms::validatePortalRoomRefs(data) == rooms::Error::kBadPortalRoom);
    CHECK(rooms::validatePortals(data) == rooms::Error::kBadPortalRoom);
  }

  TEST_CASE("Combined portal validation matches level error order") {
    RoomsData data = makeValidRooms();
    data.portals[0].room_2 = 9;
    data.portals[0].vertices[2] = data.portals[0].vertices[1];

    CHECK(rooms::validatePortalDefinitions(data) == rooms::Error::kBadPortalVertex);
    CHECK(rooms::validatePortals(data) == rooms::Error::kBadPortalRoom);
  }

  TEST_CASE("Validate composes checks in order") {
    RoomsData data = makeValidRooms();
    data.face_rooms[0] = 9;
    CHECK(rooms::validate(data, 2) == rooms::Error::kBadFaceRoomIndex);

    data = makeValidRooms();
    data.portals[0].room_2 = 9;
    CHECK(rooms::validate(data, 2) == rooms::Error::kBadPortalRoom);
  }
}
