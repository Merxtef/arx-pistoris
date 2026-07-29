// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "level/validation.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "level/anchor_bounds.h"
#include "level/data.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"

#include <cstddef>
#include <span>

namespace pistoris::level_validation {
namespace {

bool hasAny(LevelValidation value, LevelValidation bits) noexcept { return (value & bits) != LevelValidation::kNone; }

bool validLevelBounds(const ArxAabb& bounds) noexcept {
  return bounds.min.x >= kLevelMinXZ && bounds.max.x <= kLevelMaxXZ && bounds.min.z >= kLevelMinXZ &&
         bounds.max.z <= kLevelMaxXZ;
}

LevelValidation invalidationClosure(LevelValidation invalid) noexcept {
  if (hasAny(invalid, LevelValidation::kVertices | LevelValidation::kTextures)) invalid |= LevelValidation::kFaces;
  if (hasAny(invalid, LevelValidation::kFaces)) {
    invalid |= LevelValidation::kFaceRooms | LevelValidation::kCornerColors;
  }
  if (hasAny(invalid, LevelValidation::kRooms)) {
    invalid |= LevelValidation::kFaceRooms | LevelValidation::kPortals | LevelValidation::kRoomDistances;
  }
  if (hasAny(invalid, LevelValidation::kPortals)) invalid |= LevelValidation::kRoomDistances;
  if (hasAny(invalid, LevelValidation::kAnchors)) invalid |= LevelValidation::kAnchorConnections;
  return invalid;
}

ArxReturnCode recordValidationResult(LevelValidationState& state, LevelValidation validation,
                                     ArxReturnCode result) noexcept {
  if (result == ARX_OK) {
    markValid(state, validation);
  } else {
    invalidate(state, validation);
  }
  return result;
}

}  // namespace

bool validPortalBounds(const Portal& portal) noexcept {
  const std::size_t count = rooms::portalVertexCount(portal.shape);
  for (std::size_t i = 0; i < count; ++i) {
    const ArxVector3& position = portal.vertices[i];
    if (position.x < kLevelMinXZ || position.x > kLevelMaxXZ || position.z < kLevelMinXZ || position.z > kLevelMaxXZ)
      return false;
  }
  return true;
}

ArxReturnCode geometryError(geometry::Error error) noexcept {
  switch (error) {
    case geometry::Error::kNone:
      return ARX_OK;
    case geometry::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case geometry::Error::kNoGeometry:
      return ARX_LEVEL_NO_GEOMETRY;
    case geometry::Error::kTooManyVertices:
      return ARX_LEVEL_TOO_MANY_VERTICES;
    case geometry::Error::kBadVertex:
      return ARX_LEVEL_BAD_VERTEX_POSITION;
    case geometry::Error::kTooManyFaces:
      return ARX_LEVEL_TOO_MANY_FACES;
    case geometry::Error::kBadFaceVertex:
      return ARX_LEVEL_BAD_FACE_VERTEX;
    case geometry::Error::kTooManyTextures:
      return ARX_LEVEL_TOO_MANY_TEXTURES;
    case geometry::Error::kBadTexture:
      return ARX_LEVEL_BAD_TEXTURE_PATH;
    case geometry::Error::kBadTextureImage:
      return ARX_LEVEL_BAD_TEXTURE_IMAGE;
    case geometry::Error::kOutOfMemory:
      return ARX_BAD_ALLOC;
    case geometry::Error::kBadFaceTexture:
      return ARX_LEVEL_BAD_FACE_TEXTURE;
    case geometry::Error::kBadFaceType:
      return ARX_LEVEL_BAD_FACE_TYPE;
    case geometry::Error::kBadFaceTransval:
      return ARX_LEVEL_BAD_FACE_TRANSVAL;
    case geometry::Error::kBadFaceNormal:
      return ARX_LEVEL_BAD_FACE_NORMAL;
    case geometry::Error::kBadFaceUv:
      return ARX_LEVEL_BAD_FACE_UV;
    case geometry::Error::kDegenerateFace:
      return ARX_LEVEL_DEGENERATE_FACE;
    case geometry::Error::kBadVertexWeldSegment:
      return ARX_LEVEL_BAD_VERTEX_WELD_SEGMENT;
    case geometry::Error::kOverlappingVertexWeldSegments:
      return ARX_LEVEL_OVERLAPPING_VERTEX_WELD_SEGMENTS;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode imageError(geometry::ImageError error) noexcept {
  switch (error) {
    case geometry::ImageError::kNone:
      return ARX_OK;
    case geometry::ImageError::kMalformed:
      return ARX_LEVEL_BAD_TEXTURE_IMAGE;
    case geometry::ImageError::kOutOfMemory:
      return ARX_BAD_ALLOC;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode roomsError(rooms::Error error) noexcept {
  switch (error) {
    case rooms::Error::kNone:
      return ARX_OK;
    case rooms::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case rooms::Error::kNoRooms:
      return ARX_LEVEL_NO_ROOMS;
    case rooms::Error::kTooManyRooms:
      return ARX_LEVEL_TOO_MANY_ROOMS;
    case rooms::Error::kBadRoomName:
      return ARX_LEVEL_BAD_ROOM_NAME;
    case rooms::Error::kDuplicateRoomName:
      return ARX_LEVEL_DUPLICATE_ROOM_NAME;
    case rooms::Error::kBadFaceRoomCount:
      return ARX_LEVEL_BAD_FACE_ROOM_COUNT;
    case rooms::Error::kBadFaceRoomIndex:
      return ARX_LEVEL_BAD_FACE_ROOM_INDEX;
    case rooms::Error::kBadRoomDistanceCount:
      return ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT;
    case rooms::Error::kBadRoomDistance:
      return ARX_LEVEL_BAD_ROOM_DISTANCE;
    case rooms::Error::kTooManyPortals:
      return ARX_LEVEL_TOO_MANY_PORTALS;
    case rooms::Error::kBadPortalName:
      return ARX_LEVEL_BAD_PORTAL_NAME;
    case rooms::Error::kDuplicatePortalName:
      return ARX_LEVEL_DUPLICATE_PORTAL_NAME;
    case rooms::Error::kBadPortalRoom:
      return ARX_LEVEL_BAD_PORTAL_ROOM;
    case rooms::Error::kBadPortalShape:
      return ARX_LEVEL_BAD_PORTAL_SHAPE;
    case rooms::Error::kBadPortalVertex:
      return ARX_LEVEL_BAD_PORTAL_VERTEX;
    case rooms::Error::kDegeneratePortal:
      return ARX_LEVEL_DEGENERATE_PORTAL;
    case rooms::Error::kNonPlanarPortal:
      return ARX_LEVEL_NON_PLANAR_PORTAL;
    case rooms::Error::kInconsistentPortalOrientation:
      return ARX_LEVEL_INCONSISTENT_PORTAL_ORIENTATION;
    case rooms::Error::kSelfIntersectingPortal:
      return ARX_LEVEL_SELF_INTERSECTING_PORTAL;
    case rooms::Error::kBadFaceVertex:
      return ARX_LEVEL_BAD_FACE_VERTEX;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode navigationError(navigation::Error error) noexcept {
  switch (error) {
    case navigation::Error::kNone:
      return ARX_OK;
    case navigation::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case navigation::Error::kBadFaceVertex:
      return ARX_LEVEL_BAD_FACE_VERTEX;
    case navigation::Error::kBadSurface:
      return ARX_LEVEL_BAD_NAV_SURFACE;
    case navigation::Error::kTooManySurfaceVertices:
      return ARX_LEVEL_TOO_MANY_NAV_SURFACE_VERTICES;
    case navigation::Error::kBadSurfaceVertex:
      return ARX_LEVEL_BAD_NAV_SURFACE_VERTEX;
    case navigation::Error::kBadSurfaceTriangle:
      return ARX_LEVEL_BAD_NAV_SURFACE_TRIANGLE;
    case navigation::Error::kDegenerateSurfaceTriangle:
      return ARX_LEVEL_DEGENERATE_NAV_SURFACE_TRIANGLE;
    case navigation::Error::kSurfaceRequired:
      return ARX_LEVEL_NAV_SURFACE_REQUIRED;
    case navigation::Error::kEmptyResult:
      return ARX_LEVEL_EMPTY_NAVIGATION_RESULT;
    case navigation::Error::kTooManyAnchors:
      return ARX_LEVEL_TOO_MANY_ANCHORS;
    case navigation::Error::kBadAnchorName:
      return ARX_LEVEL_BAD_ANCHOR_NAME;
    case navigation::Error::kDuplicateAnchorName:
      return ARX_LEVEL_DUPLICATE_ANCHOR_NAME;
    case navigation::Error::kBadAnchorPosition:
      return ARX_LEVEL_BAD_ANCHOR_POSITION;
    case navigation::Error::kBadAnchorRadius:
      return ARX_LEVEL_BAD_ANCHOR_RADIUS;
    case navigation::Error::kBadAnchorHeight:
      return ARX_LEVEL_BAD_ANCHOR_HEIGHT;
    case navigation::Error::kBadAnchorFlags:
      return ARX_LEVEL_BAD_ANCHOR_FLAGS;
    case navigation::Error::kTooManyConnections:
      return ARX_LEVEL_TOO_MANY_ANCHOR_CONNECTIONS;
    case navigation::Error::kBadConnectionIndex:
      return ARX_LEVEL_BAD_ANCHOR_CONNECTION_INDEX;
    case navigation::Error::kBadConnectionOrder:
      return ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode lightingError(lights::Error error) noexcept {
  switch (error) {
    case lights::Error::kNone:
      return ARX_OK;
    case lights::Error::kInvalidOptions:
      return ARX_INVALID_OPTIONS;
    case lights::Error::kTooManyLights:
      return ARX_LEVEL_TOO_MANY_LIGHTS;
    case lights::Error::kBadLightName:
      return ARX_LEVEL_BAD_LIGHT_NAME;
    case lights::Error::kDuplicateLightName:
      return ARX_LEVEL_DUPLICATE_LIGHT_NAME;
    case lights::Error::kBadLightPosition:
      return ARX_LEVEL_BAD_LIGHT_POSITION;
    case lights::Error::kBadLightColor:
      return ARX_LEVEL_BAD_LIGHT_COLOR;
    case lights::Error::kBadLightFalloff:
      return ARX_LEVEL_BAD_LIGHT_FALLOFF;
    case lights::Error::kBadLightIntensity:
      return ARX_LEVEL_BAD_LIGHT_INTENSITY;
    case lights::Error::kBadLightEffect:
      return ARX_LEVEL_BAD_LIGHT_EFFECT;
    case lights::Error::kBadLightFlags:
      return ARX_LEVEL_BAD_LIGHT_FLAGS;
    case lights::Error::kBadCornerColorCount:
      return ARX_LEVEL_BAD_CORNER_COLOR_COUNT;
    case lights::Error::kBadCornerColor:
      return ARX_LEVEL_BAD_CORNER_COLOR;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode sceneError(scene::Error error) noexcept {
  switch (error) {
    case scene::Error::kNone:
      return ARX_OK;
    case scene::Error::kBadPlayerSpawn:
      return ARX_LEVEL_BAD_PLAYER_SPAWN;
    case scene::Error::kTooManyEntities:
      return ARX_LEVEL_TOO_MANY_ENTITIES;
    case scene::Error::kBadEntityName:
      return ARX_LEVEL_BAD_ENTITY_NAME;
    case scene::Error::kDuplicateEntityName:
      return ARX_LEVEL_DUPLICATE_ENTITY_NAME;
    case scene::Error::kBadEntityClassPath:
      return ARX_LEVEL_BAD_ENTITY_CLASS_PATH;
    case scene::Error::kBadEntityPosition:
      return ARX_LEVEL_BAD_ENTITY_POSITION;
    case scene::Error::kBadEntityRotation:
      return ARX_LEVEL_BAD_ENTITY_ROTATION;
    case scene::Error::kTooManyFogs:
      return ARX_LEVEL_TOO_MANY_FOGS;
    case scene::Error::kBadFogName:
      return ARX_LEVEL_BAD_FOG_NAME;
    case scene::Error::kDuplicateFogName:
      return ARX_LEVEL_DUPLICATE_FOG_NAME;
    case scene::Error::kBadFogPosition:
      return ARX_LEVEL_BAD_FOG_POSITION;
    case scene::Error::kBadFogRotation:
      return ARX_LEVEL_BAD_FOG_ROTATION;
    case scene::Error::kBadFogColor:
      return ARX_LEVEL_BAD_FOG_COLOR;
    case scene::Error::kBadFogEffect:
      return ARX_LEVEL_BAD_FOG_EFFECT;
    case scene::Error::kTooManyZones:
      return ARX_LEVEL_TOO_MANY_ZONES;
    case scene::Error::kBadZoneName:
      return ARX_LEVEL_BAD_ZONE_NAME;
    case scene::Error::kDuplicateZoneName:
      return ARX_LEVEL_DUPLICATE_ZONE_NAME;
    case scene::Error::kBadZonePerimeter:
      return ARX_LEVEL_BAD_ZONE_PERIMETER;
    case scene::Error::kBadZoneHeightMode:
      return ARX_LEVEL_BAD_ZONE_HEIGHT_MODE;
    case scene::Error::kBadZoneHeight:
      return ARX_LEVEL_BAD_ZONE_HEIGHT;
    case scene::Error::kBadZoneColor:
      return ARX_LEVEL_BAD_ZONE_COLOR;
    case scene::Error::kBadZoneFarclip:
      return ARX_LEVEL_BAD_ZONE_FARCLIP;
    case scene::Error::kBadZoneAmbiance:
      return ARX_LEVEL_BAD_ZONE_AMBIANCE;
    case scene::Error::kTooManyPaths:
      return ARX_LEVEL_TOO_MANY_PATHS;
    case scene::Error::kBadPathName:
      return ARX_LEVEL_BAD_PATH_NAME;
    case scene::Error::kDuplicatePathName:
      return ARX_LEVEL_DUPLICATE_PATH_NAME;
    case scene::Error::kBadPathPosition:
      return ARX_LEVEL_BAD_PATH_POSITION;
    case scene::Error::kBadPathNodeCount:
      return ARX_LEVEL_BAD_PATH_NODE_COUNT;
    case scene::Error::kBadPathNodePosition:
      return ARX_LEVEL_BAD_PATH_NODE_POSITION;
    case scene::Error::kBadPathNodeType:
      return ARX_LEVEL_BAD_PATH_NODE_TYPE;
    case scene::Error::kBadPathFirstNode:
      return ARX_LEVEL_BAD_PATH_FIRST_NODE;
  }
  return ARX_INTERNAL_ERROR;
}

ArxReturnCode faceTypes(std::span<const Face> faces) noexcept {
  for (const Face& face : faces)
    if ((face.flags & kLevelFaceBitsAll) != face.flags) return ARX_LEVEL_BAD_FACE_TYPE;
  return ARX_OK;
}

void invalidate(LevelValidationState& state, LevelValidation validation) noexcept {
  const LevelValidation invalid = invalidationClosure(validation);
  state.valid &= ~invalid;
  if (hasAny(invalid, LevelValidation::kVertices)) state.derived.bounds.reset();
  if (hasAny(invalid, LevelValidation::kFaces)) state.derived.referenced_bounds.reset();
}

ArxReturnCode vertices(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kVertices) && state.derived.bounds) return ARX_OK;

  ArxAabb bounds;
  ArxReturnCode rc = geometryError(geometry::validateVertices(modules.geometry.vertices, &bounds));
  if (rc != ARX_OK) return recordValidationResult(state, LevelValidation::kVertices, rc);
  if (!validLevelBounds(bounds))
    return recordValidationResult(state, LevelValidation::kVertices, ARX_LEVEL_VERTEX_OUT_OF_BOUNDS);
  state.derived.bounds = bounds;
  return recordValidationResult(state, LevelValidation::kVertices, ARX_OK);
}

ArxReturnCode textures(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kTextures)) return ARX_OK;
  return recordValidationResult(
      state, LevelValidation::kTextures, geometryError(geometry::validateTextures(modules.geometry.textures)));
}

ArxReturnCode faces(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = vertices(modules, state);
  if (rc != ARX_OK) return rc;
  rc = textures(modules, state);
  if (rc != ARX_OK) return rc;
  if (has(state, LevelValidation::kFaces) && state.derived.referenced_bounds) return ARX_OK;

  ArxAabb referenced_bounds;
  rc = geometryError(geometry::validateFaces(
      modules.geometry.faces, modules.geometry.vertices, modules.geometry.textures.size(), &referenced_bounds));
  if (rc != ARX_OK) return recordValidationResult(state, LevelValidation::kFaces, rc);
  rc = faceTypes(modules.geometry.faces);
  if (rc != ARX_OK) return recordValidationResult(state, LevelValidation::kFaces, rc);
  state.derived.referenced_bounds = referenced_bounds;
  return recordValidationResult(state, LevelValidation::kFaces, ARX_OK);
}

ArxReturnCode rooms(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kRooms)) return ARX_OK;
  ArxReturnCode rc = roomsError(pistoris::rooms::validateRooms(modules.rooms));
  if (rc == ARX_OK && modules.rooms.definitions.size() > kMaxRooms) rc = ARX_LEVEL_TOO_MANY_ROOMS;
  return recordValidationResult(state, LevelValidation::kRooms, rc);
}

ArxReturnCode faceRooms(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = faces(modules, state);
  if (rc != ARX_OK) return rc;
  rc = rooms(modules, state);
  if (rc != ARX_OK) return rc;
  if (has(state, LevelValidation::kFaceRooms)) return ARX_OK;
  rc = roomsError(pistoris::rooms::validateFaceRooms(
      modules.rooms.face_rooms, modules.geometry.faces.size(), modules.rooms.definitions.size()));
  return recordValidationResult(state, LevelValidation::kFaceRooms, rc);
}

ArxReturnCode cornerColors(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = faces(modules, state);
  if (rc != ARX_OK) return rc;
  if (has(state, LevelValidation::kCornerColors)) return ARX_OK;
  rc = lightingError(lights::validateCornerColors(modules.lighting.corner_colors, modules.geometry.faces.size()));
  return recordValidationResult(state, LevelValidation::kCornerColors, rc);
}

ArxReturnCode portals(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = rooms(modules, state);
  if (rc != ARX_OK) return rc;
  if (has(state, LevelValidation::kPortals)) return ARX_OK;
  rc = roomsError(pistoris::rooms::validatePortals(modules.rooms));
  if (rc == ARX_OK) {
    for (const Portal& portal : modules.rooms.portals) {
      if (!validPortalBounds(portal)) {
        rc = ARX_LEVEL_PORTAL_OUT_OF_BOUNDS;
        break;
      }
    }
  }
  return recordValidationResult(state, LevelValidation::kPortals, rc);
}

ArxReturnCode roomDistances(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = portals(modules, state);
  if (rc != ARX_OK) return rc;
  if (has(state, LevelValidation::kRoomDistances)) return ARX_OK;
  rc = roomsError(pistoris::rooms::validateRoomDistances(modules.rooms));
  return recordValidationResult(state, LevelValidation::kRoomDistances, rc);
}

ArxReturnCode navSurface(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kNavSurface)) return ARX_OK;
  return recordValidationResult(
      state, LevelValidation::kNavSurface, navigationError(navigation::validateSurface(modules.navigation.surface)));
}

ArxReturnCode anchors(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kAnchors)) return ARX_OK;
  ArxReturnCode rc = navigationError(navigation::validateAnchorDefinitions(modules.navigation.anchors));
  if (rc == ARX_OK) {
    for (const Anchor& anchor : modules.navigation.anchors) {
      if (!level_anchor_bounds::insideNativeMap(anchor.position)) {
        rc = ARX_LEVEL_ANCHOR_OUT_OF_BOUNDS;
        break;
      }
    }
  }
  return recordValidationResult(state, LevelValidation::kAnchors, rc);
}

ArxReturnCode anchorConnections(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = anchors(modules, state);
  if (rc != ARX_OK) return rc;
  if (has(state, LevelValidation::kAnchorConnections)) return ARX_OK;
  rc = navigationError(navigation::validateConnections(modules.navigation.anchors, modules.navigation.connections));
  return recordValidationResult(state, LevelValidation::kAnchorConnections, rc);
}

ArxReturnCode lightSources(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kLightSources)) return ARX_OK;
  return recordValidationResult(
      state, LevelValidation::kLightSources, lightingError(lights::validateLightSources(modules.lighting.lights)));
}

