// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/pistoris.hpp"

#include "level/data.h"
#include "level_add_helpers.h"
#include "modules/geometry.h"
#include "modules/rooms.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace {

using pistoris::ArxVector3;

struct LogCapture {
  std::vector<std::string> messages;

  bool contains(std::string_view text) const {
    for (const std::string& message : messages) {
      if (message.find(text) != std::string::npos) return true;
    }
    return false;
  }
};

void captureLog(ArxLogLevel level, const char* msg, void* userdata) {
  if (level != ARX_LOG_WARN || !msg) return;
  static_cast<LogCapture*>(userdata)->messages.emplace_back(msg);
}

pistoris::Face makeFace(pistoris::GeometryData&, std::uint32_t a, std::uint32_t b, std::uint32_t c,
                        std::uint32_t room) {
  constexpr ArxVector3 kNormal{0.0f, -1.0f, 0.0f};
  (void)room;
  return {
      {{{a, kNormal, 0.0f, 0.0f}, {b, kNormal, 0.0f, 0.0f}, {c, kNormal, 0.0f, 0.0f}}}, pistoris::kNoTexture, 0, 0.0f};
}

void addFloor(pistoris::LevelModules& level, float min_x, float max_x, float min_z, float max_z, std::uint32_t room) {
  const std::uint32_t base = static_cast<std::uint32_t>(level.geometry.vertices.size());
  level.geometry.vertices.push_back({{min_x, 0.0f, min_z}});
  level.geometry.vertices.push_back({{max_x, 0.0f, min_z}});
  level.geometry.vertices.push_back({{max_x, 0.0f, max_z}});
  level.geometry.vertices.push_back({{min_x, 0.0f, max_z}});
  level.geometry.faces.push_back(makeFace(level.geometry, base + 0, base + 1, base + 2, room));
  level.rooms.face_rooms.push_back(room);
  level.geometry.faces.push_back(makeFace(level.geometry, base + 0, base + 2, base + 3, room));
  level.rooms.face_rooms.push_back(room);
}

void addCeiling(pistoris::LevelModules& level, float min_x, float max_x, float min_z, float max_z, float y,
                std::uint32_t room) {
  const std::uint32_t base = static_cast<std::uint32_t>(level.geometry.vertices.size());
  level.geometry.vertices.push_back({{min_x, y, min_z}});
  level.geometry.vertices.push_back({{max_x, y, min_z}});
  level.geometry.vertices.push_back({{max_x, y, max_z}});
  level.geometry.vertices.push_back({{min_x, y, max_z}});
  level.geometry.faces.push_back(makeFace(level.geometry, base + 0, base + 2, base + 1, room));
  level.rooms.face_rooms.push_back(room);
  level.geometry.faces.push_back(makeFace(level.geometry, base + 0, base + 3, base + 2, room));
  level.rooms.face_rooms.push_back(room);
}

void addPortal(pistoris::LevelModules& level, const char* name, float x, float min_z, float max_z, std::uint32_t room_1,
               std::uint32_t room_2) {
  pistoris::Portal portal;
  portal.name = name;
  portal.room_1 = room_1;
  portal.room_2 = room_2;
  portal.shape = pistoris::PortalShape::kQuad;
  portal.vertices = {{{x, 0.0f, min_z}, {x, -100.0f, min_z}, {x, -100.0f, max_z}, {x, 0.0f, max_z}}};
  level.rooms.portals.push_back(portal);
}

