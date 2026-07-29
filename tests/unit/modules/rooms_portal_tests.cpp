// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"

#include "modules/rooms.h"

#include <cmath>
#include <limits>

using namespace pistoris;

namespace {

Portal makeTrianglePortal() {
  Portal portal;
  portal.name = "portal";
  portal.room_1 = 0;
  portal.room_2 = 1;
  portal.shape = PortalShape::kTriangle;
  portal.vertices = {{
      {0.0f, 0.0f, 0.0f},
      {1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f},
      {},
  }};
  return portal;
}

Portal makeQuadPortal() {
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

}  // namespace

TEST_SUITE("rooms::portal") {
  TEST_CASE("Reports active portal vertex count") {
    CHECK(rooms::portalVertexCount(PortalShape::kTriangle) == 3);
    CHECK(rooms::portalVertexCount(PortalShape::kQuad) == 4);
    CHECK(rooms::portalVertexCount(static_cast<PortalShape>(5)) == 0);
  }

  TEST_CASE("Computes centroid") {
    Portal triangle = makeTrianglePortal();
    ArxVector3 centroid = rooms::portalCentroid(triangle);
    CHECK(centroid.x == doctest::Approx(1.0f / 3.0f));
    CHECK(centroid.y == doctest::Approx(1.0f / 3.0f));
    CHECK(centroid.z == doctest::Approx(0.0f));

    Portal quad = makeQuadPortal();
    centroid = rooms::portalCentroid(quad);
    CHECK(centroid.x == doctest::Approx(0.5f));
    CHECK(centroid.y == doctest::Approx(0.5f));
    CHECK(centroid.z == doctest::Approx(0.0f));
  }

  TEST_CASE("Computes normal") {
    Portal triangle = makeTrianglePortal();
    ArxVector3 normal = rooms::portalNormal(triangle);
    CHECK(normal.x == doctest::Approx(0.0f));
    CHECK(normal.y == doctest::Approx(0.0f));
    CHECK(normal.z == doctest::Approx(1.0f));

    Portal quad = makeQuadPortal();
    normal = rooms::portalNormal(quad);
    CHECK(normal.x == doctest::Approx(0.0f));
    CHECK(normal.y == doctest::Approx(0.0f));
    CHECK(normal.z == doctest::Approx(1.0f));
  }

  TEST_CASE("Reports side rooms and room connection") {
    Portal portal = makeQuadPortal();

    CHECK(rooms::portalSideRoom(portal, true) == 0);
    CHECK(rooms::portalSideRoom(portal, false) == 1);
    CHECK(rooms::connectsRooms(portal, 0, 1));
    CHECK(rooms::connectsRooms(portal, 1, 0));
    CHECK_FALSE(rooms::connectsRooms(portal, 0, 2));
  }

  TEST_CASE("Validates triangle and quad geometry") {
    CHECK(rooms::validatePortalGeometry(makeTrianglePortal()) == rooms::PortalValidation::kValid);
    CHECK(rooms::validatePortalGeometry(makeQuadPortal()) == rooms::PortalValidation::kValid);
  }

  TEST_CASE("Rejects invalid shape") {
    Portal portal = makeTrianglePortal();
    portal.shape = static_cast<PortalShape>(5);

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kBadShape);
  }

  TEST_CASE("Rejects nonfinite active vertices") {
    Portal portal = makeTrianglePortal();
    portal.vertices[0].x = std::numeric_limits<float>::infinity();

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kBadVertex);
  }

  TEST_CASE("Rejects duplicate active vertices") {
    Portal portal = makeTrianglePortal();
    portal.vertices[2] = portal.vertices[1];

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kBadVertex);
  }

  TEST_CASE("Rejects degenerate triangle") {
    Portal portal = makeTrianglePortal();
    portal.vertices[2] = {2.0f, 0.0f, 0.0f};

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kDegenerate);
  }

  TEST_CASE("Rejects degenerate quad half") {
    Portal portal = makeQuadPortal();
    portal.vertices[2] = {2.0f, -1.0f, 0.0f};

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kDegenerate);
  }

  TEST_CASE("Uses scale-relative quad planarity tolerance capped at five units") {
    Portal portal = makeQuadPortal();
    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {200.0f, 0.0f, 0.0f}, {200.0f, 300.0f, 3.0f}, {0.0f, 300.0f, 0.0f}}};

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kValid);

    portal.vertices[2].z = 3.5f;
    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kNonPlanar);

    portal.vertices = {{{0.0f, 0.0f, 0.0f}, {1000.0f, 0.0f, 0.0f}, {1000.0f, 1000.0f, 5.0f}, {0.0f, 1000.0f, 0.0f}}};
    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kValid);

    portal.vertices[2].z = std::nextafter(5.0f, std::numeric_limits<float>::infinity());
    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kNonPlanar);
  }

  TEST_CASE("Rejects self-crossing quad") {
    Portal portal = makeQuadPortal();
    portal.vertices = {{
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
    }};

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kSelfIntersecting);
  }

  TEST_CASE("Rejects inconsistent quad orientation") {
    Portal portal = makeQuadPortal();
    portal.vertices = {{
        {0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.2f, 0.2f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    }};

    CHECK(rooms::validatePortalGeometry(portal) == rooms::PortalValidation::kInconsistentOrientation);
  }
}