ArxReturnCode playerSpawn(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kPlayerSpawn)) return ARX_OK;
  return recordValidationResult(
      state, LevelValidation::kPlayerSpawn, sceneError(scene::validatePlayerSpawn(modules.scene)));
}

ArxReturnCode entities(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kEntities)) return ARX_OK;
  return recordValidationResult(
      state, LevelValidation::kEntities, sceneError(scene::validateEntities(modules.scene.entities)));
}

ArxReturnCode fogs(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kFogs)) return ARX_OK;
  return recordValidationResult(state, LevelValidation::kFogs, sceneError(scene::validateFogs(modules.scene.fogs)));
}

ArxReturnCode zones(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kZones)) return ARX_OK;
  return recordValidationResult(state, LevelValidation::kZones, sceneError(scene::validateZones(modules.scene.zones)));
}

ArxReturnCode paths(const LevelModules& modules, LevelValidationState& state) {
  if (has(state, LevelValidation::kPaths)) return ARX_OK;
  return recordValidationResult(state, LevelValidation::kPaths, sceneError(scene::validatePaths(modules.scene.paths)));
}

ArxReturnCode mesh(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = faces(modules, state);
  if (rc != ARX_OK) return rc;
  rc = faceRooms(modules, state);
  if (rc != ARX_OK) return rc;
  return cornerColors(modules, state);
}