pistoris::LevelModules makeTwoRoomLevel() {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room_1"}, {"room_2"}};
  addFloor(level, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addFloor(level, 200.0f, 400.0f, 0.0f, 200.0f, 1);
  addPortal(level, "door_1", 200.0f, 25.0f, 75.0f, 0, 1);
  addPortal(level, "door_2", 200.0f, 125.0f, 175.0f, 0, 1);
  return level;
}

pistoris::LevelModules makeThreeRoomLevel() {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room_1"}, {"room_2"}, {"room_3"}};
  addFloor(level, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addFloor(level, 200.0f, 400.0f, 0.0f, 200.0f, 1);
  addFloor(level, 400.0f, 600.0f, 0.0f, 200.0f, 2);
  addPortal(level, "door_1", 200.0f, 50.0f, 150.0f, 0, 1);
  addPortal(level, "door_2", 400.0f, 50.0f, 150.0f, 1, 2);
  return level;
}

pistoris::Level makePublicLevel(const pistoris::LevelModules& src) {
  pistoris::Level level;
  for (const pistoris::Room& room : src.rooms.definitions) {
    const pistoris::RoomIndex expected = static_cast<pistoris::RoomIndex>(level.roomCount());
    REQUIRE(test::addRoom(level, room) == expected);
  }

  test::MeshSnapshot mesh;
  mesh.vertices = src.geometry.vertices;
  mesh.faces = src.geometry.faces;
  mesh.textures = src.textures.textures;
  mesh.face_rooms = src.rooms.face_rooms;
  mesh.corner_colors = src.lighting.corner_colors;
  REQUIRE(test::replaceMesh(level, mesh) == ARX_OK);

  for (const pistoris::Portal& portal : src.rooms.portals) {
    const pistoris::PortalIndex expected = static_cast<pistoris::PortalIndex>(level.portalCount());
    REQUIRE(test::addPortal(level, portal) == expected);
  }

  return pistoris::Level(level);
}

ArxReturnCode generateRoomDistances(pistoris::LevelModules& modules,
                                    const pistoris::Level::RoomDistanceGenOptions& options = {},
                                    pistoris::level_debug::RoomDistanceGenDiagnostics* diagnostics = nullptr) {
  pistoris::Level level = makePublicLevel(modules);
  ArxReturnCode rc = diagnostics ? pistoris::level_debug::generateRoomDistances(level, options, *diagnostics)
                                 : level.generateRoomDistances(options);
  if (rc == ARX_OK) {
    modules.rooms.distances = test::roomDistances(level);
  }
  return rc;
}

}  // namespace

TEST_CASE("RoomDistanceDiagnosticsBuildsPortalSamplesAndVisibility") {
  pistoris::LevelModules level = makeTwoRoomLevel();
  pistoris::level_debug::RoomDistanceGenDiagnostics diagnostics;

  REQUIRE(generateRoomDistances(level,
                                {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f},
                                &diagnostics) == ARX_OK);

  REQUIRE(diagnostics.support_by_room.size() == 2);
  CHECK(!diagnostics.support_by_room[0].empty());
  CHECK(!diagnostics.support_by_room[1].empty());
  REQUIRE(diagnostics.portal_access_points_by_room.size() == 2);
  REQUIRE(diagnostics.portal_access_points_by_room[0].size() == 2);
  REQUIRE(diagnostics.portal_access_points_by_room[1].size() == 2);
  CHECK(diagnostics.portal_access_points_by_room[0][0].position.x == doctest::Approx(190.0f));
  CHECK(diagnostics.portal_access_points_by_room[1][0].position.x == doctest::Approx(210.0f));
  REQUIRE(diagnostics.sampled_points_by_room.size() == 2);
  CHECK(diagnostics.sampled_points_by_room[0].size() == 4);
  CHECK(diagnostics.sampled_points_by_room[1].size() == 4);
  CHECK(diagnostics.portal_access_segments.size() == 4);
  CHECK(!diagnostics.in_room_visibility_edges.empty());
  REQUIRE(diagnostics.in_room_portal_paths_by_room.size() == 2);
  CHECK(diagnostics.in_room_portal_paths_by_room[0].size() == 1);
  CHECK(diagnostics.in_room_portal_paths_by_room[1].size() == 1);
}

TEST_CASE("RoomDistanceDiagnosticsSkipsSamplingForRoomsWithLessThanTwoPortals") {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room_1"}, {"room_2"}};
  addFloor(level, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addFloor(level, 200.0f, 400.0f, 0.0f, 200.0f, 1);
  addPortal(level, "door", 200.0f, 50.0f, 150.0f, 0, 1);
  pistoris::level_debug::RoomDistanceGenDiagnostics diagnostics;

  REQUIRE(generateRoomDistances(level,
                                {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f},
                                &diagnostics) == ARX_OK);

  REQUIRE(diagnostics.portal_access_points_by_room.size() == 2);
  CHECK(diagnostics.portal_access_points_by_room[0].size() == 1);
  CHECK(diagnostics.portal_access_points_by_room[1].size() == 1);
  REQUIRE(diagnostics.sampled_points_by_room.size() == 2);
  CHECK(diagnostics.sampled_points_by_room[0].empty());
  CHECK(diagnostics.sampled_points_by_room[1].empty());
}

TEST_CASE("RoomDistanceDiagnosticsOffsetsCeilingSamplesDownward") {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room"}, {"other"}};
  addFloor(level, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addCeiling(level, 0.0f, 200.0f, 0.0f, 200.0f, -100.0f, 0);
  addPortal(level, "door_1", 0.0f, 25.0f, 75.0f, 0, 1);
  addPortal(level, "door_2", 200.0f, 125.0f, 175.0f, 0, 1);
  pistoris::level_debug::RoomDistanceGenDiagnostics diagnostics;

  REQUIRE(generateRoomDistances(level,
                                {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f},
                                &diagnostics) == ARX_OK);

  REQUIRE(diagnostics.sampled_points_by_room.size() == 2);
  CHECK(diagnostics.sampled_points_by_room[0].size() == 8);
  CHECK(diagnostics.sampled_points_by_room[1].empty());
  std::uint32_t floor_samples = 0;
  std::uint32_t ceiling_samples = 0;
  for (const auto& point : diagnostics.sampled_points_by_room[0]) {
    if (point.position.y == doctest::Approx(-60.0f)) ++floor_samples;
    if (point.position.y == doctest::Approx(-40.0f)) ++ceiling_samples;
  }
  CHECK(floor_samples == 4);
  CHECK(ceiling_samples == 4);
}

TEST_CASE("RoomDistanceGenerationBuildsGlobalDistancesThroughPortalGraph") {
  pistoris::LevelModules level = makeThreeRoomLevel();

  REQUIRE(generateRoomDistances(level,
                                {.portal_side_offset = 10.0f,
                                 .sample_spacing = 100.0f,
                                 .sample_height_offset = 60.0f,
                                 .max_link_distance = 300.0f}) == ARX_OK);

  REQUIRE(pistoris::rooms::hasCompleteRoomDistances(level.rooms.distances, level.rooms.definitions.size()));
  const std::size_t direct_0_1 = pistoris::rooms::roomDistancePairIndex(0, 1);
  const std::size_t indirect_0_2 = pistoris::rooms::roomDistancePairIndex(0, 2);
  const std::size_t direct_1_2 = pistoris::rooms::roomDistancePairIndex(1, 2);
  CHECK(level.rooms.distances[direct_0_1].distance == doctest::Approx(-1.0f));
  CHECK(level.rooms.distances[direct_1_2].distance == doctest::Approx(-1.0f));
  CHECK(level.rooms.distances[indirect_0_2].distance == doctest::Approx(200.0f));
  CHECK(level.rooms.distances[direct_0_1].low_room_portal == 0);
  CHECK(level.rooms.distances[direct_0_1].high_room_portal == 0);
  CHECK(level.rooms.distances[indirect_0_2].low_room_portal == 0);
  CHECK(level.rooms.distances[indirect_0_2].high_room_portal == 1);
}

TEST_CASE("RoomDistanceGenerationRejectsInvalidOptions") {
  pistoris::LevelModules level = makeThreeRoomLevel();

  CHECK(generateRoomDistances(level, {.sample_spacing = 19.0f}) == ARX_INVALID_OPTIONS);
  CHECK(generateRoomDistances(level, {.portal_side_offset = 0.0f}) == ARX_INVALID_OPTIONS);
  CHECK(generateRoomDistances(level, {.sample_height_offset = 49.0f}) == ARX_INVALID_OPTIONS);
  CHECK(generateRoomDistances(level, {.sample_spacing = 100.0f, .max_link_distance = 109.0f}) == ARX_INVALID_OPTIONS);
}

TEST_CASE("RoomDistanceGenerationWarnsOnceForUnreachableRooms") {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room_1"}, {"room_2"}, {"unused"}};
  addFloor(level, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addFloor(level, 200.0f, 400.0f, 0.0f, 200.0f, 1);
  addFloor(level, 500.0f, 700.0f, 0.0f, 200.0f, 2);
  addPortal(level, "door", 200.0f, 50.0f, 150.0f, 0, 1);
  LogCapture logs;
  pistoris::setLogCallback(captureLog, &logs);

  const ArxReturnCode rc = generateRoomDistances(
      level, {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f});

  pistoris::setLogCallback(nullptr, nullptr);
  REQUIRE(rc == ARX_OK);
  CHECK(logs.contains("unreachable room(s): unused"));
  CHECK_FALSE(logs.contains("failed for room pair"));
}

TEST_CASE("RoomDistanceGenerationWarnsOnceForDisconnectedRoomGraph") {
  pistoris::LevelModules level;
  level.rooms.definitions = {{"room_1"}, {"room_2"}, {"room_3"}, {"room_4"}};
  addFloor(level, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addFloor(level, 200.0f, 400.0f, 0.0f, 200.0f, 1);
  addFloor(level, 500.0f, 700.0f, 0.0f, 200.0f, 2);
  addFloor(level, 700.0f, 900.0f, 0.0f, 200.0f, 3);
  addPortal(level, "door_1", 200.0f, 50.0f, 150.0f, 0, 1);
  addPortal(level, "door_2", 700.0f, 50.0f, 150.0f, 2, 3);
  LogCapture logs;
  pistoris::setLogCallback(captureLog, &logs);

  const ArxReturnCode rc = generateRoomDistances(
      level, {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f});

  pistoris::setLogCallback(nullptr, nullptr);
  REQUIRE(rc == ARX_OK);
  CHECK(logs.contains("disconnected room graph: [room_1, room_2] [room_3, room_4]"));
  CHECK_FALSE(logs.contains("failed for room pair"));
}

TEST_CASE("RoomDistanceDebugGlbDoesNotRegenerateDiagnostics") {
  pistoris::LevelModules src;
  src.rooms.definitions = {{"room_1"}, {"room_2"}, {"unused"}};
  addFloor(src, 0.0f, 200.0f, 0.0f, 200.0f, 0);
  addFloor(src, 200.0f, 400.0f, 0.0f, 200.0f, 1);
  addFloor(src, 500.0f, 700.0f, 0.0f, 200.0f, 2);
  addPortal(src, "door", 200.0f, 50.0f, 150.0f, 0, 1);
  pistoris::Level level = makePublicLevel(src);
  pistoris::level_debug::RoomDistanceGenDiagnostics diagnostics;
  LogCapture logs;
  pistoris::setLogCallback(captureLog, &logs);

  REQUIRE(pistoris::level_debug::generateRoomDistances(
              level,
              {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f},
              diagnostics) == ARX_OK);
  REQUIRE(diagnostics.support_by_room.size() == 3);
  std::vector<std::uint8_t> glb;
  REQUIRE(pistoris::level_debug::exportRoomDistanceDebugGlb(level, glb, &diagnostics) == ARX_OK);

  pistoris::setLogCallback(nullptr, nullptr);
  std::uint32_t unreachable_warnings = 0;
  for (const std::string& message : logs.messages) {
    if (message.find("unreachable room(s): unused") != std::string::npos) ++unreachable_warnings;
  }
  CHECK(unreachable_warnings == 1);
}

TEST_CASE("RoomDistanceDebugGlbExportsDiagnosticGroups") {
  pistoris::LevelModules src = makeThreeRoomLevel();
  pistoris::Level level = makePublicLevel(src);
  pistoris::level_debug::RoomDistanceGenDiagnostics diagnostics;
  REQUIRE(pistoris::level_debug::generateRoomDistances(
              level,
              {.portal_side_offset = 10.0f, .sample_spacing = 100.0f, .sample_height_offset = 60.0f},
              diagnostics) == ARX_OK);
  REQUIRE(diagnostics.support_by_room.size() == 3);
  std::vector<std::uint8_t> glb;

  REQUIRE(pistoris::level_debug::exportRoomDistanceDebugGlb(level, glb, &diagnostics) == ARX_OK);

  const std::string text(reinterpret_cast<const char*>(glb.data()), glb.size());
  CHECK(text.find("arx_navigation_support") != std::string::npos);
  CHECK(text.find("room_distance_debug_room_support") != std::string::npos);
  CHECK(text.find("room_distance_debug_portal") != std::string::npos);
  CHECK(text.find("room_distance_debug_portal_planes") != std::string::npos);
  CHECK(text.find("room_distance_debug_portal_access_points") != std::string::npos);
  CHECK(text.find("room_distance_debug_sampled_points") != std::string::npos);
  CHECK(text.find("room_distance_debug_in_room_visibility_edges") != std::string::npos);
  CHECK(text.find("room_distance_debug_in_room_portal_paths") != std::string::npos);
  CHECK(text.find("room_distance_debug_room_pair_paths") != std::string::npos);
}
