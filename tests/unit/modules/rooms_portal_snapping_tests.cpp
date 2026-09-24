// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"

#include "modules/geometry.h"
#include "modules/rooms.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace pistoris;

namespace {

Face face(VertexIndex first, VertexIndex second, VertexIndex third) {
  Face result;
  result.corners[0].vertex = first;
  result.corners[1].vertex = second;
  result.corners[2].vertex = third;
  return result;
}

GeometryData makeGeometry(const std::vector<ArxVector3>& positions, std::vector<Face> faces) {
  GeometryData result;
  result.vertices.reserve(positions.size());
  for (const ArxVector3& position : positions) result.vertices.push_back({position});
  result.faces = std::move(faces);
  return result;
}

Portal portalAtX(float x, std::string name = "portal") {
  Portal result;
  result.name = std::move(name);
  result.room_1 = 0;
  result.room_2 = 1;
  result.vertices = {{{x, 0.0f, -1.0f}, {x, 2.0f, -1.0f}, {x, 2.0f, 2.0f}, {x, 0.0f, 2.0f}}};
  return result;
}

RoomsData makeRooms(std::vector<Portal> portals, std::vector<RoomIndex> face_rooms, std::size_t room_count = 2) {
  RoomsData result;
  for (std::size_t room = 0; room < room_count; ++room) result.definitions.push_back({"room_" + std::to_string(room)});
  result.face_rooms = std::move(face_rooms);
  result.portals = std::move(portals);
  return result;
}

}  // namespace

