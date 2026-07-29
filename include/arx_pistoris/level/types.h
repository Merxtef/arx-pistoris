// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_LEVEL_TYPES_H
#define ARX_PISTORIS_LEVEL_TYPES_H

#include "arx_pistoris/api.h"
#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/image.h"
#include "arx_pistoris/indices.h"

#include <stdint.h>

// Public C-compatible Level value types
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

#ifdef __cplusplus
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value) = value
#else
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value)
#endif

typedef uint32_t ArxPortalShape;
enum { ARX_PORTAL_TRIANGLE = 3, ARX_PORTAL_QUAD = 4 };

typedef uint32_t ArxZoneHeightMode;
enum { ARX_ZONE_HEIGHT_FINITE = 0, ARX_ZONE_HEIGHT_INFINITE = 1 };

typedef uint32_t ArxPathNodeType;
enum { ARX_PATH_NODE_STANDARD = 0, ARX_PATH_NODE_BEZIER = 1, ARX_PATH_NODE_CONTROL_POINT = 2 };

enum { ARX_ANCHOR_FLAG_BLOCKED = 1U << 3, ARX_LEVEL_FACE_BITS_ALL = ARX_FACE_BITS_ALL & ~ARX_FACE_BIT_QUAD };

typedef struct ArxLevelVertex {
  ArxVector3 position;
} ArxLevelVertex;

typedef struct ArxLevelCorner {
  ArxVertexIndex vertex;
  ArxVector3 normal;
  float u;
  float v;
  ArxColor3 color;
} ArxLevelCorner;

typedef struct ArxLevelFace {
  ArxLevelCorner corners[3];
  ArxTextureIndex texture ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_TEXTURE);
  ArxRoomIndex room;
  ArxFaceType flags;
  float transval;
  uint8_t has_corner_colors;
} ArxLevelFace;

typedef struct ArxLevelRoomDistance {
  ArxRoomIndex room_a;
  ArxRoomIndex room_b;
  float distance;
  ArxPortalIndex portal_a ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
  ArxPortalIndex portal_b ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
} ArxLevelRoomDistance;

typedef struct ArxLevelAnchorConnection {
  ArxAnchorIndex first;
  ArxAnchorIndex second;
} ArxLevelAnchorConnection;

typedef struct ArxLevelNavSurfaceTriangle {
  ArxNavSurfaceVertexIndex vertices[3];
} ArxLevelNavSurfaceTriangle;

typedef struct ArxLevelPlayerSpawn {
  ArxVector3 position;
  ArxQuat rotation;
  uint8_t is_usable;
} ArxLevelPlayerSpawn;

typedef struct ArxLevelTextureView {
  ArxStringView path;
  ArxEncodedImageView encoded_image;
} ArxLevelTextureView;

typedef struct ArxLevelRoom {
  ArxStringView name;
} ArxLevelRoom;

typedef struct ArxLevelPortal {
  ArxStringView name;
  ArxRoomIndex room_1;
  ArxRoomIndex room_2;
  ArxPortalShape shape ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_PORTAL_QUAD);
  /* The fourth vertex is ignored when shape is ARX_PORTAL_TRIANGLE */
  ArxVector3 vertices[4];
} ArxLevelPortal;

typedef struct ArxLevelAnchor {
  ArxVector3 position;
  float radius ARX_PISTORIS_DETAIL_CXX_DEFAULT(50.0f);
  float height ARX_PISTORIS_DETAIL_CXX_DEFAULT(-165.0f);
  int16_t flags;
  ArxStringView name;
} ArxLevelAnchor;

typedef struct ArxLevelNavSurfaceInfo {
  uint8_t has_surface;
  size_t vertex_count;
  size_t triangle_count;
} ArxLevelNavSurfaceInfo;

typedef struct ArxLevelNavSurfaceInput {
  const ArxLevelVertex* vertices;
  size_t vertex_count;
  const ArxLevelNavSurfaceTriangle* triangles;
  size_t triangle_count;
} ArxLevelNavSurfaceInput;

typedef struct ArxLevelLight {
  ArxStringView name;
  ArxVector3 position;
  ArxColor3 color;
  float fallstart;
  float fallend ARX_PISTORIS_DETAIL_CXX_DEFAULT(1.0f);
  float intensity;
  ArxColor3 flicker;
  float effect_radius;
  float effect_frequency;
  float effect_size;
  float effect_speed;
  float flare_size;
  uint32_t flags;
} ArxLevelLight;

typedef struct ArxLevelEntity {
  ArxStringView class_path;
  int32_t ident ARX_PISTORIS_DETAIL_CXX_DEFAULT(-1);
  ArxVector3 position;
  ArxQuat rotation;
  ArxStringView name;
} ArxLevelEntity;

typedef struct ArxLevelFog {
  ArxVector3 position;
  ArxColor3 color;
  float size;
  uint8_t directional;
  float scale;
  ArxQuat rotation;
  float speed;
  float rotate_speed;
  int32_t lifetime_ms;
  float frequency;
  ArxStringView name;
} ArxLevelFog;

typedef struct ArxLevelZoneAmbiance {
  ArxStringView name;
  float volume ARX_PISTORIS_DETAIL_CXX_DEFAULT(100.0f);
} ArxLevelZoneAmbiance;

typedef struct ArxLevelZone {
  ArxStringView name;
  size_t perimeter_count;
  float reference_y;
  ArxZoneHeightMode height_mode;
  float height;
  uint8_t has_color;
  ArxColor3 color;
  uint8_t has_farclip;
  float farclip;
  uint8_t has_ambiance;
  ArxLevelZoneAmbiance ambiance;
} ArxLevelZone;

typedef struct ArxLevelZoneInput {
  ArxLevelZone value;
  const ArxVector2* perimeter_xz;
} ArxLevelZoneInput;

typedef struct ArxLevelPathNode {
  ArxVector3 relative_position;
  ArxPathNodeType type;
  uint32_t time_ms;
} ArxLevelPathNode;

typedef struct ArxLevelPath {
  ArxStringView name;
  ArxVector3 position;
  size_t node_count;
} ArxLevelPath;

typedef struct ArxLevelPathInput {
  ArxStringView name;
  ArxVector3 position;
  const ArxLevelPathNode* nodes;
  size_t node_count;
} ArxLevelPathInput;

typedef struct ArxLevelMeshInput {
  const ArxLevelVertex* vertices;
  size_t vertex_count;
  const ArxLevelFace* faces;
  size_t face_count;
  const ArxLevelTextureView* textures;
  size_t texture_count;
} ArxLevelMeshInput;

typedef struct ArxLevelAnchorsInput {
  const ArxLevelAnchor* anchors;
  size_t anchor_count;
  const ArxLevelAnchorConnection* connections;
  size_t connection_count;
} ArxLevelAnchorsInput;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_LEVEL_TYPES_H */
