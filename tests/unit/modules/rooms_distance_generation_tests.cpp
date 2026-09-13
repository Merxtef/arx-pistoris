// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/pistoris.hpp"

#include "modules/geometry.h"
#include "modules/rooms.h"

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using namespace pistoris;

namespace {

struct LogCapture {
  std::vector<std::string> messages;

  bool contains(std::string_view text) const {
    for (const std::string& message : messages) {
      if (message.find(text) != std::string::npos) return true;
    }
    return false;
  }
};

void captureLog(ArxLogLevel level, const char* message, void* userdata) {
  if (level != ARX_LOG_WARN || !message) return;
  static_cast<LogCapture*>(userdata)->messages.emplace_back(message);
}

Face makeFace(GeometryData&, VertexIndex a, VertexIndex b, VertexIndex c,
              const ArxVector3& normal = {0.0f, -1.0f, 0.0f}) {
  Face face;
  face.corners[0].vertex = a;
  face.corners[1].vertex = b;
  face.corners[2].vertex = c;
  for (Corner& corner : face.corners) corner.normal = normal;
  return face;
}

void addFloor(GeometryData& geometry, RoomsData& rooms, RoomIndex room, float min_x, float max_x, float min_z,
              float max_z) {
  const VertexIndex base = static_cast<VertexIndex>(geometry.vertices.size());
  geometry.vertices.push_back({{min_x, 0.0f, min_z}});
  geometry.vertices.push_back({{max_x, 0.0f, min_z}});
  geometry.vertices.push_back({{max_x, 0.0f, max_z}});
  geometry.vertices.push_back({{min_x, 0.0f, max_z}});
  geometry.faces.push_back(makeFace(geometry, base + 0, base + 1, base + 2));
  geometry.faces.push_back(makeFace(geometry, base + 0, base + 2, base + 3));
  rooms.face_rooms.push_back(room);
  rooms.face_rooms.push_back(room);
}

Portal makePortal(const char* name, RoomIndex first, RoomIndex second, float x, float z) {
  Portal portal;
  portal.name = name;
  portal.room_1 = first;
  portal.room_2 = second;
  portal.shape = PortalShape::kQuad;
  portal.vertices = {{
      {x - 5.0f, -10.0f, z},
      {x + 5.0f, -10.0f, z},
      {x + 5.0f, 10.0f, z},
      {x - 5.0f, 10.0f, z},
  }};
  return portal;
}

void addThreeRoomGeometry(GeometryData& geometry, RoomsData& rooms) {
  rooms.definitions = {{"room_1"}, {"room_2"}, {"room_3"}};
  addFloor(geometry, rooms, 0, -120.0f, 40.0f, -60.0f, 60.0f);
  addFloor(geometry, rooms, 1, -40.0f, 120.0f, -60.0f, 60.0f);
  addFloor(geometry, rooms, 2, 40.0f, 200.0f, -60.0f, 60.0f);
}

}  // namespace

TEST_SUITE("rooms::distance_generation") {
  TEST_CASE("Validates effective room distance options") {
    CHECK(rooms::validRoomDistanceOptions({}));

    rooms::RoomDistanceOptions options;
    options.sample_spacing = 0.0f;
    CHECK_FALSE(rooms::validRoomDistanceOptions(options));

    options = {};
    options.max_link_distance = 20.0f;
    CHECK_FALSE(rooms::validRoomDistanceOptions(options));
  }

  TEST_CASE("Generates direct and indirect room distances") {
    GeometryData geometry;
    RoomsData rooms;
    addThreeRoomGeometry(geometry, rooms);
    rooms.portals.push_back(makePortal("portal_1_2", 0, 1, 0.0f, 0.0f));
    rooms.portals.push_back(makePortal("portal_2_3", 1, 2, 80.0f, 0.0f));
    RoomDistances distances;
    rooms::RoomDistanceGenerationDiagnostics diagnostics;

    CHECK(rooms::generateRoomDistances(distances, rooms, geometry, {}, &diagnostics) == rooms::Error::kNone);

    REQUIRE(distances.size() == 3);
    const std::size_t direct_1_2 = rooms::roomDistancePairIndex(0, 1);
    const std::size_t indirect = rooms::roomDistancePairIndex(0, 2);
    const std::size_t direct_2_3 = rooms::roomDistancePairIndex(1, 2);
    CHECK(distances[direct_1_2].distance == doctest::Approx(-1.0f));
    CHECK(distances[direct_1_2].low_room_portal == 0);
    CHECK(distances[direct_1_2].high_room_portal == 0);
    CHECK(distances[direct_2_3].distance == doctest::Approx(-1.0f));
    CHECK(distances[direct_2_3].low_room_portal == 1);
    CHECK(distances[direct_2_3].high_room_portal == 1);

    const float expected_indirect = std::sqrt(80.0f * 80.0f + 20.0f * 20.0f) + 20.0f;
    CHECK(distances[indirect].distance == doctest::Approx(expected_indirect));
    CHECK(distances[indirect].low_room_portal == 0);
    CHECK(distances[indirect].high_room_portal == 1);
    REQUIRE(diagnostics.room_pair_paths.size() == 1);
    CHECK(diagnostics.room_pair_paths[0].room_1 == 0);
    CHECK(diagnostics.room_pair_paths[0].room_2 == 2);
  }

  TEST_CASE("Generates fallback distances for unreachable rooms") {
    GeometryData geometry;
    RoomsData rooms;
    addThreeRoomGeometry(geometry, rooms);
    rooms.portals.push_back(makePortal("portal_1_2", 0, 1, 0.0f, 0.0f));
    RoomDistances distances;

    CHECK(rooms::generateRoomDistances(distances, rooms, geometry, {}) == rooms::Error::kNone);

    const std::size_t unreachable = rooms::roomDistancePairIndex(0, 2);
    CHECK(distances[unreachable].distance == doctest::Approx(-1.0f));
    CHECK(distances[unreachable].low_room_portal == kInvalidPortalIndex);
    CHECK(distances[unreachable].high_room_portal == kInvalidPortalIndex);
  }

  TEST_CASE("Warns once for disconnected in-room portal groups") {
    GeometryData geometry;
    RoomsData rooms;
    addThreeRoomGeometry(geometry, rooms);
    rooms.portals.push_back(makePortal("portal_1_2", 0, 1, 0.0f, 0.0f));
    rooms.portals.push_back(makePortal("portal_2_3", 1, 2, 400.0f, 0.0f));
    rooms::RoomDistanceOptions options;
    options.max_link_distance = 120.0f;
    RoomDistances distances;
    LogCapture logs;
    pistoris::setLogCallback(captureLog, &logs);

    const rooms::Error rc = rooms::generateRoomDistances(distances, rooms, geometry, options);

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(rc == rooms::Error::kNone);
    CHECK(logs.contains("disconnected in-room portal graph in 1 room(s): 'room_2' (2 groups)"));
  }

  TEST_CASE("Rejects invalid inputs before generation") {
    GeometryData geometry;
    RoomsData rooms;
    addThreeRoomGeometry(geometry, rooms);
    rooms::RoomDistanceOptions options;
    options.sample_spacing = 0.0f;
    RoomDistances distances;
    CHECK(rooms::generateRoomDistances(distances, rooms, geometry, options) == rooms::Error::kInvalidOptions);
  }
}