ArxReturnCode all(const LevelModules& modules, LevelValidationState& state) {
  ArxReturnCode rc = vertices(modules, state);
  if (rc != ARX_OK) return rc;
  rc = textures(modules, state);
  if (rc != ARX_OK) return rc;
  rc = faces(modules, state);
  if (rc != ARX_OK) return rc;
  rc = rooms(modules, state);
  if (rc != ARX_OK) return rc;
  rc = faceRooms(modules, state);
  if (rc != ARX_OK) return rc;
  rc = roomDistances(modules, state);
  if (rc != ARX_OK) return rc;
  rc = portals(modules, state);
  if (rc != ARX_OK) return rc;
  rc = anchors(modules, state);
  if (rc != ARX_OK) return rc;
  rc = anchorConnections(modules, state);
  if (rc != ARX_OK) return rc;
  rc = navSurface(modules, state);
  if (rc != ARX_OK) return rc;
  rc = lightSources(modules, state);
  if (rc != ARX_OK) return rc;
  rc = cornerColors(modules, state);
  if (rc != ARX_OK) return rc;
  rc = playerSpawn(modules, state);
  if (rc != ARX_OK) return rc;
  rc = entities(modules, state);
  if (rc != ARX_OK) return rc;
  rc = fogs(modules, state);
  if (rc != ARX_OK) return rc;
  rc = zones(modules, state);
  if (rc != ARX_OK) return rc;
  return paths(modules, state);
}

}  // namespace pistoris::level_validation
