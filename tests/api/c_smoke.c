// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_pistoris.h"

#include <stddef.h>
#include <string.h>

_Static_assert(sizeof(ArxVector2) == 8, "ArxVector2 ABI mismatch");
_Static_assert(sizeof(ArxVector3) == 12, "ArxVector3 ABI mismatch");
_Static_assert(sizeof(ArxResourceKind) == 1, "ArxResourceKind ABI mismatch");
_Static_assert(sizeof(ArxLevelCorner) == 36, "ArxLevelCorner ABI mismatch");
_Static_assert(sizeof(ArxLevelFace) == 128, "ArxLevelFace ABI mismatch");
_Static_assert(offsetof(ArxLevelFace, texture) == 108, "ArxLevelFace ABI mismatch");
_Static_assert(sizeof(ArxLevelNavSurfaceTriangle) == 12, "ArxLevelNavSurfaceTriangle ABI mismatch");
_Static_assert((ARX_LEVEL_FACE_BITS_ALL & ARX_FACE_BIT_QUAD) == 0, "Level face mask includes QUAD");
_Static_assert((ARX_MODEL_FACE_BITS_ALL & ARX_FACE_BIT_QUAD) == 0, "Model face mask includes QUAD");

#define ARX_ASSERT_INITIALIZER(type, initializer) \
  _Static_assert(sizeof((type[]){initializer}) == sizeof(type), #type " initializer mismatch")

ARX_ASSERT_INITIALIZER(ArxQuat, ARX_QUAT_IDENTITY_INIT);
ARX_ASSERT_INITIALIZER(ArxNativeTextureBakeOptions, ARX_NATIVE_TEXTURE_BAKE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxNativeSoundBakeOptions, ARX_NATIVE_SOUND_BAKE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxDlfWriteOptions, ARX_DLF_WRITE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLlfWriteOptions, ARX_LLF_WRITE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxAmbianceGlbImportOptions, ARX_AMBIANCE_GLB_IMPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxAmbianceGlbExportOptions, ARX_AMBIANCE_GLB_EXPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelVertexWeldOptions, ARX_LEVEL_VERTEX_WELD_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelNavSurfaceSourceOptions, ARX_LEVEL_NAV_SURFACE_SOURCE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelNavSurfaceGenOptions, ARX_LEVEL_NAV_SURFACE_GEN_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelNavSurfacePruneOptions, ARX_LEVEL_NAV_SURFACE_PRUNE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelAnchorGenOptions, ARX_LEVEL_ANCHOR_GEN_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelAnchorPruneOptions, ARX_LEVEL_ANCHOR_PRUNE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelAnchorConnectionGenOptions, ARX_LEVEL_ANCHOR_CONNECTION_GEN_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelRoomDistanceGenOptions, ARX_LEVEL_ROOM_DISTANCE_GEN_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelStaticLightingGenOptions, ARX_LEVEL_STATIC_LIGHTING_GEN_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelGlbImportOptions, ARX_LEVEL_GLB_IMPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelGlbExportOptions, ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxModelGlbImportOptions, ARX_MODEL_GLB_IMPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxModelGlbExportOptions, ARX_MODEL_GLB_EXPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxModelLevelPreviewGlbOptions, ARX_MODEL_LEVEL_PREVIEW_GLB_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxObjExportOptions, ARX_OBJ_EXPORT_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelNativeBakeOptions, ARX_LEVEL_NATIVE_BAKE_OPTIONS_INIT);
ARX_ASSERT_INITIALIZER(ArxLevelDlfBakeOptions, ARX_LEVEL_DLF_BAKE_OPTIONS_INIT);

#undef ARX_ASSERT_INITIALIZER

int main(void) {
  const ArxQuat identity = ARX_QUAT_IDENTITY_INIT;
  if (identity.w != 1.0f || identity.x != 0.0f || identity.y != 0.0f || identity.z != 0.0f) return 100;

  ArxLevel* level = NULL;
  if (arx_pistoris_level_create(&level) != ARX_OK || level == NULL) return 1;

  size_t vertex_count = 1;
  if (arx_pistoris_level_vertex_count(level, &vertex_count) != ARX_OK) return 2;
  if (vertex_count != 0 || arx_pistoris_level_copy_vertices(level, 0, 0, NULL) != ARX_OK) return 3;

  {
    const char first_name[] = "room";
    ArxLevelRoom room = {{first_name, sizeof(first_name) - 1}};
    ArxRoomIndex index = ARX_INVALID_INDEX;
    ArxLevelRoom returned = {{NULL, 0}};
    if (arx_pistoris_level_add_room(level, &room, &index) != ARX_OK || index != 0) return 4;
    if (arx_pistoris_level_copy_rooms(level, index, 1, &returned) != ARX_OK) return 5;
    if (returned.name.size != sizeof(first_name) - 1 ||
        memcmp(returned.name.data, first_name, sizeof(first_name) - 1) != 0)
      return 6;

    const char second_name[] = "second";
    room.name.data = second_name;
    room.name.size = sizeof(second_name) - 1;
    if (arx_pistoris_level_add_room(level, &room, &index) != ARX_OK || index != 1) return 7;
  }

  {
    const ArxLevelVertex vertices[] = {{{0.0f, 0.0f, 0.0f}}, {{1.0f, 0.0f, 0.0f}}, {{0.0f, 0.0f, 1.0f}}};
    for (ArxVertexIndex expected = 0; expected < 3; ++expected) {
      ArxVertexIndex index = ARX_INVALID_INDEX;
      if (arx_pistoris_level_add_vertex(level, vertices[expected], &index) != ARX_OK || index != expected) return 8;
    }

    ArxLevelFace face = {0};
    for (ArxVertexIndex corner = 0; corner < 3; ++corner) {
      face.corners[corner].vertex = corner;
      face.corners[corner].normal.y = -1.0f;
    }
    face.texture = ARX_NO_TEXTURE;
    face.room = 0;
    face.flags = ARX_FACE_BIT_DOUBLESIDED;

    ArxFaceIndex face_index = ARX_INVALID_INDEX;
    if (arx_pistoris_level_add_face(level, &face, &face_index) != ARX_OK || face_index != 0) return 9;

    ArxLevelFace returned = {0};
    size_t face_count = 0;
    if (arx_pistoris_level_face_count(level, &face_count) != ARX_OK ||
        arx_pistoris_level_copy_faces(level, 0, 1, &returned) != ARX_OK)
      return 10;
    if (face_count != 1 || returned.flags != ARX_FACE_BIT_DOUBLESIDED || returned.room != 0) return 11;
  }

  {
    const char name[] = "portal";
    ArxLevelPortal portal = {0};
    portal.name.data = name;
    portal.name.size = sizeof(name) - 1;
    portal.room_1 = 0;
    portal.room_2 = 1;
    portal.shape = ARX_PORTAL_TRIANGLE;
    portal.vertices[0] = (ArxVector3){0.0f, 0.0f, 0.0f};
    portal.vertices[1] = (ArxVector3){1.0f, 0.0f, 0.0f};
    portal.vertices[2] = (ArxVector3){0.0f, 1.0f, 0.0f};
    portal.vertices[3] = (ArxVector3){123.0f, 456.0f, 789.0f};

    ArxPortalIndex index = ARX_INVALID_INDEX;
    if (arx_pistoris_level_add_portal(level, &portal, &index) != ARX_OK || index != 0) return 12;

    ArxLevelPortal returned = {0};
    if (arx_pistoris_level_copy_portals(level, index, 1, &returned) != ARX_OK) return 13;
    if (returned.shape != ARX_PORTAL_TRIANGLE || returned.vertices[3].x != 0.0f || returned.vertices[3].y != 0.0f ||
        returned.vertices[3].z != 0.0f)
      return 14;
  }

  {
    ArxLevelPlayerSpawn spawn = {0};
    spawn.position = (ArxVector3){0.25f, 0.0f, 0.25f};
    spawn.rotation.w = 1.0f;
    spawn.is_usable = 1;
    if (arx_pistoris_level_set_player_spawn(level, &spawn) != ARX_OK) return 15;
  }

  arx_pistoris_level_destroy(level);
  return 0;
}