TEST_SUITE("rooms::portal_snapping") {
  TEST_CASE("Snaps nearby vertices to the bounded portal surface") {
    GeometryData mesh = makeGeometry({{0.25f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    mesh.faces[0].normal = {1.0f, 0.0f, 0.0f};
    const RoomsData room_data = makeRooms({portalAtX(0.0f)}, {0});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(mesh, room_data, {.radius = 0.5f}, &statistics) == rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == ArxVector3{0.0f, 0.5f, 0.5f});
    CHECK(mesh.vertices[1].position == ArxVector3{2.0f, 0.0f, 0.0f});
    CHECK(mesh.faces[0].normal == geometry::faceNormalOr(mesh, mesh.faces[0], {}));
    CHECK(statistics.candidates == 1);
    CHECK(statistics.snapped == 1);
  }

  TEST_CASE("Snaps to a triangle portal") {
    Portal portal = portalAtX(0.0f);
    portal.shape = PortalShape::kTriangle;
    portal.vertices[0] = {0.0f, 0.0f, 0.0f};
    portal.vertices[1] = {0.0f, 2.0f, 0.0f};
    portal.vertices[2] = {0.0f, 0.0f, 2.0f};
    GeometryData mesh = makeGeometry({{0.1f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});

    REQUIRE(rooms::snapGeometryToPortals(mesh, makeRooms({portal}, {0}), {.radius = 0.2f}) == rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == ArxVector3{0.0f, 0.5f, 0.5f});
  }

  TEST_CASE("Snaps at the positive world boundary without coordinate-scaled equality") {
    GeometryData mesh =
        makeGeometry({{15999.99f, 0.5f, 0.5f}, {15998.0f, 0.0f, 0.0f}, {15998.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(mesh, makeRooms({portalAtX(16000.0f)}, {0}), {.radius = 0.1f}, &statistics) ==
            rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == ArxVector3{16000.0f, 0.5f, 0.5f});
    CHECK(statistics.snapped == 1);
    CHECK(statistics.already_aligned == 0);
  }

  TEST_CASE("Enforces small radii against the stored target") {
    GeometryData outside =
        makeGeometry({{0.0005f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics outside_statistics;
    REQUIRE(rooms::snapGeometryToPortals(
                outside, makeRooms({portalAtX(0.0f)}, {0}), {.radius = 0.0001f}, &outside_statistics) ==
            rooms::Error::kNone);
    CHECK(outside.vertices[0].position == ArxVector3{0.0005f, 0.5f, 0.5f});
    CHECK(outside_statistics.candidates == 0);
    CHECK(outside_statistics.snapped == 0);

    GeometryData boundary =
        makeGeometry({{0.0005f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics boundary_statistics;
    REQUIRE(rooms::snapGeometryToPortals(
                boundary, makeRooms({portalAtX(0.0f)}, {0}), {.radius = 0.0005f}, &boundary_statistics) ==
            rooms::Error::kNone);
    CHECK(boundary.vertices[0].position == ArxVector3{0.0f, 0.5f, 0.5f});
    CHECK(boundary_statistics.snapped == 1);
  }

  TEST_CASE("Does not widen a small radius at large world coordinates") {
    const float source_x = std::nextafter(16000.0f, 0.0f);
    GeometryData mesh =
        makeGeometry({{source_x, 0.5f, 0.5f}, {15998.0f, 0.0f, 0.0f}, {15998.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(
                mesh, makeRooms({portalAtX(16000.0f)}, {0}), {.radius = 0.0005f}, &statistics) == rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == ArxVector3{source_x, 0.5f, 0.5f});
    CHECK(statistics.candidates == 0);
    CHECK(statistics.snapped == 0);
  }

  TEST_CASE("Uses the engine portal plane for a mildly non-planar quad") {
    Portal portal = portalAtX(0.0f);
    portal.vertices[2].x = 0.02f;
    GeometryData mesh = makeGeometry({{0.01f, 1.0f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});

    REQUIRE(rooms::snapGeometryToPortals(mesh, makeRooms({portal}, {0}), {.radius = 0.1f}) == rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == ArxVector3{0.0f, 1.0f, 0.5f});
  }

  TEST_CASE("Portal distance uses the same projected surface as snapping") {
    Portal portal = portalAtX(0.0f);
    portal.vertices[2].x = 0.02f;

    CHECK(rooms::pointPortalDistanceSquared({0.0f, 2.0f, 2.0f}, portal) == doctest::Approx(0.0));
  }

  TEST_CASE("Does not extend the portal beyond its boundary") {
    GeometryData mesh = makeGeometry({{0.1f, 3.0f, 0.0f}, {2.0f, 3.0f, 0.0f}, {2.0f, 4.0f, 0.0f}}, {face(0, 1, 2)});
    const GeometryData original = mesh;

    REQUIRE(rooms::snapGeometryToPortals(mesh, makeRooms({portalAtX(0.0f)}, {0}), {.radius = 0.5f}) ==
            rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == original.vertices[0].position);
  }

  TEST_CASE("Rejects vertices shared with an unrelated room") {
    GeometryData mesh = makeGeometry(
        {{0.1f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}, {2.0f, 0.0f, 1.0f}, {2.0f, 1.0f, 1.0f}},
        {face(0, 1, 2), face(0, 3, 4)});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(
                mesh, makeRooms({portalAtX(0.0f)}, {0, 2}, 3), {.radius = 0.5f}, &statistics) == rooms::Error::kNone);
    CHECK(mesh.vertices[0].position == ArxVector3{0.1f, 0.5f, 0.5f});
    CHECK(statistics.skipped_room_conflict == 1);
  }

  TEST_CASE("Accepts equivalent projections and rejects ambiguous ones") {
    GeometryData equivalent =
        makeGeometry({{0.1f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    REQUIRE(rooms::snapGeometryToPortals(equivalent,
                                         makeRooms({portalAtX(0.0f, "portal_0"), portalAtX(0.0f, "portal_1")}, {0}),
                                         {.radius = 0.2f}) == rooms::Error::kNone);
    CHECK(equivalent.vertices[0].position.x == doctest::Approx(0.0f));

    GeometryData ambiguous =
        makeGeometry({{0.0f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics statistics;
    REQUIRE(rooms::snapGeometryToPortals(ambiguous,
                                         makeRooms({portalAtX(-0.1f, "portal_0"), portalAtX(0.1f, "portal_1")}, {0}),
                                         {.radius = 0.2f},
                                         &statistics) == rooms::Error::kNone);
    CHECK(ambiguous.vertices[0].position.x == doctest::Approx(0.0f));
    CHECK(statistics.skipped_ambiguous == 1);
  }

  TEST_CASE("Chooses a meaningfully closer portal") {
    GeometryData mesh = makeGeometry({{0.1f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(mesh,
                                         makeRooms({portalAtX(0.0f, "far"), portalAtX(0.15f, "near")}, {0}),
                                         {.radius = 0.2f},
                                         &statistics) == rooms::Error::kNone);
    CHECK(mesh.vertices[0].position.x == doctest::Approx(0.15f));
    CHECK(statistics.snapped == 1);
    CHECK(statistics.skipped_ambiguous == 0);
  }

  TEST_CASE("Keeps the farthest vertex when snapping a thin face would collapse it") {
    Portal portal;
    portal.name = "portal";
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.vertices = {{{-1.0f, -1.0f, 0.0f}, {2.0f, -1.0f, 0.0f}, {2.0f, 2.0f, 0.0f}, {-1.0f, 2.0f, 0.0f}}};
    GeometryData mesh = makeGeometry({{0.0f, 0.0f, 0.1f}, {1.0f, 0.0f, 0.2f}, {0.0f, 0.0f, 1.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(mesh, makeRooms({portal}, {0}), {.radius = 2.0f}, &statistics) ==
            rooms::Error::kNone);
    CHECK(mesh.vertices[0].position.z == doctest::Approx(0.0f));
    CHECK(mesh.vertices[1].position.z == doctest::Approx(0.0f));
    CHECK(mesh.vertices[2].position.z == doctest::Approx(1.0f));
    CHECK(mesh.faces.size() == 1);
    CHECK(statistics.snapped == 2);
    CHECK(statistics.skipped_face_safety == 1);
  }

  TEST_CASE("Rejects a move that reverses an incident face") {
    GeometryData mesh = makeGeometry({{0.9f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    rooms::PortalSnapStatistics statistics;

    REQUIRE(rooms::snapGeometryToPortals(mesh, makeRooms({portalAtX(1.1f)}, {0}), {.radius = 0.3f}, &statistics) ==
            rooms::Error::kNone);
    CHECK(mesh.vertices[0].position.x == doctest::Approx(0.9f));
    CHECK(statistics.skipped_face_safety == 1);
  }

  TEST_CASE("Rejects invalid options without mutating geometry or statistics") {
    GeometryData mesh = makeGeometry({{0.1f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 2)});
    const GeometryData original = mesh;
    rooms::PortalSnapStatistics statistics{.candidates = 7};

    CHECK(rooms::snapGeometryToPortals(mesh,
                                       makeRooms({portalAtX(0.0f)}, {0}),
                                       {.radius = std::numeric_limits<float>::quiet_NaN()},
                                       &statistics) == rooms::Error::kInvalidOptions);
    CHECK(mesh.vertices[0].position == original.vertices[0].position);
    CHECK(statistics.candidates == 7);
  }

  TEST_CASE("Rejects invalid face references without partial output") {
    GeometryData mesh = makeGeometry({{0.1f, 0.5f, 0.5f}, {2.0f, 0.0f, 0.0f}, {2.0f, 1.0f, 0.0f}}, {face(0, 1, 3)});
    const GeometryData original = mesh;
    rooms::PortalSnapStatistics statistics{.candidates = 7};

    CHECK(rooms::snapGeometryToPortals(mesh, makeRooms({portalAtX(0.0f)}, {0}), {}, &statistics) ==
          rooms::Error::kBadFaceVertex);
    CHECK(mesh.vertices[0].position == original.vertices[0].position);
    CHECK(statistics.candidates == 7);
  }
}
