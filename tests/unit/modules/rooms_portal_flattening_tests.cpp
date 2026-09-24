// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"

#include "modules/rooms.h"

#include <string>
#include <utility>
#include <vector>

using namespace pistoris;

namespace {

Portal quad(std::string name = "quad") {
  Portal portal;
  portal.name = std::move(name);
  portal.room_1 = 0;
  portal.room_2 = 1;
  portal.vertices = {{{0.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {0.02f, 2.0f, 2.0f}, {0.0f, 0.0f, 2.0f}}};
  return portal;
}

RoomsData roomsWith(std::vector<Portal> portals) {
  RoomsData rooms;
  rooms.definitions = {{"first"}, {"second"}};
  rooms.portals = std::move(portals);
  rooms.distances = {{.distance = 0.0f, .low_room_portal = 0, .high_room_portal = 0}};
  return rooms;
}

}  // namespace

TEST_SUITE("rooms::portal_flattening") {
  TEST_CASE("Flattens a quad onto the canonical engine plane") {
    RoomsData rooms = roomsWith({quad()});
    const Portal original = rooms.portals[0];
    rooms::PortalFlattenStatistics statistics;

    REQUIRE(rooms::flattenPortals(rooms, &statistics) == rooms::Error::kNone);
    CHECK(rooms.portals[0].vertices[0] == original.vertices[0]);
    CHECK(rooms.portals[0].vertices[1] == original.vertices[1]);
    CHECK(rooms.portals[0].vertices[2] == ArxVector3{0.0f, 2.0f, 2.0f});
    CHECK(rooms.portals[0].vertices[3] == original.vertices[3]);
    CHECK(rooms.distances.empty());
    CHECK(statistics.flattened_quads == 1);
    CHECK(statistics.already_planar_quads == 0);
    CHECK(statistics.triangles == 0);
  }

  TEST_CASE("Leaves planar quads and triangles unchanged") {
    Portal planar = quad("planar");
    planar.vertices[2].x = 0.0f;
    Portal triangle = quad("triangle");
    triangle.shape = PortalShape::kTriangle;
    RoomsData rooms = roomsWith({planar, triangle});
    const RoomDistances distances = rooms.distances;
    rooms::PortalFlattenStatistics statistics;

    REQUIRE(rooms::flattenPortals(rooms, &statistics) == rooms::Error::kNone);
    REQUIRE(rooms.distances.size() == distances.size());
    CHECK(rooms.distances[0].distance == distances[0].distance);
    CHECK(rooms.distances[0].low_room_portal == distances[0].low_room_portal);
    CHECK(rooms.distances[0].high_room_portal == distances[0].high_room_portal);
    CHECK(statistics.flattened_quads == 0);
    CHECK(statistics.already_planar_quads == 1);
    CHECK(statistics.triangles == 1);
  }

  TEST_CASE("Rejects invalid input without partial mutation or statistics") {
    Portal invalid = quad();
    invalid.vertices[2] = invalid.vertices[1];
    RoomsData rooms = roomsWith({invalid});
    const ArxVector3 original = rooms.portals[0].vertices[2];
    rooms::PortalFlattenStatistics statistics{.flattened_quads = 7};

    CHECK(rooms::flattenPortals(rooms, &statistics) == rooms::Error::kBadPortalVertex);
    CHECK(rooms.portals[0].vertices[2] == original);
    CHECK(rooms.distances.size() == 1);
    CHECK(statistics.flattened_quads == 7);
  }
}
