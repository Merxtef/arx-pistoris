// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"

#include "modules/rooms.h"

using namespace pistoris;

TEST_SUITE("rooms::distance_storage") {
  TEST_CASE("Counts compact room-distance pairs") {
    CHECK(rooms::roomDistancePairCount(0) == 0);
    CHECK(rooms::roomDistancePairCount(1) == 0);
    CHECK(rooms::roomDistancePairCount(2) == 1);
    CHECK(rooms::roomDistancePairCount(4) == 6);
  }

  TEST_CASE("Indexes compact room-distance pairs symmetrically") {
    CHECK(rooms::roomDistancePairIndex(0, 1) == 0);
    CHECK(rooms::roomDistancePairIndex(1, 0) == 0);
    CHECK(rooms::roomDistancePairIndex(0, 2) == 1);
    CHECK(rooms::roomDistancePairIndex(2, 0) == 1);
    CHECK(rooms::roomDistancePairIndex(1, 2) == 2);
    CHECK(rooms::roomDistancePairIndex(0, 3) == 3);
    CHECK(rooms::roomDistancePairIndex(1, 3) == 4);
    CHECK(rooms::roomDistancePairIndex(2, 3) == 5);
  }

  TEST_CASE("Detects complete room-distance storage") {
    RoomDistances distances;
    CHECK(rooms::hasCompleteRoomDistances(distances, 1));
    CHECK_FALSE(rooms::hasCompleteRoomDistances(distances, 2));

    distances = {{.distance = -1.0f, .low_room_portal = 3, .high_room_portal = 3}};
    CHECK(rooms::hasCompleteRoomDistances(distances, 2));
    CHECK_FALSE(rooms::hasCompleteRoomDistances(distances, 3));

    distances.clear();
    CHECK_FALSE(rooms::hasCompleteRoomDistances(distances, 2));
  }

  TEST_CASE("Initializes compact room-distance storage") {
    RoomDistances distances;
    distances = {{.distance = 12.0f, .low_room_portal = 1, .high_room_portal = 2}};

    rooms::resetRoomDistances(distances, 3);

    REQUIRE(distances.size() == 3);
    for (const RoomDistance& distance : distances) {
      CHECK(distance.distance == doctest::Approx(-1.0f));
      CHECK(distance.low_room_portal == kInvalidPortalIndex);
      CHECK(distance.high_room_portal == kInvalidPortalIndex);
    }
  }
}
