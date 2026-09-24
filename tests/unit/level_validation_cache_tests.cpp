// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"

#include "level/data.h"
#include "level/validation.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"

#include <array>

namespace {

pistoris::LevelModules validLevelModules() {
  using namespace pistoris;

  LevelModules modules;
  modules.geometry.vertices = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
  constexpr ArxVector3 kNormal = {0.0f, -1.0f, 0.0f};
  Face face;
  face.corners[0] = {0, kNormal, 0.0f, 0.0f};
  face.corners[1] = {1, kNormal, 1.0f, 0.0f};
  face.corners[2] = {2, kNormal, 0.0f, 1.0f};
  modules.geometry.faces.push_back(face);
  modules.rooms.definitions.push_back({"room"});
  modules.rooms.face_rooms.push_back(0);
  return modules;
}

}  // namespace

TEST_SUITE("Level validation cache") {
  TEST_CASE("Module error mappings preserve Level return-code domains") {
    using namespace pistoris;

    CHECK(level_validation::geometryError(geometry::Error::kTooManyVertices) == ARX_LEVEL_TOO_MANY_VERTICES);
    CHECK(level_validation::textureError(textures::Error::kTooManyTextures) == ARX_LEVEL_TOO_MANY_TEXTURES);
    CHECK(level_validation::textureError(textures::Error::kDuplicateTexture) == ARX_LEVEL_BAD_TEXTURE_PATH);
    CHECK(level_validation::geometryError(geometry::Error::kTooManyFaces) == ARX_LEVEL_TOO_MANY_FACES);
    CHECK(level_validation::geometryError(geometry::Error::kBadVertexWeldSegment) == ARX_LEVEL_BAD_VERTEX_WELD_SEGMENT);
    CHECK(level_validation::geometryError(geometry::Error::kOverlappingVertexWeldSegments) ==
          ARX_LEVEL_OVERLAPPING_VERTEX_WELD_SEGMENTS);

    CHECK(level_validation::roomsError(rooms::Error::kNoRooms) == ARX_LEVEL_NO_ROOMS);
    CHECK(level_validation::roomsError(rooms::Error::kTooManyVertices) == ARX_LEVEL_TOO_MANY_VERTICES);
    CHECK(level_validation::roomsError(rooms::Error::kTooManyFaces) == ARX_LEVEL_TOO_MANY_FACES);
    CHECK(level_validation::roomsError(rooms::Error::kTooManyRooms) == ARX_LEVEL_TOO_MANY_ROOMS);
    CHECK(level_validation::roomsError(rooms::Error::kBadRoomName) == ARX_LEVEL_BAD_ROOM_NAME);
    CHECK(level_validation::roomsError(rooms::Error::kDuplicateRoomName) == ARX_LEVEL_DUPLICATE_ROOM_NAME);
    CHECK(level_validation::roomsError(rooms::Error::kBadFaceRoomCount) == ARX_LEVEL_BAD_FACE_ROOM_COUNT);
    CHECK(level_validation::roomsError(rooms::Error::kTooManyPortals) == ARX_LEVEL_TOO_MANY_PORTALS);
    CHECK(level_validation::roomsError(rooms::Error::kBadPortalName) == ARX_LEVEL_BAD_PORTAL_NAME);
    CHECK(level_validation::roomsError(rooms::Error::kDuplicatePortalName) == ARX_LEVEL_DUPLICATE_PORTAL_NAME);
    CHECK(level_validation::roomsError(rooms::Error::kInconsistentPortalOrientation) ==
          ARX_LEVEL_INCONSISTENT_PORTAL_ORIENTATION);

    CHECK(level_validation::navigationError(navigation::Error::kBadSurface) == ARX_LEVEL_BAD_NAV_SURFACE);
    CHECK(level_validation::navigationError(navigation::Error::kTooManySurfaceVertices) ==
          ARX_LEVEL_TOO_MANY_NAV_SURFACE_VERTICES);
    CHECK(level_validation::navigationError(navigation::Error::kBadSurfaceVertex) == ARX_LEVEL_BAD_NAV_SURFACE_VERTEX);
    CHECK(level_validation::navigationError(navigation::Error::kDegenerateSurfaceTriangle) ==
          ARX_LEVEL_DEGENERATE_NAV_SURFACE_TRIANGLE);
    CHECK(level_validation::navigationError(navigation::Error::kSurfaceRequired) == ARX_LEVEL_NAV_SURFACE_REQUIRED);
    CHECK(level_validation::navigationError(navigation::Error::kTooManyAnchors) == ARX_LEVEL_TOO_MANY_ANCHORS);
    CHECK(level_validation::navigationError(navigation::Error::kBadAnchorName) == ARX_LEVEL_BAD_ANCHOR_NAME);
    CHECK(level_validation::navigationError(navigation::Error::kDuplicateAnchorName) ==
          ARX_LEVEL_DUPLICATE_ANCHOR_NAME);
    CHECK(level_validation::navigationError(navigation::Error::kBadAnchorPosition) == ARX_LEVEL_BAD_ANCHOR_POSITION);
    CHECK(level_validation::navigationError(navigation::Error::kTooManyConnections) ==
          ARX_LEVEL_TOO_MANY_ANCHOR_CONNECTIONS);

    CHECK(level_validation::lightingError(lights::Error::kTooManyLights) == ARX_LEVEL_TOO_MANY_LIGHTS);
    CHECK(level_validation::lightingError(lights::Error::kDuplicateLightName) == ARX_LEVEL_DUPLICATE_LIGHT_NAME);
    CHECK(level_validation::lightingError(lights::Error::kBadCornerColorCount) == ARX_LEVEL_BAD_CORNER_COLOR_COUNT);

    CHECK(level_validation::sceneError(scene::Error::kTooManyEntities) == ARX_LEVEL_TOO_MANY_ENTITIES);
    CHECK(level_validation::sceneError(scene::Error::kBadEntityName) == ARX_LEVEL_BAD_ENTITY_NAME);
    CHECK(level_validation::sceneError(scene::Error::kDuplicateEntityName) == ARX_LEVEL_DUPLICATE_ENTITY_NAME);
    CHECK(level_validation::sceneError(scene::Error::kTooManyFogs) == ARX_LEVEL_TOO_MANY_FOGS);
    CHECK(level_validation::sceneError(scene::Error::kBadFogName) == ARX_LEVEL_BAD_FOG_NAME);
    CHECK(level_validation::sceneError(scene::Error::kDuplicateFogName) == ARX_LEVEL_DUPLICATE_FOG_NAME);
    CHECK(level_validation::sceneError(scene::Error::kBadFogRotation) == ARX_LEVEL_BAD_FOG_ROTATION);
    CHECK(level_validation::sceneError(scene::Error::kBadFogColor) == ARX_LEVEL_BAD_FOG_COLOR);
    CHECK(level_validation::sceneError(scene::Error::kTooManyZones) == ARX_LEVEL_TOO_MANY_ZONES);
    CHECK(level_validation::sceneError(scene::Error::kDuplicateZoneName) == ARX_LEVEL_DUPLICATE_ZONE_NAME);
    CHECK(level_validation::sceneError(scene::Error::kTooManyPaths) == ARX_LEVEL_TOO_MANY_PATHS);
    CHECK(level_validation::sceneError(scene::Error::kBadPathNodeCount) == ARX_LEVEL_BAD_PATH_NODE_COUNT);
    CHECK(level_validation::sceneError(scene::Error::kBadPathNodePosition) == ARX_LEVEL_BAD_PATH_NODE_POSITION);
  }

  TEST_CASE("Complete validation seeds every fact and both bounds") {
    using namespace pistoris;

    LevelModules modules = validLevelModules();
    LevelValidationState state;
    REQUIRE(validateLevelModules(modules, state) == ARX_OK);

    constexpr std::array kFacts = {
        LevelValidation::kVertices,
        LevelValidation::kTextures,
        LevelValidation::kFaces,
        LevelValidation::kFaceRooms,
        LevelValidation::kCornerColors,
        LevelValidation::kRooms,
        LevelValidation::kPortals,
        LevelValidation::kRoomDistances,
        LevelValidation::kNavSurface,
        LevelValidation::kAnchors,
        LevelValidation::kAnchorConnections,
        LevelValidation::kLightSources,
        LevelValidation::kPlayerSpawn,
        LevelValidation::kEntities,
        LevelValidation::kFogs,
        LevelValidation::kZones,
        LevelValidation::kPaths,
    };
    for (LevelValidation validation : kFacts) CHECK(level_validation::has(state, validation));
    CHECK(state.derived.bounds.has_value());
    CHECK(state.derived.referenced_bounds.has_value());
  }

  TEST_CASE("Invalidation clears dependents before root revalidation") {
    using namespace pistoris;

    LevelModules modules = validLevelModules();
    LevelValidationState state;
    REQUIRE(validateLevelModules(modules, state) == ARX_OK);

    modules.textures.textures.push_back({""});
    modules.geometry.faces[0].texture = 0;
    level_validation::invalidate(state, LevelValidation::kTextures);
    CHECK_FALSE(level_validation::has(state, LevelValidation::kTextures));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaces));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaceRooms));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kCornerColors));
    CHECK(level_validation::has(state, LevelValidation::kAnchors));
    CHECK(level_validation::has(state, LevelValidation::kAnchorConnections));
    CHECK(state.derived.bounds.has_value());
    CHECK_FALSE(state.derived.referenced_bounds.has_value());

    CHECK(level_validation::faces(modules, state) == ARX_LEVEL_BAD_TEXTURE_PATH);
    CHECK_FALSE(level_validation::has(state, LevelValidation::kTextures));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaces));

    modules.textures.textures[0].path = "graph/tex.bmp";
    CHECK(level_validation::faces(modules, state) == ARX_OK);
    CHECK(level_validation::has(state, LevelValidation::kTextures | LevelValidation::kFaces));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaceRooms));
  }

  TEST_CASE("Room invalidation clears only the room dependency chain") {
    using namespace pistoris;

    LevelModules modules = validLevelModules();
    LevelValidationState state;
    REQUIRE(validateLevelModules(modules, state) == ARX_OK);

    level_validation::invalidate(state, LevelValidation::kRooms);
    CHECK_FALSE(level_validation::has(state, LevelValidation::kRooms));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaceRooms));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kPortals));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kRoomDistances));
    CHECK(level_validation::has(state,
                                LevelValidation::kVertices | LevelValidation::kTextures | LevelValidation::kFaces |
                                    LevelValidation::kCornerColors | LevelValidation::kAnchors |
                                    LevelValidation::kAnchorConnections));
    CHECK(state.derived.bounds.has_value());
    CHECK(state.derived.referenced_bounds.has_value());
  }

  TEST_CASE("Portal invalidation clears dependent room distances") {
    using namespace pistoris;

    LevelModules modules = validLevelModules();
    LevelValidationState state;
    REQUIRE(validateLevelModules(modules, state) == ARX_OK);

    level_validation::invalidate(state, LevelValidation::kPortals);
    CHECK_FALSE(level_validation::has(state, LevelValidation::kPortals));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kRoomDistances));
    CHECK(level_validation::has(state, LevelValidation::kRooms | LevelValidation::kFaceRooms));
  }

  TEST_CASE("Marking a root valid does not restore invalidated dependents") {
    using namespace pistoris;

    LevelModules modules = validLevelModules();
    LevelValidationState state;
    REQUIRE(validateLevelModules(modules, state) == ARX_OK);

    level_validation::invalidate(state, LevelValidation::kVertices);
    level_validation::markValid(state, LevelValidation::kVertices);
    CHECK(level_validation::has(state, LevelValidation::kVertices));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaces));
    CHECK(level_validation::has(state, LevelValidation::kAnchors));
    CHECK(level_validation::has(state, LevelValidation::kAnchorConnections));
    CHECK_FALSE(state.derived.bounds.has_value());
    CHECK_FALSE(state.derived.referenced_bounds.has_value());
  }

  TEST_CASE("Failed validation never caches the failed fact") {
    using namespace pistoris;

    LevelModules modules = validLevelModules();
    modules.geometry.vertices[2].position = {2.0f, 0.0f, 0.0f};
    LevelValidationState state;

    CHECK(level_validation::faces(modules, state) == ARX_LEVEL_DEGENERATE_FACE);
    CHECK(level_validation::has(state, LevelValidation::kVertices));
    CHECK(level_validation::has(state, LevelValidation::kTextures));
    CHECK_FALSE(level_validation::has(state, LevelValidation::kFaces));
    CHECK_FALSE(state.derived.referenced_bounds.has_value());
  }
}
